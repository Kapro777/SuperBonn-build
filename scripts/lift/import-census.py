#!/usr/bin/env python3
"""Censo de imports dos binarios do jogador (Dia 1 do sprint).

Substitui o extract_imports.py do pcrecomp, que so cobre PE32 (32-bit):
o Megabonk e PE32+ (x86-64). Saida: por modulo e agregado, que DLL/API o
guest importa — a lista que dirige os shims do runtime/ (docs/ARCHITECTURE §4).

Uso:
    python scripts/lift/import-census.py <pasta-com-PEs> [saida.txt]

NAO cometa a saida: e derivada dos binarios do jogo (docs/LEGAL.md §2).
"""
from __future__ import annotations

import pathlib
import sys
from collections import Counter, defaultdict

import pefile


def imports_of(path: pathlib.Path) -> list[tuple[str, str, int | None]]:
    """(dll, api, ordinal) de um PE, imports normais + delay-load."""
    out: list[tuple[str, str, int | None]] = []
    try:
        pe = pefile.PE(str(path), fast_load=True)
    except pefile.PEFormatError as exc:
        return [("<nao-PE>", str(exc), None)]

    pe.parse_data_directories(
        directories=[
            pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"],
            pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT"],
        ]
    )

    for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        dll = entry.dll.decode(errors="replace")
        for imp in entry.imports:
            if imp.name:
                out.append((dll, imp.name.decode(errors="replace"), None))
            else:
                out.append((dll, f"#{imp.ordinal}", imp.ordinal))

    for entry in getattr(pe, "DIRECTORY_ENTRY_DELAY_IMPORT", []):
        dll = entry.dll.decode(errors="replace") + " (delay)"
        for imp in entry.imports:
            if imp.name:
                out.append((dll, imp.name.decode(errors="replace"), None))
            else:
                out.append((dll, f"#{imp.ordinal}", imp.ordinal))

    pe.close()
    return out


def main() -> int:
    if len(sys.argv) < 2:
        print("uso: import-census.py <pasta-com-PEs> [saida.txt]")
        return 1

    root = pathlib.Path(sys.argv[1])
    out_path = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else None

    per_module: dict[str, list[tuple[str, str]]] = {}
    dll_counter: Counter[str] = Counter()
    api_calls: dict[str, set[str]] = defaultdict(set)

    files = sorted(p for p in root.rglob("*") if p.suffix.lower() in {".exe", ".dll"})
    for pe_path in files:
        rel = str(pe_path.relative_to(root))
        rows = imports_of(pe_path)
        per_module[rel] = [(d, a) for d, a, _ in rows]
        for dll, api, _ in rows:
            dll_counter[dll] += 1
            api_calls[dll].add(api)

    lines: list[str] = []
    lines.append("CENSO DE IMPORTS — SuperBonn (derivado dos binarios, NAO comitar)")
    lines.append(f"modulos analisados: {len(per_module)}")
    lines.append("")
    lines.append("== POR MODULO ==")
    for rel in sorted(per_module):
        lines.append(f"\n-- {rel} ({len(per_module[rel])} imports)")
        for dll, api in per_module[rel]:
            lines.append(f"   {dll} -> {api}")

    lines.append("\n== DLLs POR FREQUENCIA (prioridade de shim) ==")
    for dll, count in dll_counter.most_common():
        lines.append(f"{count:5d}  {dll}")

    lines.append("\n== APIs POR DLL (superficie a implementar) ==")
    for dll in sorted(api_calls):
        apis = sorted(api_calls[dll])
        lines.append(f"\n-- {dll} ({len(apis)} APIs)")
        for api in apis:
            lines.append(f"   {api}")

    text = "\n".join(lines)
    if out_path:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(text, encoding="utf-8")
        print(f"escrito: {out_path} ({len(text)} bytes)")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
