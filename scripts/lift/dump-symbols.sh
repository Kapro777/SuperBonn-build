#!/usr/bin/env bash
# SKELETON (Fase 2) — roda Il2CppDumper nos PE do jogo e produz o mapa de
# simbolos (nome + endereço de cada método) usado pelo lifter.
# Uso: dump-symbols.sh <dir-pe> <dir-saida>

set -euo pipefail
PE="${1:?uso: dump-symbols.sh <pe-dir> <out-dir>}"
OUT="${2:?uso: dump-symbols.sh <pe-dir> <out-dir>}"

DUMPER="${IL2CPP_DUMPER:-tools/Il2CppDumper}"
if [ ! -x "$DUMPER/Il2CppDumper" ]; then
  echo "Il2CppDumper ausente em $DUMPER (baixar release do github Perfare/Il2CppDumper)"
  exit 1
fi

mkdir -p "$OUT"
# Entradas: GameAssembly.dll + globalgamemanagers + globalgamemanagers.dat?
# (Unity 2022 usa globalgamemanagers + global-metadata.dat em Megabonk_Data/il2cpp_data)
echo "SKELETON: rodar $DUMPER/Il2CppDumper $PE/GameAssembly.dll $PE/../Megabonk_Data/il2cpp_data/Metadata/global-metadata.dat $OUT"
echo "Saida esperada: dump.cs + script.json + stringliteral.json (enderecos!)"
exit 0
