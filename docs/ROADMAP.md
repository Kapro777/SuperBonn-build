# Roadmap — SuperBonn

> **Atualizado (v2):** com a descoberta do **pcrecomp** (`docs/ARCHITECTURE.md`
> v2), o lifter já existe e tem prova em x86-64 de PC (92.554 funções, 0 erros
> no SystemES3). O plano de 5 dias está em `docs/SPRINT.md`.

Cada fase é **entregável sozinha** e tem critério de saída escrito. Se uma fase
falhar no critério, paramos e reavaliamos (kill switch) — nenhuma fase depende de
"depois a gente vê".

## Fase 0 — Fundação (sem risco, sem código do jogo) ✅ em andamento

- Repositório privado, `.gitignore`, LICENSE, docs legal/ops.
- Esqueleto dos módulos e dos workflows com `permissions` mínimas.
- **Saída:** repo pronto para receber código; guarda `guard.yml` ativa.

## Fase 1 — Launcher + "BYO copy" (entregável, útil por si só)

- App Android mínimo: importar cópia existente (`/sdcard/Megabonk/...`) e
  listar/validar arquivos (checksums).
- Login Steam + download do depósito da própria conta (SteamKit2), pausável,
  com confirmação explícita. Só depois do import manual funcionando.
- Permissões mínimas justificadas; sem telemetria.
- **Saída:** o usuário consegue, no celular, trazer o Megabonk da própria
  conta Steam e ver "arquivos OK". Nada de recomp ainda.

## Fase 2 — Prova de conceito do lifter (o gate técnico)

- Clone do pcrecomp no CI (cache) + RE: Il2CppDumper, `pe_analyze`,
  `extract_imports`, amostragem de cobertura do lifter.
- Lift de `Megabonk.exe` (o stub pequeno, puro C) → bitcode → arm64, com
  símbolos. Rodar no device só para provar o pipeline (não é jogável).
- Il2CppDumper sobre `GameAssembly.dll` + `globalgamemanagers` → mapa de
  símbolos.
- Lift de **uma** função do `GameAssembly.dll` escolhida pelo mapa e teste de
  sanidade (executa no host ARM e retorna o valor esperado).
- **Saída (go/no-go):** pipeline funciona ponta a ponta em binário pequeno e
  uma função do jogo recompilada executa correto. Se o gate falhar, o projeto
  muda de estratégia antes de gastar meses.

## Fase 3 — Runtime

- SDL3 + surface Vulkan + input touch no NDK.
- Shims Win32 necessários ao que a Fase 2 levantou (arquivo, thread, tempo).
- D3D12→Vulkan (vkd3d arm64) ou `-force-gles`.
- **Saída:** uma janela Vulkan no celular com o host funcionando e controles.

## Fase 4 — Jogo rodando (o moonshot, honestamente)

- Lift progressivo: engine → telas de menu → gameplay.
- Hooks de Steamworks/Rewired/Discord (stub ou shim).
- Otimização e perf (cache de bitcode, PGO do que foi levantado).
- **Saída:** Megabonk jogável no celular com toque/gamepad.

## Fase 5 — Polimento

- Layout de controles editável, FPS/resolution scaling, saves, suporte a
  gamepad Bluetooth, updates OTA do APK (como o GDroid tem).

## Riscos que encerram o projeto (kill switches)

1. Gate da Fase 2 não passa em 8 semanas de esforço focado.
2. Custo de CI inviável (RAM/tempo dos runners) para o volume de lift.
3. Pedido de remoção da Ved/Unity/Valve (ver `docs/LEGAL.md` §6).
