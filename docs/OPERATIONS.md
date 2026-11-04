# Operações seguras — SuperBonn

Como o projeto mexe em binário de jogo comercial, o **processo** é tão importante
quanto o código. Regras práticas para o mantenedor.

## 1. Repositório

- **Privado**, sempre. Criação manual pelo dono (não usar fork público).
- Branch protection na `main`; PRs revisados por uma segunda pessoa ou por você
  mesmo depois de 24h (revisão "fria" pega deslize).
- `.gitignore` já exclui `Megabonk/`. Antes de qualquer `git add .`, rode:

  ```powershell
  pwsh scripts/check_no_game_files.ps1
  ```

- Um workflow (`guard.yml`) refaz essa verificação no CI. Se ele falhar, o push
  está com conteúdo proibido: **não force**, remova do histórico (`git rm
  --cached` + commit) antes de tudo.

## 2. Segredos e dados do jogo

- **Nenhum secret de conta Steam** no repositório. GitHub Actions secrets nem
  são configurados para o projeto.
- Binários do jogo só entram na máquina/CI do mantenedor via:
  - pasta local `C:\Users\USER\bonk\Megabonk\` (ignorada pelo git), ou
  - upload manual para release **privada** do repo, ou
  - self-hosted runner (sua máquina mesmo, quando aguentar).
- O launcher, quando tiver login Steam, guarda credenciais **só no aparelho**,
  criptografadas com o keystore do Android, e nunca em log/telemetria.

## 3. CI — permissões e artefatos

- Todos os workflows começam com `permissions: contents: read` (mínimo).
- Artefatos de lift (bitcode/objetos) marcados `retention-days: 7` e **não
  publicados** em release.
- Sem `pull_request_target`, sem checkout de fork em job com permissão de
  escrita. PR de desconhecido não executa job que mexe em arquivo.

## 4. Assinatura do APK

- Keystore de release **nunca** no repositório nem no CI. Fica em gerenciador de
  senhas/local; você assina manualmente (`apksigner`) ou em runner self-hosted.
- Debug APK é assinado com chave de debug, distribuição só para você/testadores
  próximos.

## 5. Backups (mínimo viável)

- Keystore + senhas: gerenciador de senhas.
- Pasta `Megabonk/` local: backup em nuvem privada antes de qualquer
  experimento que mova/apague arquivo.
- Repo: está no GitHub, mas mantenha um clone local de tempo em tempo.

## 6. Se algo sair errado (runbook)

| Situação | Ação |
| --- | --- |
| Commit acidental com arquivo do jogo | `git rm --cached <arq>`, commit, e se já foi pushado: contactar suporte GitHub para purge do histórico antes de mais qualquer coisa |
| Pedido de remoção da Ved/Unity/Valve | deferir em 48h: repo → private/delete, APK → retirado de circulação |
| Conta Steam com comportamento estranho após uso do launcher | deslogar em todos os dispositivos, trocar senha; o launcher não deve pedir credenciais fora do device |
| Dúvida legal | `docs/LEGAL.md` + parecer de advogado antes de qualquer release público |

## 7. Escopo do que dividimos publicamente (se um dia)

Só o que é 100% nosso: launcher, runtime, ferramentas de lift. Nunca o APK com
o jogo, nunca o código recompilado, nunca commits derivados de binário.
