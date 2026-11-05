# Sprint SuperBonn — estado real (calendário: **Dia 1**)

> **Atenção ao calendário:** hoje é o **Dia 1** do sprint. O que está marcado
> como feito abaixo foi feito **dentro do Dia 1** — o sprint está muito
> adiantado. Nada aqui é promessa; tudo tem número medido.

## Feito

| Item | Estado | Medição |
| --- | --- | --- |
| Repo privado + push | ✅ | github.com/Kapro777/SuperBonn, 9+ commits |
| RE: censo de imports, PE analyze | ✅ | `docs/RECON-DAY1.md` |
| Lift dos 4 módulos (disasm64+generate64) | ✅ | 439 + 796 + **148.582** + 62.211 funções, **0 erros**, ~1,8 GB de C, tudo no Celeron em minutos |
| Fork do lifter (AVX/ymm/FMA/SSE2/MMX/SAD/CFE) | ✅ | TODOs: exe **172→6**, GameAssembly **3.106→560**, UnityPlayer **22.331→7.160** (-70% no total) |
| Shims Win32 + testes no host | 🟡 | memória/arquivo/thread/misc: **42 testes passando** (`scripts/rt/test-shims.ps1`) |
| Build desktop p/ iterar sem device | 🟡 | esqueleto do job `desktop-bringup` no CI |

## O que ainda falta (ordem)

1. `user32`, `ws2_32`, `advapi32`, `winmm`/input, DXCore (89 APIs de adapter)
2. **Compilar** o C levantado (CI `runtime.yml` + `desktop-bringup`)
3. `difftest64` (oráculo de correção; precisa de MSVC local ou variante Linux)
4. Dia 4: decisão do backend gráfico (vkd3d-proton arm64 vs DXVK-arm64)
5. Boot real: `Megabonk.exe` → UnityMain → primeira tela no device

## Kill switches ativos

1. Cursor de TODO: GameAssembly 560 e UnityPlayer 7.160 — compostos de
   `div:` conservador (só dispara em runtime), MMX residual, e ~700 instruções
   de **I/O de porta** (`insd/outsd/int 0x70`) que são **dados lidos como
   código** — só importam se executados; o runtime aborta alto nesse caso.
2. CI estourando RAM/tempo → reduz shard ou self-hosted.
3. Pedido de remoção da Ved/Unity/Valve → 48h (`docs/LEGAL.md`).

Base: **pcrecomp** (toolbox de recompilação x86-64, MIT) + Il2CppDumper.
Precedente: **Skyrim_Android** (recompilação estática de jogo de PC para
Android, funcional) — copiamos o método, não o resultado.

## O que está PROMETIDO no fim dos 5 dias

- APK instalável: launcher BYO-copy validando seus arquivos + host SDL3/Vulkan
  com controles touch.
- Pipeline no GitHub Actions ponta a ponta: PE → catálogo → C → arm64.
- **Build desktop (Linux x86-64) do mesmo C rodando com smoke test** — é aqui
  que eu itero, seu celular só entra na validação final.
- `Megabonk.exe` e `baselib.dll` levantados, linkados e rodando no desktop.
- Relatório de RE: censo de imports, símbolos, cobertura do lifter.
- **Jogo jogável: não prometido.** Unity de 85 MB x64 é 4x o Skyrim; o Dia 5
  entrega a fundação + o pipeline pronto, com a lista exata do que falta.

## Device de teste

Motorola **Edge 30** — Snapdragon 778G+, Adreno 642L, 8 GB RAM. É melhor que o
Adreno 610/3 GB em que o Skyrim roda. vkd3d em Adreno 642L é viável.

## Padrões copiados do Skyrim_Android

1. `HOST_OVERRIDES`: função substituída pelo host → renomeada
   `f_<addr>_guest` automático. Shim entra sem editar C gerado.
2. Build desktop (`scripts/desktop/build.sh` + job `desktop-bringup`): iteração
   sem device.
3. D3D→Vulkan pronto (dxvk-native no caso dele; vkd3d-proton/DXVK-arm64 no
   nosso), SDL3 para janela/input/audio, XAudio2 reimplementado sobre SDL.
4. Launcher com presets + BYO-copy validado (a nossa Fase 1).
5. `port/log.txt` + switches por arquivo (`singlecore.txt`, `noprof.txt`) para
   diagnóstico no campo.

## Dia 1 — Ferramenta + RE ✅ FEITO

- Repo no GitHub (privado) — **criado e pushado**: `github.com/Kapro777/SuperBonn`.
- `build-toolchain.sh`: clone do pcrecomp + Python portátil, cache no CI.
- RE: `pe_analyze` + `catalog`; censo próprio `scripts/lift/import-census.py`
  (PE32+, o do pcrecomp só cobre 32-bit).
- **Entregável:** `docs/RECON-DAY1.md` + veredito **GO lift total** (~205 mil
  funções, 0 erros). Bonus: `Megabonk.exe` e `baselib.dll` já levantados
  inteiros (Dia 2 adiantado) e o **fork do lifter** (AVX/SSE2/ymm/CFE) que
  derruba os TODO de 172 → 4 no stub.

## Dia 2 — Pipeline ponta a ponta ✅ FEITO (local)

- `disasm64` + `generate64` em `Megabonk.exe` (439 funções, 0 erros, ~1 s),
  `baselib.dll` (796, 0 erros), `GameAssembly.dll` (148.582, 0 erros, 775 s) e
  `UnityPlayer.dll` (62.211, 0 erros, 589 s). C gerado: ~1,8 GB no total.
- Falta: compilar o C (CI/desktop — ver Dia 3) e o `difftest64`.

## Dia 2 — Pipeline ponta a ponta

- Levantar `Megabonk.exe` (650 KB) com `generate64.py` → C.
- Compilar no **desktop** (Linux x86-64) + `difftest64` (2.000 instruções).
- Host SDL3/Vulkan mínimo (já existe em `runtime/src/host.cpp`).
- **Entregável:** C do stub compilado e linkado no desktop, diff testado.
- **Corte:** se o catálogo do stub não fechar, para e investiga.

## Dia 3 — Shims por censo + baselib 🟡 EM ANDAMENTO

Feito (42 testes passando no host mingw — `scripts/rt/test-shims.ps1`):

| Arquivo | Cobre | Testes |
| --- | --- | --- |
| `runtime/src/shim/winabi.h` | ABI dos shims (CPU*, args rcx/rdx/r8/r9 + stack, tabela) | — |
| `runtime/src/shim/handles.h` | HANDLE do guest -> objeto do host | — |
| `runtime/src/shim/kernel32_memory.cpp` | VirtualAlloc/Free/Query, Heap*, TLS/FLS, Encode/DecodePointer | 10 |
| `runtime/src/shim/kernel32_file.cpp` | CreateFileA/W, Read/Write, SetFilePointer, GetFileSize, Find*, diretórios, mmap | 12 |
| `runtime/src/shim/kernel32_thread.cpp` | CreateThread (pthread + CPU por thread), CriticalSection recursivo, SRW, Event/Mutex/Semaforo, Wait, Sleep | 11 |
| `runtime/src/shim/kernel32_misc.cpp` | QPC/GetTickCount64, UTF-8↔UTF-16, GetCommandLine, módulos, Get/SetLastError, console, ExitProcess, GetSystemInfo, GetVersion | 9 |

Falta: user32 (janela), ws2_32 (rede — Steamworks.NET), advapi32 (registro),
winmm/hid/dinput8 (input → ponte SDL3), DXCore/D3DKMT (89 APIs de adapter —
decisão do Dia 4). Regra de ouro: shim faltante **aborta alto**, nunca silencia.

- Top-20 do censo de imports: kernel32 (arquivo/thread/TLS/tempo), user32
  (janela/mensagem), ws2_32.
- Levantar `baselib.dll`; linkar; rodar no desktop até o 1º import faltante.
- Primeiro shard do `GameAssembly.dll` (init do runtime IL2CPP).
- **Entregável:** init do runtime IL2CPP rodando até o limite real dos shims.

## Dia 4 — Gráficos e áudio

- Decidir backend pelos imports medidos: vkd3d-proton arm64 (D3D12) vs
  DXVK-arm64 (D3D11).
- Áudio: XAudio2/baselib → SDL3. Input: Rewired → ponte touch/gamepad.
- Shards do `GameAssembly.dll`; `UnityPlayer.dll` só se o Dia 1 foi go.
- **Entregável:** primeira superfície Vulkan renderizando algo do guest.

## Dia 5 — Integração

- Launcher BYO-copy na tela do celular: valida checksums, sem credencial.
- Smoke test no Edge 30; `logcat` limpo; layout de toque inicial.
- Artefatos privados + atualização honesta do ROADMAP.
- **Entregável:** APK dia-5 + pipeline CI verde + lista do Dia 6+.

## Kill switches

1. Dia 1 com veredito plano B → replaneja 3-5 imediato.
2. CI estourando RAM/tempo → reduz shard ou self-hosted. Não queimar dinheiro.
3. Pedido de remoção da Ved/Unity/Valve → 48h, `docs/LEGAL.md`.

## Realismo (para não ter ilusão)

O Skyrim tem 12 commits e 1 desenvolvedor, mas o trabalho por trás foi semanas.
O que faz parecer rápido: toolchain própria enxuta + jogo 32-bit/documentado +
substituição de subsistemas por host. Para o Megabonk o mesmo método encurta
muito, mas 85 MB x64 + VM IL2CPP + D3D12 continuam sendo mais trabalho — por
isso o Dia 5 não promete jogo jogável, promete a fundação que torna o Dia 6+
uma linha de produção.
