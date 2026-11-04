# android/ — APK SuperBonn (Gradle)

SKELETON — Fase 4. Junta tudo:

```
launcher (Kotlin)  +  runtime/libsuperbonn.so (NDK arm64)  +  código recompilado (lift/out)
        └──────────────────► app-release.apk assinado localmente
```

## Build

No CI (`.github/workflows/apk.yml`, a criar): Gradle + AGP + NDK r27.
Localmente **não** — Celeron N4020 não compila isso.

## Assinatura

Keystore de release **nunca** no repo/CI (`docs/OPERATIONS.md` §4): você assina
com `apksigner` na sua máquina ou em runner self-hosted. Debug APK só para você
e testadores próximos.

## Dados do jogo

O APK não inclui Megabonk. O launcher baixa/importa a cópia do próprio usuário
para o armazenamento do app (`Android/data/...`), como o GDroid faz.
