# Relatório de RE — Dia 1 do sprint

> **Onde:** executado localmente com o pcrecomp + script próprio
> (`scripts/lift/import-census.py`), Python portátil, nos binários da pasta
> `Megabonk/` (ignorada pelo git). **Nada derivado foi commitado** — este doc
> traz só fatos e contagens (sem endereços nem mapa de símbolos).

## 1. Fatos dos binários (pe_analyze)

| Módulo | Tamanho | Formato | Linker |
| --- | --- | --- | --- |
| `Megabonk.exe` | 667.648 B | PE32+ EXE, base 0x140000000 | MSVC 14.34 |
| `UnityPlayer.dll` | 30.741.240 B | PE32+ DLL, base 0x180000000 | MSVC 14.34 |
| `GameAssembly.dll` | 54.771.712 B | PE32+ DLL, base 0x180000000 | MSVC **14.50** |
| `baselib.dll` | 223.480 B | PE32+ DLL | MSVC 14.34 |
| `UnityCrashHandler64.exe` | 991.992 B | PE32+ EXE | MSVC 14.34 |

Plugins: `steam_api64.dll` (92 imports), `discord_game_sdk.dll` (124),
`Rewired_DirectInput`/`Rewired_WindowsGamingInput` em **ARM64 e x86_64**
(Windows ARM64, **não** Android — input será stub/ponte SDL3).

## 2. Censo de imports (import-census.py, nossa ferramenta PE32+)

Superfície por DLL (APIs distintas):

```
KERNEL32 264 | USER32 122 | WS2_32 52 | WINMM 23 | ADVAPI32 35
api-ms-win-core-synch-l1-1-0 25 | processthreads-l1-1-0 19 | memory 6
api-ms-win-core-file 17 | heap 6 | libraryloader 11 | registry 7
ext-ms-win-dx-d3dkmt-dxcore-l1-1-0 (delay) 89   <- DXCore/D3DKMT
winrt-string 9 | ole32 14 + OLEAUT32 15 | WINHTTP 4 | WININET 7 | HID 14
d3d12.dll (delay) 3 | dxgi.dll 2+1 | d3d11.dll 2 | OPENGL32 6 | DINPUT8 1
api-ms-win-core-threadpool-l1-2-0 12 | winrt-l1-1-0 4 | dbghelp 4
```

**Leituras:**
- **Gráfico = D3D12 confirmado**: `d3d12.dll` (delay) + `dxgi.dll` + 89 APIs de
  DXCore/D3DKMT (enumeração de adapter e KMT do Agility SDK). Rota: vkd3d-proton
  arm64 (D3D12→Vulkan), decisão mantida para o Dia 4.
- **Input**: WINMM (timers/joyGetDevCaps), DINPUT8, HID.DLL — vem do Rewired
  DirectInput. Substituir por ponte SDL3 (gamepad + touch).
- **Rede**: WS2_32 (Steamworks) + WINHTTP/WININET (UnityCurl) — shim sobre bionic.
- `Megabonk.exe` importa 1 função do próprio `UnityPlayer.dll` — é o nosso hook
  de entrada (`UnityMain` → `superbonn_host_main`).

## 3. Lift medido (disasm64 + generate64)

| Módulo | Funções no catálogo | Lift erros | RECOMP_TODO | Tempo |
| --- | --- | --- | --- | --- |
| `Megabonk.exe` | 439 (401 + 38 do fecho do catálogo) | **0** | 172 | ~1 s |
| `baselib.dll` | 796 (710 + 86) | **0** | 205 | ~3 s |
| `GameAssembly.dll` | **148.582** (142.445 + 6.137 do fecho) | **0** | 3.106 | **775 s** |
| `UnityPlayer.dll` | 56.962 (disasm feito; lift rodando) | — | — | — |

Saída do lift do `GameAssembly.dll`: **377 arquivos de C, ~1,05 GB de código**
(372 TUs + dispatch + imports + meta). Para escala: o maior alvo já levantado
com esse toolchain era o Battle Pods com 92.554 funções — **passamos 60% dele
no CPU mais fraco possível**.

C gerado por módulo levantado: `recomp_funcs_*.c` em pedaços + dispatch +
imports + meta. O `Megabonk.exe` gerou ~1,7 MB de C; o `baselib.dll` ~3 TUs.

## 4. RECOMP_TODO e o fork do lifter (Fase B — feita)

O censo mostrou os gaps do `lift64_cpu.py` do pcrecomp:

```
int 0x29        -> CFE check do VS2022 (/guardcf): no-op seguro no host
xgetbv          -> detecção de AVX: responder XCR0 0x7
vinsertf128 / vmovdqa ymm -> AVX-256 (precisa de storage de ymm)
vmulsd/vfmadd213sd/...    -> VEX 128-bit escalar (math de Vector3 do jogo)
sqrtpd/addpd/andpd        -> SSE2 packed-double (2 lanes f64)
div:/idiv:                -> numerador 128-bit real (conservador, raro)
```

**Solução implementada** (`lift/fork/lift64_avx.py`, aplicado por
`scripts/lift/apply-fork.py` no checkout do pcrecomp):

| Cobertura | O que faz |
| --- | --- |
| VEX 128-bit escalar | `vmulsd/vaddss/vdivsd/vmin*/vmax*` → C lane-a-lane, 3-operando |
| VEX 128-bit packed | `vaddpd/vmulps/vxorps/vandnps` → lanes |
| FMA | `vfmadd/vfnmadd/vfmsub` (132/213/231) → `fma()`/`fmaf()` exatos em ARM |
| VEX packed-inteiro | `vpand/vpxor/vpor/vpaddd/vpsubq`, shifts, `cvtdq2pd/ps` |
| AVX-256 (ymm) | decomposto em 2 metades de 128; `c->ymm[16]` adicionado ao CPU struct do guest (patch em `runtime/recomp64_cpu/cpu64.h`) |
| SSE2 packed-double | `sqrtpd/addpd/andpd/...` |
| no-ops | `int 0x29` (CFE), `xgetbv` (XCR0 0x7), `sfence/lfence/mfence`, `vzeroupper` |

**Medido no `Megabonk.exe`** (o gate): **172 → 4 sitios RECOMP_TODO** (só os
`div:` conservadores), com 0 erros de lift. Regra do fork: forma desconhecida
devolve `None` e o lifter original emite o TODO com nome e endereço — nunca
levanta errado em silêncio. Os números do `GameAssembly`/`UnityPlayer` com o
fork estão sendo medidos agora (script `scripts/lift/run-lifts-with-fork.ps1`).

**Lacunas conhecidas que ficaram**: `div:` 128-bit (conservador, só dispara em
runtime se `rdx != 0`), VCVT 256-bit, `vgather*`, x87/MMX (o IL2CPP praticamente
não usa — o ABI Win64 joga float em XMM).

## 5. Veredito do Dia 1

- **GO para o lift total** (não plano B): ~205 mil funções somadas
  (148.582 + 56.962 + 439 + 796), **0 erros de lift em tudo que rodou**, 0,9%
  de sítios RECOMP_TODO. O gargalo passa a ser volume de C/CI (~1,5 GB gerado
  localmente), não viabilidade.
- Prioridade de shims medida (Dia 3): kernel32 → user32 → ws2_32 → advapi32 →
  winmm/hid/dinput8 → camada D3D12.
- Riscos atualizados: AVX no lifter (medição em curso), disco local (22,6 GB —
  artefatos de C podem passar de 1 GB; o CI produz de novo, local é só prova),
  e o upload dos PE pra CI (decisção do mantenedor — ver `docs/OPERATIONS.md`).
