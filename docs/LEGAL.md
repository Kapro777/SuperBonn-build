# Postura legal e de segurança — SuperBonn

Objetivo: **risco zero para o mantenedor**. Este documento descreve as regras do
projeto. Elas não são negociáveis — qualquer contribuição que viole uma delas é
rejeitada.

## 1. Nada de conteúdo do jogo no repositório

- Binários (`Megabonk.exe`, `GameAssembly.dll`, `UnityPlayer.dll`, `D3D12Core.dll`,
  `baselib.dll`), assets, saves e qualquer dado de jogo **nunca** entram no repo.
- O `.gitignore` bloqueia a pasta `Megabonk/` inteira, e existe um workflow de
  guarda (`guard.yml`) que falha o CI se algum desses arquivos aparecer.
- Modelo **BYO-copy (bring your own copy)** — o mesmo de GDroid, Black-Android e
  rePOP: quem roda o port precisa ter o jogo. O launcher baixa a cópia do
  próprio usuário a partir da conta Steam dele, ou importa de uma instalação
  Steam existente.

## 2. Nada de material proprietário

- Não versionamos assets da Unity, da Valve, da Ved, nem código decompilado que
  não seja nosso.
- O código **gerado pelo lifter** (C/C++ saindo de `lift/`) é derivado dos
  binários do jogo: ele vive apenas como artefato privado de CI (retenção curta)
  e **nunca é publicado** em release, tag ou branch pública.

## 3. Nada de credenciais

- Nenhuma credencial da Steam (usuário, senha, refresh token, Steam Guard) no
  repo, em issues, em Actions secrets ou em logs.
- O CI **nunca** baixa o jogo usando conta Steam. Os binários para a recompilação
  são fornecidos pelo próprio mantenedor, manualmente (upload para release
  privada, self-hosted runner, ou execução local do lifter).
- Se/us quando o launcher tiver login Steam (estilo GDroid), isso acontece
  **só no aparelho do usuário**, via protocolo oficial (SteamKit2), e as
  credenciais ficam apenas no armazenamento do app, nunca em log, nunca em
  telemetria.

## 4. Sem quebra de proteção

- Não há DRM em Megabonk, mas a regra vale para o futuro: o projeto **não**
  neutraliza anti-cheat, nem redistribui loaders/cracks. Só operamos sobre
  arquivos que o usuário legitimamente possui.

## 5. Distribuição

- O repositório é **privado por padrão**; convites são manuais e nominativos.
- O APK nunca é vendido nem distribuído com o jogo incluso. Se um dia sair um
  release público, ele sai sem código recompilado e sem dados do jogo, e o
  usuário precisa trazer os seus arquivos.

## 6. Resposta a pedido de remoção

- Se a Ved, a Unity ou a Valve pedirem remoção ou modificação, atendemos:
  `docs/CONTACT.md` guarda o e-mail de contato do mantenedor e o processo é
  encerrar o repo/APK em até 48h, sem discussão.

## 7. Conduta do app

- O launcher não terá anúncios, analytics, telemetria ou coleta de dados.
- Permissões Android mínimas e justificadas em `docs/PERMISSIONS.md` quando o
  app existir (acesso a arquivos apenas para importar/ler a cópia do jogo do
  próprio usuário).

## 8. Riscos conhecidos que não são nossos de eliminar (avisar, não esconder)

- Usar o port é uso não-oficial. A Valve não baniu ninguém por *possuir* cópia
  própria de jogo, mas o usuário decide por sua conta e risco se sideloada APKs.
- Rodar port/emulador no celular é por conta do usuário; o projeto segue o mesmo
  modelo dos ports da cena (AndroidNativePorts, GDroid, Black-Android).
