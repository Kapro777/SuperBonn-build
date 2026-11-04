#!/usr/bin/env bash
# Prepara a toolchain: clone do pcrecomp (SDK de recompilacao) + Python 3.11.
# Uso: scripts/lift/build-toolchain.sh <destino>   (chamado pelo CI com cache)
#
# SKELETON real: o clone e a checagem de deps acontecem; o build do NDK fica
# no job runtime.yml.

set -euo pipefail
DEST="${1:?uso: build-toolchain.sh <destino>}"
PCRE="${PCRE_COMP_REPO:-https://github.com/sp00nznet/pcrecomp.git}"
PCRE_TAG="${PCRE_TAG:-main}"

mkdir -p "$DEST"
cd "$DEST"

python3 -V || { echo "Python 3.11 necessario"; exit 1; }

if [ ! -d pcrecomp/.git ]; then
  git clone --depth 1 --branch "$PCRE_TAG" "$PCRE" pcrecomp
else
  echo "pcrecomp ja existe em $DEST/pcrecomp"
fi

# Sanidade: os arquivos que o pipeline usa precisam existir.
for f in tools/pe/pe_analyze.py tools/lift/generate64.py tools/lift/lift64_cpu.py \
         tools/lift/difftest64.py tools/disasm/disasm64.py runtime/recomp64_cpu; do
  if [ ! -e "pcrecomp/$f" ]; then
    echo "::error::pcrecomp/$f ausente — a tag $PCRE_TAG mudou de layout?"
    exit 1
  fi
done

echo "OK: pcrecomp pronto em $DEST/pcrecomp"
