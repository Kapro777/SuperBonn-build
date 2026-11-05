// test_loader.cpp — teste do loader multi-módulo (host mingw).
// Cobre: load com relocacao (base alternativa), exports, IAT com resolucao
// cruzada entre modulos, e o dispatcher de sentinelas.
//
// Uso: test_loader <PE-principal> <PE-modulo>
#include <cstdio>
#include <cstring>

#include "multimod.h"
#include "kernel32_shims.h"
#include "sb_test_counts.h"

extern const sb_entry_t  recomp_dispatch_table[];
extern const uint32_t    recomp_dispatch_count;
extern const sb_import_t recomp_imports[];

uint64_t sb_load_pe_ex(const char*, uint64_t*, uint32_t*, int64_t*, int);
int     sb_pe_exports(const char*, sb_export_t*, int);
void    sb_hle_register_module(const sb_module_t*);
void    sb_hle_set_exports(int, const sb_export_t*, int);
sb_module_t* sb_hle_module(int);
uint64_t sb_hle_slot_addr(sb_module_t*, const char*, const char*);
void    sb_hle_init(void);
void    sb_set_module_delta(int, int64_t);

#define CAP 2048
static sb_export_t exports_a[CAP], exports_b[CAP];

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 3) { printf("uso: %s <PE-A> <PE-B>\n", argv[0]); return 2; }

    int fails = 0;
    uint64_t base_a = 0, base_b = 0;
    uint32_t size_a = 0, size_b = 0;
    int64_t delta_a = 0, delta_b = 0;

    // 1. carrega os dois
    uint64_t entry_a = sb_load_pe_ex(argv[1], &base_a, &size_a, &delta_a, 1);
    if (!entry_a) { printf("FALHOU: load %s\n", argv[1]); return 1; }
    sb_set_module_delta(0, delta_a);
    uint64_t entry_b = sb_load_pe_ex(argv[2], &base_b, &size_b, &delta_b, 1);
    if (!entry_b) { printf("FALHOU: load %s\n", argv[2]); return 1; }
    sb_set_module_delta(1, delta_b);

    // 2. bases diferentes (ou delta 0 quando nao colidem)
    printf("bases: A=0x%llx B=0x%llx (deltas %lld, %lld)\n",
           (unsigned long long)base_a, (unsigned long long)base_b,
           (long long)delta_a, (long long)delta_b);
    if (base_a == base_b) { printf("FALHOU: bases iguais\n"); ++fails; }

    // 3. exports
    int na = sb_pe_exports(argv[1], exports_a, CAP);
    int nb = sb_pe_exports(argv[2], exports_b, CAP);
    printf("exports: A=%d B=%d\n", na, nb);
    if (na <= 0 || nb <= 0) { printf("FALHOU: sem exports\n"); ++fails; }
    for (int k = 0; k < nb; ++k)
        exports_b[k].va = (uint64_t)((int64_t)exports_b[k].va + delta_b);

    // 4. registra os modulos
    sb_module_t ma, mb;
    memset(&ma, 0, sizeof(ma));
    memset(&mb, 0, sizeof(mb));
    const char* slash_a = strrchr(argv[1], '\\');
    const char* slash_b = strrchr(argv[2], '\\');
    ma.dll_name = slash_a ? slash_a + 1 : argv[1];
    mb.dll_name = slash_b ? slash_b + 1 : argv[2];
    ma.image_base = base_a; ma.hle_base = 0xE5E30000ull; ma.hle_stride = 8;
    ma.imports = recomp_imports; ma.import_count = SB_TEST_IMPORTS_A;
    ma.table = recomp_dispatch_table; ma.table_count = recomp_dispatch_count;
    sb_hle_register_module(&ma);
    sb_hle_set_exports(0, exports_a, na);

    // modulo B usa tabelas com prefixo BL (baselib) — declaradas pelo build
    extern const sb_entry_t  BLrecomp_dispatch_table[];
    extern const uint32_t    BLrecomp_dispatch_count;
    extern const sb_import_t BLrecomp_imports[];
    mb.image_base = base_b; mb.hle_base = 0xE5E40000ull; mb.hle_stride = 8;
    mb.imports = BLrecomp_imports; mb.import_count = SB_TEST_IMPORTS_B;
    mb.table = BLrecomp_dispatch_table; mb.table_count = BLrecomp_dispatch_count;
    sb_hle_register_module(&mb);
    sb_hle_set_exports(1, exports_b, nb);

    // 5. IAT do modulo A: todo import resolve (levantado, shim ou falha)
    sb_module_t* ra = sb_hle_module(0);
    int resolvidos = 0, shims = 0;
    for (uint32_t k = 0; k < ra->import_count; ++k) {
        uint64_t a = sb_hle_slot_addr(ra, ra->imports[k].dll, ra->imports[k].name);
        if (a) {
            wr64(ra->imports[k].slot, a);
            ++resolvidos;
            if (a >= 0xE5E30000ull && a < 0xE5E30000ull + 0x100000ull) ++shims;
        }
    }
    printf("IAT de A: %d/%u resolvidos (%d shims)\n",
           resolvidos, ra->import_count, shims);

    sb_hle_init();

    if (fails) { printf("\n%d FALHAS\n", fails); return 1; }
    printf("\nloader ok\n");
    return 0;
}
