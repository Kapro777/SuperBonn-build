// desktop_main.c — bring-up do guest no host desktop (sem device).
//
// Uso: desktop_main <caminho-do-PE> [entry_rva_hex]
//
// Fluxo: carrega o PE (pe_load.c) -> hle_init (preenche a IAT com sentinelas)
// -> monta um CPU com pilha propria -> dispatch(entry). ExitProcess do guest
// chama nosso shim e sai do processo.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "cpu64.h"

uint64_t sb_load_pe(const char* path, uint64_t* out_base, uint32_t* out_size);
void hle_init(void);

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("uso: %s <PE>\n", argv[0]);
        return 2;
    }
    setvbuf(stdout, NULL, _IONBF, 0);

#ifdef _WIN32
    // Handler de excecao: imprime o RIP do guest quando o host faultar.
    extern CPU* g_current_cpu;
    AddVectoredExceptionHandler(1, [](PEXCEPTION_POINTERS ep) -> LONG {
        CPU* c = g_current_cpu;
        printf("[fault] codigo 0x%lx em host-rip 0x%llx | guest rip 0x%llx "
               "rsp 0x%llx rax 0x%llx\n",
               (unsigned long)ep->ExceptionRecord->ExceptionCode,
               (unsigned long long)ep->ContextRecord->Rip,
               c ? (unsigned long long)c->rip : 0,
               c ? (unsigned long long)c->rsp : 0,
               c ? (unsigned long long)c->rax : 0);
        fflush(stdout);
        return EXCEPTION_CONTINUE_SEARCH;
    });
#endif

    uint64_t base = 0;
    uint32_t size = 0;
    uint64_t entry = sb_load_pe(argv[1], &base, &size);
    if (!entry) {
        printf("[main] loader falhou\n");
        return 1;
    }

    hle_init();

    // Pilha do guest: 8 MB, topo alinhado em 16.
    uint64_t stack_size = 8u << 20;
    void* stack = calloc(1, stack_size);
    if (!stack) { printf("[main] sem pilha\n"); return 1; }

    CPU* c = (CPU*)calloc(1, sizeof(CPU));
    if (!c) { printf("[main] sem CPU\n"); return 1; }
    c->rsp = (uint64_t)(uintptr_t)stack + stack_size - 64;
    c->rsp &= ~15ull;

    printf("[main] entrando no guest em 0x%llx\n", (unsigned long long)entry);
    dispatch(c, entry);
    printf("[main] guest retornou (rax=0x%llx)\n", (unsigned long long)c->rax);
    return (int)(c->rax & 0xFF);
}
