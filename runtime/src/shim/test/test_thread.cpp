// test_thread.cpp — teste local dos shims de thread/sincronizacao.
// Cria uma thread do guest (funcao do host via mock do dispatch), espera via
// WaitForSingleObject e testa CriticalSection/Event.
#include "../kernel32_shims.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

using namespace sb;

static int g_fail = 0;
#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (!(cond)) { printf("FALHOU: %s\n", msg); ++g_fail; }             \
        else { printf("ok: %s\n", msg); }                                   \
    } while (0)

// "Codigo do guest": uma funcao host que o mock do dispatch chama.
static volatile uint64_t g_thread_result = 0;
static void guest_thread_body(CPU* c) {
    g_thread_result = c->rcx * 2;   // ecoa o parametro
    c->rax = 77;
}

static uint64_t guest_stack[16] = {0};

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);

    // --- CreateThread + WaitForSingleObject (join) ---
    CPU c{};
    c.rsp = reinterpret_cast<uint64_t>(guest_stack);
    c.rcx = 0;                                  // lpThreadAttributes
    c.rdx = 0;                                  // dwStackSize
    c.r8 = reinterpret_cast<uint64_t>(&guest_thread_body);  // lpStartAddress
    c.r9 = 21;                                  // lpParameter
    sb_shim_CreateThread(&c);
    uint64_t h = c.rax;
    CHECK(h != 0, "CreateThread devolve handle");

    c = CPU{}; c.rcx = h; c.rdx = INFINITE;
    sb_shim_WaitForSingleObject(&c);
    CHECK(c.rax == WAIT_OBJECT_0, "WaitForSingleObject espera a thread");
    usleep(20000);
    CHECK(g_thread_result == 42, "thread do guest rodou e ecoou 21*2");

    // --- CriticalSection ---
    static uint64_t cs[5];
    c = CPU{}; c.rcx = reinterpret_cast<uint64_t>(cs);
    sb_shim_InitializeCriticalSection(&c);
    CHECK(rd64(reinterpret_cast<uint64_t>(cs) + 24) != 0,
          "InitializeCriticalSection cria lock do host");
    c = CPU{}; c.rcx = reinterpret_cast<uint64_t>(cs);
    sb_shim_EnterCriticalSection(&c);
    CHECK(sb_shim_TryEnterCriticalSection(&c) == 1,
          "TryEnter (recursivo) entra de novo");
    c = CPU{}; c.rcx = reinterpret_cast<uint64_t>(cs);
    sb_shim_LeaveCriticalSection(&c);
    c = CPU{}; c.rcx = reinterpret_cast<uint64_t>(cs);
    sb_shim_DeleteCriticalSection(&c);
    CHECK(rd64(reinterpret_cast<uint64_t>(cs) + 24) == 0,
          "DeleteCriticalSection limpa");

    // --- Event (auto-reset) ---
    c = CPU{}; c.rcx = 0; c.rdx = 0; c.r8 = 0; c.r9 = 0;   // auto-reset, nao-sinalizado
    sb_shim_CreateEventW(&c);
    uint64_t ev = c.rax;
    CHECK(ev != 0, "CreateEventW devolve handle");
    c = CPU{}; c.rcx = ev;
    sb_shim_SetEvent(&c);
    CHECK(c.rax == 1, "SetEvent ok");
    c = CPU{}; c.rcx = ev;
    sb_shim_WaitForSingleObject(&c);
    CHECK(c.rax == WAIT_OBJECT_0, "espera consome o auto-reset");

    // --- Semaforo ---
    c = CPU{}; c.rsp = reinterpret_cast<uint64_t>(guest_stack);
    guest_stack[0] = 1;                          // initial = 1 (5o arg no stack)
    c.rcx = 0; c.rdx = 1; c.r8 = 10;
    sb_shim_CreateSemaphoreExW(&c);
    uint64_t sem = c.rax;
    CHECK(sem != 0, "CreateSemaphoreExW devolve handle");
    c = CPU{}; c.rcx = sem; c.rdx = 1;
    sb_shim_ReleaseSemaphore(&c);
    CHECK(c.rax == 1, "ReleaseSemaphore ok");

    printf("\n%d FALHAS\n", g_fail);
    fflush(stdout);
    _exit(g_fail ? 1 : 0);
}
