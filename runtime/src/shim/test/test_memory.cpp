// test_memory.cpp — teste local (host mingw) dos shims de memoria/TLS.
// Roda sem o jogo e sem o runtime real: prova que os shims dao o resultado
// esperado do ponto de vista do guest. Build: scripts/rt/test-shims.sh
#include "../kernel32_shims.h"

#include <cstdio>
#include <cstring>

using namespace sb;

static int g_fail = 0;
#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (!(cond)) { printf("FALHOU: %s\n", msg); ++g_fail; }           \
        else { printf("ok: %s\n", msg); }                                 \
    } while (0)

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    CPU c{};

    // --- VirtualAlloc / VirtualQuery ---
    // arg1..4: rcx=lpAddress, rdx=dwSize, r8=type, r9=protect
    c = CPU{}; c.rcx = 0; c.rdx = 0x10000; c.r8 = 0x1000 /*MEM_COMMIT*/;
    c.r9 = 0x40 /*PAGE_EXECUTE_READWRITE*/;
    sb_shim_VirtualAlloc(&c);
    uint64_t page = c.rax;
    CHECK(page != 0, "VirtualAlloc devolve endereco");

    c = CPU{}; c.rcx = page; c.rdx = page + 0x2000;
    sb_shim_VirtualQuery(&c);
    CHECK(c.rax != 0, "VirtualQuery devolve bytes escritos");
    CHECK(rd64(page + 0x2000 + 0) == page, "VirtualQuery.BaseAddress");

    // --- Heap ---   (1o=rcx, 2o=rdx, 3o=r8, 4o=r9)
    c = CPU{}; c.rcx = 0x48454150ull; c.rdx = 0; c.r8 = 0x1234;
    sb_shim_HeapAlloc(&c);
    uint64_t heap = c.rax;
    CHECK(heap != 0, "HeapAlloc devolve ponteiro");
    memset(reinterpret_cast<void*>(heap), 0xAB, 0x1234);

    c = CPU{}; c.rcx = 0x48454150ull; c.rdx = 0; c.r8 = heap;
    sb_shim_HeapSize(&c);
    CHECK(c.rax == 0x1234, "HeapSize confere o tamanho");

    c = CPU{}; c.rcx = 0x48454150ull; c.rdx = 0; c.r8 = heap;
    sb_shim_HeapFree(&c);
    CHECK(c.rax == 1, "HeapFree ok");

    // --- TLS ---
    c = CPU{}; sb_shim_TlsAlloc(&c);
    uint32_t idx = (uint32_t)c.rax;
    CHECK(idx != 0xFFFFFFFFu, "TlsAlloc devolve indice");

    c = CPU{}; c.rcx = idx; c.rdx = 0xDEADBEEF;
    CHECK(sb_shim_TlsSetValue(&c) == 1, "TlsSetValue ok");

    c = CPU{}; c.rcx = idx;
    sb_shim_TlsGetValue(&c);
    CHECK(c.rax == 0xDEADBEEF, "TlsGetValue devolve o valor");

    // --- Encode/DecodePointer ---
    c = CPU{}; c.rcx = 0x140001000;
    sb_shim_EncodePointer(&c);
    uint64_t enc = c.rax;
    c = CPU{}; c.rcx = enc;
    sb_shim_DecodePointer(&c);
    CHECK(c.rax == 0x140001000, "Encode/DecodePointer round-trip");

    printf("\n%d FALHAS\n", g_fail);
    fflush(stdout);
    _exit(g_fail ? 1 : 0);   // teste: evita crash em destrutores estaticos do mingw
}
