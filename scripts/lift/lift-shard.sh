#!/usr/bin/env bash
# SKELETON (Fase 2) — levanta um shard de funcoes do PE para LLVM bitcode.
# Uso: lift-shard.sh <pe-dir> <symbols-dir> <shard i/n> <out-dir>
#
# Estrategia: sharding por função (não por arquivo) usando o mapa de simbolos do
# Il2CppDumper, porque GameAssembly.dll tem dezenas de milhares de funcoes e o
# lift de uma vez só estoura RAM/tempo do runner.

set -euo pipefail
PE="${1:?uso: lift-shard.sh <pe> <simbolos> <i/n> <out>}"
SYM="${2:?uso: lift-shard.sh <pe> <simbolos> <i/n> <out>}"
SHARD="${3:?uso: lift-shard.sh <pe> <simbolos> <i/n> <out>}"
OUT="${4:?uso: lift-shard.sh <pe> <simbolos> <i/n> <out>}"

echo "SKELETON: shard $SHARD de $PE com simbolos de $SYM -> $OUT"
echo "Passos previstos (pcrecomp):"
echo "  1. selecionar funcoes do shard (script.json + ranges + names.txt)"
echo "  2. generate64.py --split N -> C em pedacos"
echo "  3. difftest64.py --count 2000 (amostra deste shard)"
echo "  4. compilar C com NDK arm64; cache de objetos para nao re-levantar"
exit 0
