# launcher/ — app Android (Kotlin)

SKELETON — Fase 1 do roadmap. Modelo: o que o GDroid faz de melhor (trazer o
jogo da **sua própria conta Steam** para o aparelho).

## Escopo (nesta ordem)

1. **Importar cópia existente** — lê `/sdcard/...` e valida checksums. Sem
   credencial, sem risco: primeira tela a existir.
2. **Login Steam + download do depósito** — SteamKit2, protocolo oficial,
   credenciais **só no aparelho** (nunca em log/CI). Pausável/retomável.
3. **Gerenciador de arquivos** — updates do jogo, DLC, espaço livre.
4. **Editor de layout touch** — arrastar botões na tela (Fase 5).

## Regras (docs/LEGAL.md)

- Sem anúncios, analytics, telemetria.
- Permissões mínimas e justificadas na tela (`docs/PERMISSIONS.md`, a criar).
- O app **nunca** inclui nem baixa nada além do appid do próprio usuário.

## Stack

Kotlin + Jetpack Compose + Gradle. Nada de compilação pesada aqui — roda no CI
(`apk.yml`, Fase 4+).
