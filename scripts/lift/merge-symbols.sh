#!/usr/bin/env bash
# RE: funde os nomes do Il2CppDumper no merge_names.py do pcrecomp, para o C
# gerado sair nomeado em vez de FUN_1400...
# Uso: merge-symbols.sh <symbols-dir-saida-do-il2cppdumper> <out.json>

set -euo pipefail
SYM="${1:?uso: merge-symbols.sh <simbolos> <out.json>}"
OUT="${2:?uso: merge-symbols.sh <simbolos> <out.json>}"

PCRE="${PCRE_DIR:-tools/pcrecomp}"
if [ ! -e "$PCRE/tools/merge_names.py" ]; then
  echo "pcrecomp ausente em $PCRE (rode build-toolchain.sh antes)"
  exit 1
fi

echo "SKELETON: converter script.json -> formato de nomes e alimentar"
echo "  $PCRE/tools/merge_names.py -> $OUT"
echo "Formato alvo: uma linha '0xEND name' por funcao (mesmo do disasm64)."
exit 0
