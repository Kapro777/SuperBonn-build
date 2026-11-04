# SuperBonn

**Engine nativa Android para rodar Megabonk (Steam/Unity IL2CPP x64) no celular, via recompilação estática.**

> Estado atual: scaffolding (fase 0). Nada compilado ainda.
>
> **BYO-copy:** o APK nunca inclui os arquivos do jogo. Você conecta sua conta
> Steam e baixa a *sua* cópia de Megabonk (ou importa de uma instalação Steam
> existente), igual o GDroid faz com Garry's Mod. O repositório também não
> versiona binários do jogo (ver `.gitignore`).

## O plano

1. **Launcher Android (`launcher/`)** — login da Steam, download do depósito do
   jogo, gerenciamento de arquivos, layout de controle touch.
2. **Recompilação estática (`lift/`)** — pipeline que levanta os binários x86-64
   do jogo (PE) para LLVM bitcode e recompila para **arm64-v8a**.
3. **Runtime nativo (`runtime/`)** — biblioteca C++ (NDK) que substitui Win32 /
   D3D12 / Unity host por SDL3 + Vulkan no Android, com controles touch.
4. **APK (`android/`)** — empacota runtime + código recompilado + dados do jogo
   baixados pelo launcher.

Todo build pesado roda no **GitHub Actions** (runners Ubuntu/Linux). A máquina
local (Celeron N4020) só edita arquivos.

## Documentação

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — arquitetura completa, pipeline
  de recompilação, riscos.
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — fases e marcos.

## Referências estudadas

| Projeto | O que é | Recompilação estática? |
| --- | --- | --- |
| [sp00nznet/pcrecomp](https://github.com/sp00nznet/pcrecomp) | **Toolbox unificado de recompilação de PC**: lifter x86-64→C, runtime com EH de C++ do guest, diferencial vs CPU real, RE de PE | **Sim — é a nossa base** (MIT) |
| [RanggaCasper/Black-Android](https://github.com/RanggaCasper/Black-Android) | Port de *Black* (Xbox) pra Android, arm64 + Vulkan + SDL3 | **Sim** — SDK XboxXBE-Recomp (x86 do XBE → C++). Nosso template de port |
| [RanggaCasper/ShaolinMonks-Android](https://github.com/RanggaCasper/ShaolinMonks-Android) | Segundo port da mesma autora, mesmo SDK, Vulkan + touch | **Sim** — prova do padrão: com SDK pronta, port novo é rápido |
| [sp00nznet/systemes3recomp](https://github.com/sp00nznet/systemes3recomp) | Arcade **Windows x86-64** (UE3) recompilado: 92.552 funções, 0 erros | **Sim — a prova de que x64 de PC levanta** |
| [tinsarfal/GDroid](https://github.com/tinsarfal/GDroid) | Garry's Mod no Android | Não — port nativo do Source (nillerusr). **O que copiamos é o launcher com download via Steam.** |

## Aviso legal

Uso não-oficial, feito por fãs, sem vínculo com a Unity/Valve/Ved. Requer que o
usuário possua o jogo na Steam. Nenhum arquivo do jogo é incluído neste
repositório.
