// desktop_main.c — bring-up multi-módulo do guest no host desktop.
//
// Uso: sb_desktop <PE-principal> [<PE-modulo>...]
//
//   1. carrega cada PE na sua base preferida (loader de identidade)
//   2. le os exports de cada modulo (nome -> endereco no guest)
//   3. registra os modulos (tabelas de dispatch + imports do C levantado)
//   4. preenche a IAT de cada modulo: import de outro modulo -> endereco
//      levantado; import do sistema -> sentinel HLE (shim)
//   5. dispatch no entry point do modulo principal
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "cpu64.h"
#include "multimod.h"
#include "kernel32_shims.h"
#include "sb_counts.h"   // SB_EXE_IMPORTS / SB_UP_IMPORTS (gerado no build)

// Módulo principal (sem prefixo). Os extras vêm com prefixo via
// namespace-gen.py e são declarados no bloco de cada build.
extern const sb_entry_t  recomp_dispatch_table[];
extern const uint32_t    recomp_dispatch_count;
extern const sb_import_t recomp_imports[];

// UnityPlayer (prefixo UP aplicado pelo namespace-gen.py).
extern const sb_entry_t  UPrecomp_dispatch_table[];
extern const uint32_t    UPrecomp_dispatch_count;
extern const sb_import_t UPrecomp_imports[];

typedef struct { const char* name; uint64_t va; } sb_export_t;

uint64_t sb_load_pe(const char* path, uint64_t* out_base, uint32_t* out_size);
int     sb_pe_exports(const char* path, sb_export_t* out, int cap);
void    sb_hle_register_module(const sb_module_t* m);
void    sb_hle_set_exports(int mod_idx, const sb_export_t* exps, int n);
sb_module_t* sb_hle_module(int i);
uint64_t sb_hle_slot_addr(sb_module_t* m, const char* dll, const char* name);
void    sb_hle_init(void);

#define SB_MAX_MODS 8
#define SB_EXPORT_CAP 2048

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("uso: %s <PE-principal> [<PE-modulo>...]\n", argv[0]);
        return 2;
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    int n_mods = argc - 1;
    if (n_mods > SB_MAX_MODS) { printf("[main] no maximo %d modulos\n", SB_MAX_MODS); return 2; }

    static sb_export_t exports[SB_MAX_MODS][SB_EXPORT_CAP];
    int export_counts[SB_MAX_MODS] = {0};
    uint64_t bases[SB_MAX_MODS], entries[SB_MAX_MODS];

#ifdef _WIN32
    extern CPU* g_current_cpu;
    AddVectoredExceptionHandler(1, [](PEXCEPTION_POINTERS ep) -> LONG {
        CPU* c = g_current_cpu;
        printf("[fault] codigo 0x%lx | guest rip 0x%llx rsp 0x%llx rax 0x%llx\n",
               (unsigned long)ep->ExceptionRecord->ExceptionCode,
               c ? (unsigned long long)c->rip : 0,
               c ? (unsigned long long)c->rsp : 0,
               c ? (unsigned long long)c->rax : 0);
        fflush(stdout);
        return EXCEPTION_CONTINUE_SEARCH;
    });
#endif

    // ---- 1. carrega cada PE ---------------------------------------------------
    for (int i = 0; i < n_mods; ++i) {
        uint32_t size = 0;
        bases[i] = 0;
        entries[i] = sb_load_pe(argv[i + 1], &bases[i], &size);
        if (!entries[i]) { printf("[main] falha carregando %s\n", argv[i + 1]); return 1; }
        export_counts[i] = sb_pe_exports(argv[i + 1], exports[i], SB_EXPORT_CAP);
        printf("[main] %s: %d exports\n", argv[i + 1], export_counts[i]);
    }

    // ---- 3. registra os modulos -------------------------------------------------
    // Modulo 0: exe (sem prefixo). Modulo 1: UnityPlayer (prefixo UP).
    for (int i = 0; i < n_mods; ++i) {
        sb_module_t m;
        memset(&m, 0, sizeof(m));
        const char* slash = strrchr(argv[i + 1], '\\');
        m.dll_name = slash ? slash + 1 : argv[i + 1];
        m.image_base = bases[i];
        m.hle_base = 0xE5E30000ull + (uint64_t)i * 0x100000ull;   // faixa própria
        m.hle_stride = 8;
        if (i == 0) {
            m.imports = recomp_imports;
            m.import_count = SB_EXE_IMPORTS;
            m.table = recomp_dispatch_table;
            m.table_count = recomp_dispatch_count;
        } else {
            m.imports = UPrecomp_imports;
            m.import_count = SB_UP_IMPORTS;
            m.table = UPrecomp_dispatch_table;
            m.table_count = UPrecomp_dispatch_count;
        }
        sb_hle_register_module(&m);
        sb_hle_set_exports(i, exports[i], export_counts[i]);
    }

    // ---- 4. IATs ------------------------------------------------------------------
    for (int i = 0; i < n_mods; ++i) {
        sb_module_t* m = sb_hle_module(i);
        for (uint32_t k = 0; k < m->import_count; ++k) {
            uint64_t addr = sb_hle_slot_addr(m, m->imports[k].dll, m->imports[k].name);
            wr64(m->imports[k].slot, addr);
        }
        printf("[main] IAT de %s: %u slots\n", m->dll_name, m->import_count);
    }

    sb_hle_init();
    printf("[main] entrando no guest em 0x%llx\n", (unsigned long long)entries[0]);

    // ---- 5. pilha + CPU + dispatch -------------------------------------------------
    uint64_t stack_size = 16u << 20;
    void* stack = calloc(1, stack_size);
    CPU* c = (CPU*)calloc(1, sizeof(CPU));
    c->rsp = ((uint64_t)(uintptr_t)stack + stack_size - 64) & ~15ull;
    dispatch(c, entries[0]);
    printf("[main] guest retornou (rax=0x%llx)\n", (unsigned long long)c->rax);
    return (int)(c->rax & 0xFF);
}
