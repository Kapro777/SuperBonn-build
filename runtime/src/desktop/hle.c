// hle.c — camada HLE do host: traduz os sentinelas da IAT em shims.
//
// generate64 emite:
//   recomp_dispatch_table[] — endereco do guest -> funcao L_ levantada
//   recomp_imports[]        — {slot_va, dll, api} por slot da IAT
//   HLE_SENTINEL(i) = HLE_BASE + i*8
// O loader escreve HLE_SENTINEL(i) no slot i; o dispatch ve o endereco na
// faixa HLE e chama hle_shims[i](CPU*).
//
// Regra de ouro (docs/OPERATIONS): shim NAO implementado ABORTA alto — nunca
// retorna valor falso em silencio.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu64.h"
#include "recomp_dispatch.h"

extern void sb_hle_register(shim_ent_t* reg, int* n);   // shim_registry.c

// ---- dispatch ---------------------------------------------------------------
void dispatch(CPU* c, uint64_t addr) {
    // faixa HLE?
    if (addr >= HLE_BASE && addr < HLE_BASE + (uint64_t)HLE_COUNT * HLE_STRIDE) {
        uint32_t i = (uint32_t)((addr - HLE_BASE) / HLE_STRIDE);
        if (i >= HLE_COUNT) {
            printf("[hle] sentinel %u fora da faixa\n", i);
            abort();
        }
        hle_shims[i](c);
        return;
    }
    // tabela de funcoes levantadas (busca binaria)
    int lo = 0, hi = (int)recomp_dispatch_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint64_t va = recomp_dispatch_table[mid].va;
        if (va == addr) { recomp_dispatch_table[mid].fn(c); return; }
        if (va < addr) lo = mid + 1; else hi = mid - 1;
    }
    printf("[hle] endereco 0x%llx sem funcao levantada — abortando\n",
           (unsigned long long)addr);
    abort();
}

// ---- registro de shims ------------------------------------------------------
typedef void (*shim_fn)(CPU*);

typedef struct {
    const char* dll;
    const char* api;
    shim_fn fn;
} shim_ent_t;

static shim_ent_t g_registry[512];
static int g_registry_n = 0;

shim_fn hle_shims[HLE_COUNT];   // preenchido no init

shim_fn hle_lookup(const char* api) {
    for (int i = 0; i < g_registry_n; ++i)
        if (strcmp(g_registry[i].api, api) == 0) return g_registry[i].fn;
    return NULL;
}

// Stub de aborta-alto para API sem shim (nunca retorna falso em silencio).
static void hle_unimplemented(CPU* c) {
    (void)c;
    printf("[hle] shim NAO implementado — abortando (nunca retornar falso)\n");
    abort();
}

void hle_init(void) {
    sb_hle_register(g_registry, &g_registry_n);
    printf("[hle] %d shims registrados\n", g_registry_n);
    for (uint32_t i = 0; i < HLE_COUNT; ++i) {
        if (i >= (uint32_t)0 && i < HLE_COUNT) {
            const char* dll = recomp_imports[i].dll;
            const char* name = recomp_imports[i].name;
            shim_fn fn = hle_lookup(name);
            if (!fn) {
                printf("[hle] ! %s!%s sem shim (aborta se chamado)\n", dll, name);
                fn = hle_unimplemented;
            }
            hle_shims[i] = fn;
            // escreve o sentinel no slot da IAT (memoria do guest)
            wr64(recomp_imports[i].slot, HLE_SENTINEL(i));
        }
    }
    printf("[hle] %u slots de import preenchidos\n", HLE_COUNT);
}
