#!/usr/bin/env python3
"""apply-fork.py — aplica o fork SuperBonn ao lifter do pcrecomp.

Idempotente: rodar 2x nao duplica o patch. Faz:
  1. copia lift/fork/lift64_avx.py -> tools/pcrecomp/tools/lift/
  2. insere o hook em tools/pcrecomp/tools/lift/lift64_cpu.py, dentro de
     translate(), logo depois do catch do x87 e ANTES do despacho SSE —
     assim as instrucoes VEX nunca caem no fallback 'sse %s' (TODO).

Uso: python scripts/lift/apply-fork.py [dir-do-pcrecomp]
     (padrao: tools/pcrecomp)
"""
from __future__ import annotations

import pathlib
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
FORK_SRC = ROOT / "lift" / "fork" / "lift64_avx.py"

MARKER = "# SuperBonn fork (scripts/lift/apply-fork.py)"

HOOK = '''
        # SuperBonn fork (scripts/lift/apply-fork.py): AVX 128-bit, SSE2
        # packed-double e no-op do check CFE — docs/RECON-DAY1.md §4.
        try:
            from lift64_avx import lift_avx as _sb_lift_avx
        except ImportError:
            _sb_lift_avx = None
        if _sb_lift_avx is not None:
            _sb_out = _sb_lift_avx(self, insn, m, ops, ea)
            if _sb_out is not None:
                return _sb_out
'''.rstrip("\n")

ANCHOR = """        if m[0] == 'f' and m not in ('fs',):
            return [_todo(ea, '%s %s (x87 not modelled)' % (m, insn.op_str))]
"""


def main() -> int:
    pcre = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "tools" / "pcrecomp"
    if not pcre.is_dir():
        print(f"pcrecomp ausente em {pcre} (rode build-toolchain.sh antes)")
        return 1

    lift_dir = pcre / "tools" / "lift"
    dst = lift_dir / "lift64_avx.py"
    if not FORK_SRC.exists():
        print(f"fork ausente: {FORK_SRC}")
        return 1
    shutil.copyfile(FORK_SRC, dst)
    print(f"copiado: {dst}")

    target = lift_dir / "lift64_cpu.py"
    text = target.read_text(encoding="utf-8")

    if MARKER in text:
        print("hook do lifter já aplicado (idempotente)")
    elif ANCHOR not in text:
        print("ANCORA nao encontrada em lift64_cpu.py — o pcrecomp mudou de "
              "layout; insira o hook a mao em translate() (depois do catch x87)")
        return 1
    else:
        patched = text.replace(ANCHOR, ANCHOR + HOOK, 1)
        target.write_text(patched, encoding="utf-8")
        print("patch aplicado em lift64_cpu.py")

    # 3. eh64.c: __declspec(thread) é Windows-only; thread_local é portátil
    eh = pcre / "runtime" / "recomp64_cpu" / "eh64.c"
    etext = eh.read_text(encoding="utf-8")
    if "__declspec(thread)" in etext:
        etext = etext.replace("__declspec(thread)", "thread_local")
        eh.write_text(etext, encoding="utf-8")
        print("patch aplicado em eh64.c (thread_local)")
    else:
        print("eh64.c ja portatil")
    return 0

    # 3. CPU struct do guest: storage para ymm (AVX-256). O lifecycle do CPU
    #    (init/reset/thread switch) e do host (nosso runtime/), entao o campo
    #    novo entra zerado como xmm.
    cpu_h = pcre / "runtime" / "recomp64_cpu" / "cpu64.h"
    htext = cpu_h.read_text(encoding="utf-8")
    if "ymm[16]" in htext:
        print("cpu64.h ja tem ymm[16]")
    elif "xmm[16];" in htext:
        htext = htext.replace(
            "XMM      xmm[16];",
            "XMM      xmm[16];\n    XMM      ymm[16];   /* SuperBonn: AVX-256 upper half */",
            1)
        cpu_h.write_text(htext, encoding="utf-8")
        print("patch aplicado em cpu64.h (ymm[16])")
    else:
        print("ANCORA do cpu64.h nao encontrada (layout mudou); ymm ficou sem "
              "storage — as instrucoes 256-bit vao continuar em TODO")

    htext = cpu_h.read_text(encoding="utf-8")
    if "mm[8]" in htext:
        print("cpu64.h ja tem mm[8]")
        return 0
    if "ymm[16]" in htext:
        htext = htext.replace(
            "XMM      ymm[16];   /* SuperBonn: AVX-256 upper half */",
            "XMM      ymm[16];   /* SuperBonn: AVX-256 upper half */\n"
            "    uint64_t  mm[8];    /* SuperBonn: MMX (memcmp MMX do CRT) */",
            1)
        cpu_h.write_text(htext, encoding="utf-8")
        print("patch aplicado em cpu64.h (mm[8])")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
