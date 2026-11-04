#!/usr/bin/env bash
# Build desktop (Linux x86-64) do mesmo C levantado + runtime — lição do
# Skyrim_Android: iterar runtime/shim com GPU de PC, sem device.
# Uso: scripts/desktop/build.sh <lift-out-dir> <saida-binario>
#
# SKELETON: monta o esqueleto do build; o linker final depende dos shards
# levantados (Fase 2/3). Precisa de: clang, cmake, ninja, SDL3 e Vulkan dev.

set -euo pipefail
GEN="${1:?uso: build.sh <gen-dir> <out-bin>}"
OUT="${2:?uso: build.sh <gen-dir> <out-bin>}"

mkdir -p "$OUT"
echo "SKELETON: build desktop de $GEN -> $OUT"
echo "Passos previstos:"
echo "  1. compilar runtime/ (recomp64_cpu + shims) para x86-64 Linux"
echo "  2. compilar C de $GEN (flag: -w, chunks paralelos com limite de RAM)"
echo "  3. linkar com SDL3 + Vulkan + dxvk/vkd3d conforme o backend medido"
echo "  4. gerar $OUT com --selftest (sem abrir janela)"
exit 0
