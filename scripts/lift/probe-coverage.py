#!/usr/bin/env python3
"""SKELETON (Dia 1) — mede cobertura do lifter pcrecomp nos binarios do jogo.

O lifter lift64_cpu cobre SSE/SSE2; x87 e MMX viram RECOMP_TODO. Unity 2022
pode usar AVX. Este script conta, por modulo, instrucoes por classe de SSE/AVX
e estima o risco do "plano B" (levantar so GameAssembly.dll).

Uso: probe-coverage.py <dir-com-PEs>
Saida: texto (vai para lift/work/recon/coverage.txt no CI).
"""
import sys
import pathlib

# Classes de interesse do lift64_cpu (ver tools/lift/lift64_cpu.py do pcrecomp):
# coberto: SSE/SSE2 packed+string, cpuid
# gap    : x87 (fpu), MMX, AVX/AVX2
COVERED_HINTS = (b"SSE2", b"SSE3", b"SSSE3")
GAP_HINTS = (b"MMX", b"AVX", b"AVX2", b"FMA")


def main() -> int:
    if len(sys.argv) < 2:
        print("uso: probe-coverage.py <dir-com-PEs>")
        return 1
    root = pathlib.Path(sys.argv[1])
    if not root.is_dir():
        print(f"diretorio inexistente: {root}")
        return 1

    print("modulo\ttamanho_MB\tnota")
    for pe in sorted(root.glob("*")):
        if not pe.is_file():
            continue
        size_mb = round(pe.stat().st_size / 1e6, 1)
        # Heuristica de sexta-feira: nao ha disassembly aqui ainda, so o
        # tamanho. O recon real (disasm64 do pcrecomp) completa esta tabela.
        print(f"{pe.name}\t{size_mb}\tSKELETON: rodar disasm64 + contar classes")
    print()
    print("Veredito go/no-go sai do Dia 1: se AVX/x87 dominar UnityPlayer.dll,")
    print("o sprint vai para o plano B (levantar so GameAssembly.dll).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
