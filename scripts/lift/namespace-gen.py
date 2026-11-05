#!/usr/bin/env python3
"""namespace-gen.py — prefixa os símbolos de um módulo levantado.

Dois módulos levantados (exe + UnityPlayer) têm símbolos com o MESMO nome
(`recomp_dispatch_table`, `L_<addr>`, `recomp_imports`...) e não linkam juntos.
Este script reescreve cada um com um prefixo por módulo, no diretório de saída.

Uso: python namespace-gen.py <gen-dir> <prefixo> [out-dir]
      prefixo: ex. "UP" para UnityPlayer, "GA" para GameAssembly.
"""
from __future__ import annotations

import pathlib
import re
import shutil
import sys


def main() -> int:
    if len(sys.argv) < 3:
        print("uso: namespace-gen.py <gen-dir> <prefixo> [out-dir]")
        return 1
    src = pathlib.Path(sys.argv[1])
    pfx = sys.argv[2]
    dst = pathlib.Path(sys.argv[3]) if len(sys.argv) > 3 else src

    # Símbolos gerados pelo generate64 que colidem entre módulos.
    SYMS = [
        "recomp_dispatch_table",
        "recomp_dispatch_count",
        "recomp_imports",
        "recomp_funcs_h_seen",      # guarda interna, se existir
    ]

    files = sorted(src.glob("*.c")) + sorted(src.glob("*.h"))
    touched = 0
    for f in files:
        text = f.read_text(encoding="utf-8")
        orig = text
        for s in SYMS:
            text = re.sub(r"\b%s\b" % re.escape(s), pfx + s, text)
        # funcoes L_<endereco>
        text = re.sub(r"\bL_(0[0-9A-Fa-f]+)\b", r"%sL_\1" % pfx, text)
        if text != orig:
            if dst != src:
                (dst / f.name).write_text(text, encoding="utf-8")
            else:
                f.write_text(text, encoding="utf-8")
            touched += 1

    # o dispatch do modulo precisa do seu proprio nome de tabela: o host
    # registra varios modulos, então nada mais a fazer aqui.
    print(f"prefixados {touched} arquivos com '{pfx}': {', '.join(SYMS[:3])} + L_*")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
