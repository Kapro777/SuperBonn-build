#!/usr/bin/env bash
# compile-one.sh — compila UM arquivo de C levantado, com fallback -O0.
# Usado pelo bringup do CI via xargs (paralelo):
#   xargs -P$(nproc) -I{} bash scripts/rt/compile-one.sh {} <outdir> "<includes>"
set -u
f="$1"; outdir="$2"; inc="$3"
[ -f "$f" ] || exit 0
mkdir -p "$outdir"
n="$(basename "$f" .c)"
out="$outdir/$n.o"
g++ -c "$f" -o "$out" -O1 -w -std=c++20 -fpermissive $inc 2>/dev/null
if [ ! -f "$out" ]; then
    g++ -c "$f" -o "$out" -O0 -w -std=c++20 -fpermissive $inc
fi
