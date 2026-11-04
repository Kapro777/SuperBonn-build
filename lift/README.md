# lift/ — recompilação estática (core do SuperBonn)

Pipeline que transforma os binários x86-64 do Megabonk em código ARM64 nativo.
Base: **[pcrecomp](https://github.com/sp00nznet/pcrecomp)** (MIT) + Il2CppDumper.
Ver `docs/ARCHITECTURE.md` v2 e `docs/SPRINT.md`.

## O que já existe (não reinventamos)

| Peça | Do pcrecomp |
| --- | --- |
| lifter x86-64 → C | `tools/lift/lift64_cpu.py` |
| driver de imagem inteira + catálogo fechado | `tools/lift/generate64.py` |
| runtime 64-bit com **EH de C++ do guest** | `runtime/recomp64_cpu` + `eh64.c` |
| diferencial vs CPU real | `tools/lift/difftest64.py` |
| análise/recon do PE | `tools/pe/*`, `tools/disasm/disasm64.py` |
| RTTI/vtables, nomes | `tools/cpp/*`, `merge_names.py` |
| headless Ghidra/IDA | `tools/ghidra/*`, `tools/ida/*` |
| auditoria do repo | `tools/audit_repo.py` |

Lacunas conhecidas do lifter (medir no Dia 1): **x87 e MMX → `RECOMP_TODO`**.

## Específico do Megabonk (o que é nosso)

- `config/megabonk.toml` — módulos, ordem, hooks, shards.
- `scripts/lift/dump-symbols.sh` — Il2CppDumper → mapa de símbolos.
- `scripts/lift/merge-symbols.sh` — nomes do Il2CppDumper no `merge_names.py`.
- `scripts/lift/build-toolchain.sh` — clone/cache do pcrecomp + Python 3.11.
- `scripts/lift/lift-shard.sh` — sharding por faixa de função (o `GameAssembly.dll`
  tem dezenas de milhares; levantar tudo de uma vez estoura o runner).

## Saída

`lift/out/` (ignorado pelo git): C gerado + objetos arm64. **Nunca publicada**
— é derivada dos binários do jogo (`docs/LEGAL.md` §2).
