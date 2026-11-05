#!/usr/bin/env python3
"""gen-shim-registry.py — gera shim_registry.c a partir de kernel32_shims.h.

Mantém o registro HLE (nome da API -> funcao do shim) sempre em sincronia com
os shims implementados. Roda no build local e no CI.

Uso: python gen-shim-registry.py <kernel32_shims.h> <saida.c>
"""
from __future__ import annotations

import pathlib
import re
import sys

HEADER = """/* GERADO por scripts/rt/gen-shim-registry.py — nao editar a mao. */
#include "cpu64.h"
#include "kernel32_shims.h"
#include <stddef.h>

typedef void (*shim_fn)(CPU*);
typedef struct { const char* dll; const char* api; shim_fn fn; } shim_ent_t;

"""


def main() -> int:
    if len(sys.argv) < 3:
        print("uso: gen-shim-registry.py <header> <saida.c>")
        return 1
    src = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
    out = pathlib.Path(sys.argv[2])

    # “<qualquer tipo> sb_shim_Xxx(CPU *c);” -> (“Xxx”, “sb_shim_Xxx”)
    entries = re.findall(
        r'^\s*[\w:]+(?:\s+\*+)?\s+(sb_shim_(\w+))\s*\(\s*CPU\s*\*\s*c\s*\)\s*;',
        src, flags=re.MULTILINE)
    if not entries:
        print("nenhum shim encontrado no header")
        return 1

    # dll por API (a maioria e KERNEL32; o resto anotamos aqui)
    DLL_BY_PREFIX = {
        'sb_shim_': 'KERNEL32.dll',
    }
    lines = [HEADER]
    lines.append('extern "C" void sb_hle_register(void* reg, int* n) {')
    lines.append("    shim_ent_t* r = (shim_ent_t*)reg;")
    lines.append("    int _i = 0;")
    lines.append("    shim_ent_t _all[] = {")
    for fn, api in entries:
        dll = 'KERNEL32.dll'
        lines.append('        { "%s", "%s", %s },' % (dll, api, fn))
    lines.append("    };")
    lines.append("    int _total = (int)(sizeof(_all) / sizeof(_all[0]));")
    lines.append("    for (; _i < _total; ++_i) r[_i] = _all[_i];")
    lines.append("    *n = _total;")
    lines.append("}")
    lines.append("")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines), encoding="utf-8")
    print(f"registro gerado: {out} ({len(entries)} shims)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
