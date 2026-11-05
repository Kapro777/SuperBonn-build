// multimod.c — dispatch multi-módulo + resolução de imports/exports.
//
// Substitui a lógica de módulo único. Roda no desktop e, depois, no Android
// (mesmo C; só o loader de memória muda).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu64.h"
#include "multimod.h"
#include "kernel32_shims.h"

// ---- registro de shims (extern "C" em shim_registry.c) ----------------------
extern "C" void sb_hle_register(void* reg, int* n);

typedef void (*shim_fn)(CPU*);
typedef struct { const char* dll; const char* api; shim_fn fn; } shim_ent_t;

static shim_ent_t g_registry[512];
static int g_registry_n = 0;

static shim_fn hle_lookup(const char* api) {
    for (int i = 0; i < g_registry_n; ++i)
        if (strcmp(g_registry[i].api, api) == 0) return g_registry[i].fn;
    return NULL;
}

// ---- módulos ------------------------------------------------------------------
#define SB_MAX_MODULES 6
static sb_module_t g_mods[SB_MAX_MODULES];
static int g_mods_n = 0;
static const sb_export_t* g_exports[SB_MAX_MODULES];
static int g_export_counts[SB_MAX_MODULES];

void sb_hle_register_module(const sb_module_t* m) {
    if (g_mods_n >= SB_MAX_MODULES) {
        printf("[hle] limite de modulos atingido\n");
        return;
    }
    g_mods[g_mods_n] = *m;
    g_exports[g_mods_n] = nullptr;
    g_export_counts[g_mods_n] = 0;
    g_mods_n++;
}

void sb_hle_set_exports(int mod_idx, const sb_export_t* exps, int n) {
    if (mod_idx < 0 || mod_idx >= g_mods_n) return;
    g_exports[mod_idx] = exps;
    g_export_counts[mod_idx] = n;
}

sb_module_t* sb_hle_module(int i) {
    return (i >= 0 && i < g_mods_n) ? &g_mods[i] : nullptr;
}

// ---- resolução ------------------------------------------------------------------
static uint64_t resolve_local(const char* dll, const char* name) {
    for (int i = 0; i < g_mods_n; ++i) {
        if (dll && strcmp(g_mods[i].dll_name, dll) != 0) continue;
        const sb_export_t* ex = g_exports[i];
        for (int k = 0; k < g_export_counts[i]; ++k)
            if (strcmp(ex[k].name, name) == 0) return ex[k].va;
    }
    return 0;
}

uint64_t sb_hle_slot_addr(sb_module_t* m, const char* dll, const char* name) {
    uint64_t local = resolve_local(dll, name);
    if (local) return local;                       // função levantada de outro modulo
    for (uint32_t i = 0; i < m->import_count; ++i)
        if (strcmp(m->imports[i].name, name) == 0)
            return m->hle_base + (uint64_t)i * m->hle_stride;
    return 0;
}

// ---- linking dinâmico (LoadLibrary/GetProcAddress do guest) -----------------
// Handle sintético: HLE_DYNMOD_BASE + indice do modulo.
#define DYNMOD_BASE 0x0000500000000000ull

uint64_t sb_hle_module_handle(const char* dll_name) {
    for (int i = 0; i < g_mods_n; ++i)
        if (strcmp(g_mods[i].dll_name, dll_name) == 0)
            return DYNMOD_BASE + (uint64_t)i;
    return 0;
}

uint64_t sb_hle_get_proc(uint64_t hmodule, const char* name) {
    if (hmodule < DYNMOD_BASE || hmodule >= DYNMOD_BASE + (uint64_t)g_mods_n) {
        // módulo desconhecido (DLL do sistema): tenta os shims
        return hle_dyn_addr(name);
    }
    int idx = (int)(hmodule - DYNMOD_BASE);
    const sb_export_t* ex = g_exports[idx];
    for (int k = 0; k < g_export_counts[idx]; ++k)
        if (strcmp(ex[k].name, name) == 0) return ex[k].va;   // função levantada!
    return hle_dyn_addr(name);
}

// ---- dispatch ----------------------------------------------------------------------
static int  g_trace = -1;

void dispatch(CPU* c, uint64_t addr) {
    if (g_trace < 0) {
        const char* e = getenv("SB_TRACE");
        g_trace = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    g_current_cpu = c;

    // 1. faixa HLE de cada modulo (shims do host)
    for (int mi = 0; mi < g_mods_n; ++mi) {
        sb_module_t* m = &g_mods[mi];
        uint64_t end = m->hle_base + (uint64_t)m->import_count * m->hle_stride;
        if (addr >= m->hle_base && addr < end) {
            uint32_t i = (uint32_t)((addr - m->hle_base) / m->hle_stride);
            const char* api = m->imports[i].name;
            shim_fn fn = hle_lookup(api);
            if (g_trace)
                fprintf(stderr, "[t] %s!%s\n", m->imports[i].dll, api);
            if (!fn) {
                printf("[hle] shim NAO implementado: %s — abortando\n", api);
                abort();
            }
            fn(c);
            return;
        }
    }

    // 2. DYN (GetProcAddress): base fixo do host
    if (addr >= 0xE5E40000ull && addr < 0xE5E40000ull + 512ull * 8ull) {
        uint32_t i = (uint32_t)((addr - 0xE5E40000ull) / 8);
        // resolve pelo registro: o indice i é o do registro (hle_dyn_addr)
        extern shim_fn hle_dyn_by_index(uint32_t);
        shim_fn fn = hle_dyn_by_index(i);
        if (g_trace) fprintf(stderr, "[t] dyn #%u\n", i);
        if (!fn) { printf("[hle] dyn #%u sem shim — abortando\n", i); abort(); }
        fn(c);
        return;
    }

    // 3. tabelas de funcoes levantadas (qualquer modulo)
    for (int mi = 0; mi < g_mods_n; ++mi) {
        const sb_entry_t* t = g_mods[mi].table;
        int lo = 0, hi = (int)g_mods[mi].table_count - 1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            uint64_t va = t[mid].va;
            if (va == addr) { t[mid].fn(c); return; }
            if (va < addr) lo = mid + 1; else hi = mid - 1;
        }
    }

    printf("[hle] endereco 0x%llx sem funcao levantada — abortando\n",
           (unsigned long long)addr);
    if (c)
        printf("[hle] chamador (ret em [rsp]) = 0x%llx\n",
               (unsigned long long)rd64(c->rsp));
    abort();
}

void dispatch_jmp(CPU* c, uint64_t target) { dispatch(c, target); }

// ---- EH: profundidade da pilha de excecoes C++ do guest ----------------------
static __thread int g_cs_depth = 0;
int  es3_cs_depth_get(void) { return g_cs_depth; }
void es3_cs_depth_set(int d) { g_cs_depth = d; }

// ---- GetProcAddress dinamico ----------------------------------------------------
shim_fn hle_dyn_by_index(uint32_t i) {
    if ((int)i < g_registry_n) return g_registry[i].fn;
    return nullptr;
}

uint64_t hle_dyn_addr(const char* name) {
    if (!name) return 0;
    for (int i = 0; i < g_registry_n; ++i)
        if (strcmp(g_registry[i].api, name) == 0)
            return 0xE5E40000ull + (uint64_t)i * 8;
    return 0;
}

// ---- init ---------------------------------------------------------------------------
int64_t g_image_delta = 0;
// Deltas por modulo (cada TU compila com -Dg_image_delta=<PFX>_g_image_delta,
// entao modulos que precisam de relocacao tem o seu proprio simbolo).
int64_t UP_g_image_delta = 0;
int64_t GA_g_image_delta = 0;
int64_t BL_g_image_delta = 0;

void sb_set_module_delta(int mod_idx, int64_t delta) {
    switch (mod_idx) {
        case 1: UP_g_image_delta = delta; break;
        case 2: GA_g_image_delta = delta; break;
        case 3: BL_g_image_delta = delta; break;
        default: g_image_delta = delta; break;
    }
}

CPU* g_current_cpu = nullptr;

void sb_hle_init(void) {
    sb_hle_register(g_registry, &g_registry_n);
    printf("[hle] %d shims, %d modulos\n", g_registry_n, g_mods_n);
    for (int i = 0; i < g_mods_n; ++i)
        printf("[hle]   %s: %u imports, %u funcoes, base 0x%llx, hle 0x%llx\n",
               g_mods[i].dll_name, g_mods[i].import_count,
               g_mods[i].table_count,
               (unsigned long long)g_mods[i].image_base,
               (unsigned long long)g_mods[i].hle_base);
}
