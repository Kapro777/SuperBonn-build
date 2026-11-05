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

extern "C" void sb_hle_register(void* reg, int* n);   // shim_registry.c

// Delta de relocacao da imagem (cpu64.h): 0 no loader de identidade.
int64_t g_image_delta = 0;

// Faixa de enderecos devolvidos por GetProcAddress (resolucao dinamica).
#define DYN_BASE   0xE5E40000ull
#define DYN_STRIDE 8
#define DYN_MAX    512

typedef void (*shim_fn)(CPU*);

typedef struct {
    const char* dll;
    const char* api;
    shim_fn fn;
} shim_ent_t;

static shim_ent_t g_registry[512];
static int g_registry_n = 0;

shim_fn hle_shims[HLE_COUNT];   // preenchido no init
static shim_fn g_dyn[DYN_MAX];  // por indice do registro (GetProcAddress)

static shim_fn hle_lookup(const char* api) {
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

// ---- dispatch ---------------------------------------------------------------
static int  g_trace = -1;      // -1 = nao lido ainda
static int  g_depth = 0;

// CPU atual (para o handler de excecao do host reportar o RIP do guest).
CPU* g_current_cpu = nullptr;

void dispatch(CPU* c, uint64_t addr) {
    g_current_cpu = c;
    if (g_trace < 0) {
        const char* e = getenv("SB_TRACE");
        g_trace = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (g_trace && addr < HLE_BASE) {
        // so endereco, para o log nao virar擎 nem extrato de binario
        fprintf(stderr, "[t] %*s-> 0x%llx\n", g_depth * 2, "",
                (unsigned long long)addr);
        ++g_depth;
    }

    // faixa HLE?
    if (addr >= HLE_BASE && addr < HLE_BASE + (uint64_t)HLE_COUNT * HLE_STRIDE) {
        uint32_t i = (uint32_t)((addr - HLE_BASE) / HLE_STRIDE);
        if (i >= HLE_COUNT) {
            printf("[hle] sentinel %u fora da faixa\n", i);
            abort();
        }
        if (g_trace) {
            fprintf(stderr, "[t] %*simport %s(rcx=0x%llx rdx=0x%llx r8=0x%llx r9=0x%llx)\n",
                    g_depth * 2, "", recomp_imports[i].name,
                    (unsigned long long)c->rcx, (unsigned long long)c->rdx,
                    (unsigned long long)c->r8, (unsigned long long)c->r9);
        }
        hle_shims[i](c);
        if (g_trace) --g_depth;
        return;
    }
    // faixa DYN (GetProcAddress)?
    if (addr >= DYN_BASE && addr < DYN_BASE + (uint64_t)DYN_MAX * DYN_STRIDE) {
        uint32_t i = (uint32_t)((addr - DYN_BASE) / DYN_STRIDE);
        if (i >= DYN_MAX) { abort(); }
        if (g_trace) {
            fprintf(stderr, "[t] %*sdyn %s\n", g_depth * 2, "",
                    g_registry[i].api);
        }
        g_dyn[i](c);
        if (g_trace) --g_depth;
        return;
    }
    // tabela de funcoes levantadas (busca binaria)
    int lo = 0, hi = (int)recomp_dispatch_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint64_t va = recomp_dispatch_table[mid].va;
        if (va == addr) {
            recomp_dispatch_table[mid].fn(c);
            if (g_trace) --g_depth;
            return;
        }
        if (va < addr) lo = mid + 1; else hi = mid - 1;
    }
    printf("[hle] endereco 0x%llx sem funcao levantada — abortando\n",
           (unsigned long long)addr);
    if (g_trace && c) {
        printf("[hle] chamador (ret em [rsp]) = 0x%llx, rsp=0x%llx\n",
               (unsigned long long)rd64(c->rsp),
               (unsigned long long)c->rsp);
    }
    // MODO DEBUG ONLY (SB_TOLERANT=1): segue em frente devolvendo 0, para ver
    // ate onde o guest chegaria. NUNCA no build do device — mascara bug.
    static int tolerant = -1;
    if (tolerant < 0) {
        const char* e = getenv("SB_TOLERANT");
        tolerant = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (tolerant) {
        printf("[hle] (tolerante) seguindo com rax=0\n");
        c->rax = 0;
        return;
    }
    abort();
}

// Tail-jump do guest (jmp [IAT] / switch tables): nao ha stack frame a
// preservar no lado do host — dispatch e suficiente.
void dispatch_jmp(CPU* c, uint64_t target) { dispatch(c, target); }

// ---- EH: profundidade da pilha de excecoes C++ do guest (por thread) --------
// (linkage consistente: tudo compilado como C++, ver build-desktop.ps1)
static __thread int g_cs_depth = 0;
int  es3_cs_depth_get(void) { return g_cs_depth; }
void es3_cs_depth_set(int d) { g_cs_depth = d; }

// ---- GetProcAddress: devolve endereco que o dispatch resolve ---------------
// O guest chama LoadLibraryExW + GetProcAddress para achar funcoes; devolvemos
// DYN_BASE + indice no registro, e o dispatch traduz para o shim.


void hle_init(void) {
    sb_hle_register(g_registry, &g_registry_n);
    printf("[hle] %d shims registrados\n", g_registry_n);
    for (uint32_t i = 0; i < HLE_COUNT; ++i) {
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
    for (int i = 0; i < g_registry_n && i < DYN_MAX; ++i)
        g_dyn[i] = g_registry[i].fn;
    printf("[hle] %u slots de import preenchidos\n", HLE_COUNT);
}

// Chamado pelo shim GetProcAddress: devolve endereco dispatchavel ou 0.
uint64_t hle_dyn_addr(const char* name) {
    if (!name) return 0;
    for (int i = 0; i < g_registry_n && i < DYN_MAX; ++i)
        if (strcmp(g_registry[i].api, name) == 0)
            return DYN_BASE + (uint64_t)i * DYN_STRIDE;
    return 0;
}
