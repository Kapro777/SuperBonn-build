# Sprint SuperBonn — 5 dias

Base: **pcrecomp** (toolbox de recompilação estática x86-64, MIT) + Il2CppDumper
para os símbolos. Regra do jogo: cada dia tem entregável verificável e critério
de corte. O que não estiver no critério do dia **não** é feito.

## O que está PROMETIDO no fim dos 5 dias

- App Android (APK instalável) com: launcher BYO-copy validando seus arquivos,
  host SDL3/Vulkan com controles touch, e o **pipeline de recompilação rodando
  no GitHub Actions** de ponta a ponta: PE → catálogo → C → objeto arm64 →
  `difftest64` verde.
- `Megabonk.exe` (o stub) e `baselib.dll` levantados e linkados no host.
- Relatório de RE: censo de imports, mapa de símbolos, classificação
  engine/jogo, medição de cobertura do lifter (x87/MMX/AVX).
- **Jogo jogável: NÃO está prometido.** Um Unity de 85 MB de x64 é maior que
  tudo que o pcrecomp já levantou; o que cabe em 5 dias é prova de pipeline +
  fundação + começo do levantamento. Cada marco tem corte honesto abaixo.

## Dependência crítica: seu celular

Eu escrevo código, mas **cada bug precisa rodar no aparelho**. Sem device de
teste o sprint vira "entrega no escuro". Mínimo: Android 11+, arm64, Vulkan 1.1,
com `adb` (PC) ou instalando APK + me mandando `logcat`. Ideal: o modelo
exato — SD 4xx/6xx/7xx/8xx muda tudo em desempenho e em bugs de driver.

## Dia 1 — Ferramenta + RE (fundação)

- Push do repo (privado) — precisa Git na máquina ou MinGit portátil (posso
  baixar o oficial, peço sua autorização).
- `scripts/lift/build-toolchain.sh`: clone do pcrecomp + Python 3.11 + deps,
  com cache no CI.
- RE: `pe_analyze` + `catalog` na pasta do jogo; **Il2CppDumper** rodado →
  `script.json`/`dump.cs`; `extract_imports` na pasta inteira → censo.
- Medir cobertura: `disasm64` numa amostra + procurar x87/MMX/AVX nos bytes.
- **Entregável:** relatório de RE + veredito go/no-go do plano (lift total vs
  plano B só-`GameAssembly`).
- **Corte:** se AVX/x87 dominar `UnityPlayer.dll`, o sprint reescreve o Dia 3-5
  pelo plano B no mesmo dia (não insiste).

## Dia 2 — Pipeline ponta a ponta no stub

- Levantar `Megabonk.exe` (650 KB, C puro) com `generate64.py` → build tree.
- Compilar com NDK arm64 + `runtime/recomp64_cpu`; `difftest64` com 2.000
  instruções do próprio binário.
- Host Android mínimo: SDL3 + Vulkan surface + toque (já existe em
  `runtime/src/host.cpp`).
- **Entregável:** APK que instala e abre uma janela; C do stub compilado e
  linkado; diff testado.
- **Corte:** se `generate64` não fechar o catálogo do stub, paramos e investigamos
  antes de tocar nas DLLs.

## Dia 3 — Shims por censo de imports

- Implementar os shims do Top-20 do censo (kernel32: arquivos/threads/TLS/tempo;
  user32: janela/mensagens; ws2_32).
- Levantar `baselib.dll`; linkar e rodar no host.
- Começar `GameAssembly.dll`: primeiro shard (funções de init do runtime IL2CPP).
- **Entregável:** host executa init do runtime IL2CPP até o primeiro import
  não implementado (medido, não estimado).
- **Corte:** imports desconhecidos viram stub que aborta alto (padrão pcrecomp),
  nunca silencioso.

## Dia 4 — Gráficos e áudio

- Decidir D3D12→Vulkan (vkd3d-proton arm64) vs D3D11→Vulkan (DXVK-arm64) pelos
  imports reais medidos no Dia 1.
- Áudio: XAudio2/baselib → SDL3. Input: Rewired → ponte touch/gamepad.
- Continuar shards do `GameAssembly.dll`; `UnityPlayer.dll` só se o Dia 1 tiver
  sido go para lift total.
- **Entregável:** primeira superfície Vulkan renderizando algo do guest, OU o
  relatório exato do que falta.

## Dia 5 — Integração e configuração

- Launcher BYO-copy funcional na tela do celular: valida checksums dos seus
  arquivos (sem baixar nada, sem credencial).
- layout de toque inicial; `logcat` limpo de crashes óbvios.
- Publicar (privado) os artefatos do sprint e escrever o estado real em
  `docs/ROADMAP.md`.
- **Entregável:** APK dia-5 nas suas mãos + pipeline CI verde + lista do que
  falta para o Dia 6+.

## Kill switches

1. Dia 1 com veredito de plano B e replanejamento imediato do Dia 3-5.
2. CI estourando RAM/tempo além do plano gratuito → self-hosted ou redução de
  shard; não queimar dinheiro.
3. Pedido de remoção da Ved/Unity/Valve → tudo para (48h, `docs/LEGAL.md`).

## Trilha B (só se você pedir)

Se em paralelo quiser **algo jogável esta semana** (antes da recomp maturar), a
rota é a camada de tradução dinâmica que já existe (Box64/FEX + Wine + VKD3D,
como GameNative/Winlator/Bannerlator). **Não é recompilação estática** — é
translação em tempo de execução. Eu só toco essa trilha se você mandar, e ela
fica separada da engine SuperBonn.
