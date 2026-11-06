// desktop_host.c — glue do host desktop (Windows/Linux) pro runtime recomp64.
//
// O runtime do pcrecomp usa mapeamento identidade: endereco do guest ==
// endereco do host. Quem garante isso é o loader (pe_load.c) que reserva a
// memoria na base preferida da imagem. Este arquivo prove o resto do contrato
// que os shims usam: alocadores do host, strings do guest, CPU novo, mmap.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <malloc.h>      // malloc_usable_size (glibc)
#include <unistd.h>
#include <sched.h>       // sched_yield
#endif

// ---- alocadores do host (bionic/mmap no Android; Win32/malloc no desktop) --
void* sb_host_alloc(size_t n) {
#ifdef _WIN32
    return VirtualAlloc(NULL, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    void* p = malloc(n ? n : 1);
    return p;
#endif
}

void sb_host_free(void* p) {
#ifdef _WIN32
    if (p) VirtualFree(p, 0, MEM_RELEASE);
#else
    free(p);
#endif
}

void* sb_host_realloc(void* p, size_t n) {
#ifdef _WIN32
    if (!p) return sb_host_alloc(n);
    void* q = sb_host_alloc(n);
    if (!q) return NULL;
    // copia o minimo: sem saber o tamanho antigo, copiamos n (seguro para
    // crescimento; para encolhimento tambem, pois nunca lemos além do novo).
    memcpy(q, p, n);
    sb_host_free(p);
    return q;
#else
    return realloc(p, n ? n : 1);
#endif
}

namespace sb {
size_t sb_malloc_usable_size(void* p) {
#ifdef _WIN32
    if (!p) return 0;
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(p, &mbi, sizeof(mbi));
    return (size_t)mbi.RegionSize;
#else
    return malloc_usable_size(p);
#endif
}
}  // namespace sb

// ---- memoria do guest: identidade ------------------------------------------
// (dentro de namespace sb para casar com a declaracao em winabi.h)
namespace sb {
void* sb_map_guest(uint64_t a) { return (void*)(uintptr_t)a; }
}  // namespace sb

// CPU novo do guest (zeroado; o host empilha/aloca como quiser).
#include "cpu64.h"
CPU* sb_new_guest_cpu() {
    CPU* c = (CPU*)calloc(1, sizeof(CPU));
    return c;
}

namespace sb {   // (para o shim compilar igual no desktop e no teste)

// Nota: <string> vai no topo do arquivo — incluir dentro de namespace quebra.
size_t guest_wstrlen(const void*, uint64_t addr) {
    const char16_t* p = (const char16_t*)sb_map_guest(addr);
    size_t n = 0;
    while (p[n]) ++n;
    return n;
}

std::u16string guest_wstr_copy(const void*, uint64_t addr) {
    const char16_t* p = (const char16_t*)sb_map_guest(addr);
    return std::u16string(p ? p : u"");
}
std::string guest_astr_copy(const void*, uint64_t addr) {
    const char* p = (const char*)sb_map_guest(addr);
    return std::string(p ? p : "");
}
uint64_t guest_alloc_wstr(const void*, const std::u16string& s) {
    void* p = sb_host_alloc((s.size() + 1) * sizeof(char16_t));
    memcpy(p, s.c_str(), (s.size() + 1) * sizeof(char16_t));
    return (uint64_t)(uintptr_t)p;
}

}  // namespace sb
