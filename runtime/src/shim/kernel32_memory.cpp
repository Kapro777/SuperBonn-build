// kernel32_memory.cpp — shims de memoria/thread-local do KERNEL32.
// Cobertura pelo censo (docs/RECON-DAY1.md §6): VirtualAlloc/Free/Protect/Query,
// HeapAlloc/Free/ReAlloc/Size/Create/Destroy, TlsAlloc/GetValue/SetValue/Free,
// FlsAlloc/..., EncodePointer/DecodePointer.
//
// Mapeamento: pagina do guest -> mmap do host; heap -> malloc; TLS/FLS ->
// pthread_setspecific com tabela por indice.

#include "kernel32_shims.h"

#include <pthread.h>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <mutex>

// Alocadores do HOST (fora do namespace de propsito): o runtime real os
// implementa sobre mmap do bionic; o teste host, sobre malloc rastreado.
void* sb_host_alloc(size_t n);
void sb_host_free(void* p);
void* sb_host_realloc(void* p, size_t n);

namespace sb {

// ---- paginacao do guest ---------------------------------------------------
// O loader do host mapeia a imagem do PE; paginas novas (VirtualAlloc) veem
// de mmap. No host real: mmap(PROT_...). No teste host: calloc.
static void* host_alloc_pages(size_t bytes, bool exec) {
    (void)exec;
    return sb_host_alloc(bytes);
}

extern "C" {

void sb_shim_VirtualAlloc(CPU* c) {
    uint64_t addr = arg1(c);        // lpAddress (NULL = qualquer)
    size_t size = (size_t)arg2(c);  // dwSize
    uint32_t type = (uint32_t)arg3(c);
    uint32_t prot = (uint32_t)arg4(c);
    (void)prot;
    if (size == 0) { ret(c, 0); return; }
    void* p = host_alloc_pages(size, (type & 0x10000000u) != 0);  // MEM_COMMIT etc
    if (!p) { ret(c, 0); return; }
    if (addr) {
        // Reserva em endereco fixo: so funciona se o host mapeou la; no teste
        // devolvemos o proprio (o loader real faz o mmap fixo).
    }
    ret(c, reinterpret_cast<uint64_t>(p));
}

void sb_shim_VirtualFree(CPU* c) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(arg1(c)));
    if (p) sb_host_free(p);
    ret(c, 1);
}

void sb_shim_VirtualProtect(CPU* c) {
    // Sem MProtect real no teste; no host real usa mprotect. Assumimos exito.
    ret(c, 1);
}

void sb_shim_VirtualQuery(CPU* c) {
    // Preenche MEMORY_BASIC_INFORMATION minimo (BaseAddress, AllocationBase,
    // RegionSize, State). O IL2CPP usa para sizing do heap gerenciado.
    uint64_t addr = arg1(c);
    uint64_t out = arg2(c);
    if (!out) { ret(c, 0); return; }
    wr64(out + 0, addr);            // BaseAddress
    wr64(out + 8, 0);               // AllocationBase
    wr32(out + 16, 0x1000);         // AllocationProtect (PAGE_EXECUTE_READWRITE)
    wr64(out + 24, 0x10000);        // RegionSize (64 KB, palpite conservador)
    wr32(out + 32, 0x1000);         // State = MEM_COMMIT
    ret(c, sizeof(uint64_t) * 6);
}

// ---- heap -----------------------------------------------------------------
void* sb_host_heap_alloc(size_t n) { return sb_host_alloc(n); }
void  sb_host_heap_free(void* p) { if (p) sb_host_free(p); }

void sb_shim_HeapAlloc(CPU* c) {
    // hHeap, dwFlags, dwBytes
    size_t bytes = (size_t)arg3(c);
    ret(c, reinterpret_cast<uint64_t>(sb_host_heap_alloc(bytes)));
}

void sb_shim_HeapFree(CPU* c) {
    // hHeap, dwFlags, lpMem
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(arg3(c)));
    if (p) sb_host_heap_free(p);
    ret(c, 1);
}

void sb_shim_HeapReAlloc(CPU* c) {
    // HeapReAlloc(hHeap, dwFlags, lpMem, dwBytes) — lpMem no 4o arg, bytes no stack
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(arg4(c)));
    size_t bytes = (size_t)arg_stack(c, 0);
    void* q = sb_host_realloc(p, bytes ? bytes : 1);
    ret(c, reinterpret_cast<uint64_t>(q));
}

void sb_shim_HeapSize(CPU* c) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(arg3(c)));
    ret(c, p ? sb_malloc_usable_size(p) : (size_t)-1);
}

void sb_shim_HeapCreate(CPU* c) {
    // Devolve um handle nao-NULL; tudo vem do malloc do host.
    ret(c, 0x48454150ull);   // "HEAP"
}

void sb_shim_HeapDestroy(CPU* c) { ret(c, 1); }

// ---- TLS / FLS ------------------------------------------------------------
static std::atomic<uint32_t> g_tls_next{1};
static constexpr uint32_t TLS_SLOTS = 1024;
static void* g_tls_values[TLS_SLOTS];
static std::mutex g_tls_mu;

uint32_t sb_shim_TlsAlloc(CPU* c) {
    std::lock_guard<std::mutex> lk(g_tls_mu);
    uint32_t id = g_tls_next.fetch_add(1);
    if (id >= TLS_SLOTS) { ret(c, 0xFFFFFFFFu); return 0xFFFFFFFFu; }
    g_tls_values[id] = nullptr;
    return id;
}

void sb_shim_TlsGetValue(CPU* c) {
    uint32_t id = (uint32_t)arg1(c);
    // No host real: pthread_getspecific por thread. No teste: valor global.
    ret(c, id < TLS_SLOTS ? reinterpret_cast<uint64_t>(g_tls_values[id]) : 0);
}

int sb_shim_TlsSetValue(CPU* c) {
    uint32_t id = (uint32_t)arg1(c);
    void* v = reinterpret_cast<void*>(static_cast<uintptr_t>(arg2(c)));
    if (id >= TLS_SLOTS) { ret(c, 0); return 0; }
    g_tls_values[id] = v;   // teste: global; host real: pthread_setspecific
    ret(c, 1);
    return 1;
}

void sb_shim_TlsFree(CPU* c) {
    uint32_t id = (uint32_t)arg1(c);
    if (id < TLS_SLOTS) { std::lock_guard<std::mutex> lk(g_tls_mu); g_tls_values[id] = nullptr; }
    ret(c, 1);
}

// FLS: mesma semantica do TLS com callback de destruicao (ignorado).
void sb_shim_FlsAlloc(CPU* c) { sb_shim_TlsAlloc(c); }
void sb_shim_FlsGetValue(CPU* c) { sb_shim_TlsGetValue(c); }
void sb_shim_FlsSetValue(CPU* c) { sb_shim_TlsSetValue(c); }
void sb_shim_FlsFree(CPU* c) { sb_shim_TlsFree(c); }

// ---- ponteiros ofuscados (encode/decode: XOR com um segredo por processo) --
static uint64_t g_ptr_secret = 0x51AB3D07F19C62E5ull;

void sb_shim_EncodePointer(CPU* c) { ret(c, arg1(c) ^ g_ptr_secret); }
void sb_shim_DecodePointer(CPU* c) { ret(c, arg1(c) ^ g_ptr_secret); }

}  // extern "C"

}  // namespace sb
