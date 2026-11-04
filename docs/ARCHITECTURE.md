# Arquitetura — SuperBonn (v2, baseada no pcrecomp)

## 1. O que é o Megabonk (medido no disco)

| Componente | Tamanho | Papel |
| --- | --- | --- |
| `Megabonk.exe` | ~650 KB | stub do player Unity: encaminha para `UnityMain` no `UnityPlayer.dll` |
| `UnityPlayer.dll` | ~30 MB | engine nativo Unity (gráficos D3D11/D3D12/Vulkan/GLES, física, áudio, VM) |
| `GameAssembly.dll` | ~55 MB | **todo o código C# do jogo + módulos do UnityEngine já compilados x64 pelo IL2CPP** |
| `baselib.dll` | ~220 KB | biblioteca de plataforma da Unity (threads, net) |
| `D3D12Core.dll` | ~5.6 MB | Agility SDK (D3D12) |
| `Rewired_Windows.dll` | — | plugin de input (Windows) |

Unity **2022.3 LTS**, backend **IL2CPP**, integrações: Steamworks.NET, Discord
SDK, Addressables, Rewired. Não existe fonte: tudo sai do binário.

## 2. A virada: o SDK que existia e a gente não tinha — **pcrecomp**

[`sp00nznet/pcrecomp`](https://github.com/sp00nznet/pcrecomp) (MIT) é o toolbox
unificado de recompilação estática de binários **PC**, e ele já entrega o que
eu tinha marcado como "não existe ferramenta":

- **`tools/lift/lift64_cpu.py`** — lifter x86-64 → C (16 GPR + 16 XMM, SSE2
  packed, string ops, `cpuid` encaminhado). Lacunas conhecidas: x87 e MMX viram
  `RECOMP_TODO`.
- **`tools/lift/generate64.py`** — driver de imagem inteira: catálogo fechado
  (`.pdata` + relocações DIR64 + `lea rip` de callbacks + thunks `jmp rel32`),
  TUs em pedaços, tabela de dispatch, mapa de imports.
- **`runtime/recomp64_cpu/` + `eh64.c`** — runtime 64-bit com CPU struct
  explícito **e EH de C++ do guest**: anda a pilha do guest pelo `.pdata`/
  `.xdata` original, lê o `FuncInfo` do `__CxxFrameHandler3`, roda o catch
  funclet como código guest e faz `longjmp` no landing pad. **É exatamente o
  bloqueador que eu tinha como risco nº1 (exceções MSVC) — e ele tem solução.**
- **`tools/lift/difftest64.py`** — diferencial lifted-C vs CPU real rodando os
  bytes originais (registros, memória, `lock`). Nosso oráculo de correção.
- **RE**: `pe_analyze`/`extract_imports`/`catalog`/`stdcall_argc`/
  `analyze_sections`, `disasm/disasm64` (recursive descent com switch tables do
  MSVC x64), `cpp/rtti`+`vtable_scan`, `ghidra/` e `ida/` headless,
  `classify/` (SDK vs código de jogo), `merge_names.py` (MAP/PDB/RTTI/`__FILE__`
  → um nome por endereço), `drm/` (inclusive SteamStup DRM stub, se um dia
  precisar), `assets/` (instaladores/ISO) e `tools/audit_repo.py`.
- **Prova de escala**: o mesmo lifter nasceu no `systemes3recomp` rodando um
  UE3 de arcade **Windows x86-64**: 3.6M instruções, 92.552 funções, **0 erros
  de lift**. Os projetos do toolbox já passam de 83.468 funções (The Movies).

**Consequência:** não construímos lifter. Forkamos/integramos o pcrecomp e
gastamos o tempo no que é específico do Megabonk: shims de import, tradução de
gráfico e integração Unity/Android. É o mesmo movimento do Black-Android em
cima do XboxXBE-Recomp — com a toolchain pronta, o port do jogo é rápido.

## 3. Pipeline

```
                    CI (GitHub Actions)
 Megabonk_Data/  ──► Il2CppDumper ──► mapa de símbolos (nome/endereço por método)
 Megabonk.exe etc ──► pcrecomp pe_analyze/disasm64 ──► catálogo de funções
                          │
                          ▼
                 lift64_cpu/generate64 ──► C (chunks) ──► difftest64 (amostra)
                          │
                          ▼
        clang NDK arm64-v8a  ──►  objetos  ──►  link com runtime/
                          │
                          ▼
                     libsuperbonn.so  ──►  APK (android/)
```

## 4. O que o `runtime/` implementa (host Android, sem Wine, sem emulador)

| Camada do guest | Implementação no Android |
| --- | --- |
| `recomp64_cpu` (CPU struct, dispatch, EH guest) | runtime do próprio pcrecomp, compilado pelo NDK |
| kernel32/advapi/ws2_32 (arquivos, threads, TLS, sockets) | shims C++ sobre bionic/POSIX/SDL3 |
| user32 (janela, mensagens, input) | SDL3 + surface Android + gestos de toque |
| D3D11 ou D3D12 (`D3D12Core.dll`) | tradução para Vulkan: build arm64 do **vkd3d-proton** (D3D12) ou **DXVK-arm64** (D3D11), decidido no Dia 4 do sprint |
| XAudio2/`baselib` áudio | SDL3 audio |
| Rewired_Windows | ponte de input SDL3 (gamepad + touch) |
| Steamworks.NET / Discord | stubs: login offline, sem conquistas/telemetria |

**Estratégia de fallback**: se o lift do `UnityPlayer.dll` (30 MB de C++ MSVC)
travar por cobertura do lifter (x87/MMX) ou tamanho, o plano B é não levantar a
engine: reimplementar só os internals que o jogo usa (a lista exata sai da RE:
`extract_imports` + `Il2CppDumper` + `classify`) e levantar apenas
`GameAssembly.dll`. Registrado como risco, não como plano.

## 5. RE como multiplicador (o que pediu)

1. **Il2CppDumper** → `script.json`/`dump.cs`: nome + endereço de cada método.
   Alimentado no `merge_names.py` do pcrecomp como fonte de nomes: o C levantado
   sai **nomeado**, não `FUN_1400...`.
2. **`extract_imports` na pasta inteira** → censo de imports: a lista exata de
   APIs Windows que os shims precisam cobrir, por prioridade de uso.
3. **`classify_functions.py` + `ghidra` headless** → separar engine/Unity de
   código do jogo e decompilar só as funções quentes para leitura humana.
4. **`difftest64`** → cada shard levantado é testado contra a CPU real antes de
   entrar no build. Correção medida, não esperada.

## 6. Repositório

```
launcher/   App Android (Kotlin): import BYO-copy + login Steam (Fase 1)
lift/       Configs e scripts do pipeline pcrecomp para o Megabonk
runtime/    Host C++ (NDK): shims Win32, SDL3, Vulkan, D3D→Vulkan, toque
android/    Gradle: launcher + runtime + código levantado → APK
tools/      checkout do pcrecomp (sibling/clone, nunca commitado)
docs/       architecture, roadmap, legal, operations, sprint
```

## 7. CI

| Job | Runner | O que faz |
| --- | --- | --- |
| `guard.yml` | ubuntu-latest | barra arquivo do jogo/binário proibido (+ `audit_repo.py`) |
| `lift.yml` | ubuntu-latest (self-hosted se preciso) | pcrecomp: catálogo → lift shardado → difftest → bitcode/objetos |
| `runtime.yml` | ubuntu-latest | NDK r27 + CMake → `libsuperbonn.so` |
| `apk.yml` | ubuntu-latest | Gradle → APK (debug; release só assinado local) |

Sem secrets de conta em nenhum job (`docs/OPERATIONS.md`).

## 8. Riscos (atualizado, honesto)

| Risco | Severidade | Mitigação |
| --- | --- | --- |
| Cobertura do lifter: x87/MMX → `RECOMP_TODO` (Unity usa SSE/AVX) | **alto** | medir no Dia 1 com `pe_analyze`+amostragem; se AVX/AVX2 aparecer muito, estender o lifter (é Python, é nosso fork) ou decidir pelo plano B (não levantar a engine) |
| Volume: ~85 MB de x64, C gerado na casa dos milhões de linhas | alto | sharding, cache de bitcode/objetos, runners grandes; custo de CI monitorado desde o Dia 1 |
| D3D12/D3D11 → Vulkan em arm64 | médio | builds arm64 prontos de vkd3d/DXVK (usados por GameNative/Winlator); decisão Dia 4 |
| Threading/jobs do Unity 2022 | médio | rodar single-threaded primeiro (`-batchmode`-ish), depois jobs |
| Steamworks.NET / Rewired / Discord | médio | stubs por módulo, um de cada vez |
| **Teste em device** (não tenho acesso ao seu celular) | **alto** | você é o harness: instalar APK + `adb logcat`. Sem isso não existe iteração — ver `docs/SPRINT.md` |
