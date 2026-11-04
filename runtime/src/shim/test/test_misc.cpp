// test_misc.cpp — teste local dos shims de tempo/strings/modulo/console.
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

static uint64_t g_stack[16] = {0};

static int wstr_cmp(const char16_t* a, const char16_t* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return (int)*a - (int)*b;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);

    // --- QueryPerformanceCounter cresce ---
    static uint64_t ctr1, ctr2;
    CPU c{};
    c.rcx = reinterpret_cast<uint64_t>(&ctr1);
    sb_shim_QueryPerformanceCounter(&c);
    usleep(2000);
    c = CPU{}; c.rcx = reinterpret_cast<uint64_t>(&ctr2);
    sb_shim_QueryPerformanceCounter(&c);
    CHECK(ctr2 > ctr1, "QueryPerformanceCounter avanca");

    c = CPU{}; c.rcx = reinterpret_cast<uint64_t>(g_stack);
    sb_shim_QueryPerformanceFrequency(&c);
    CHECK(g_stack[0] == 1000000000ull, "Frequency = 1 GHz");

    // --- GetTickCount64 ---
    c = CPU{};
    sb_shim_GetTickCount64(&c);
    CHECK(c.rax > 0, "GetTickCount64 > 0");

    // --- MultiByteToWideChar / WideCharToMultiByte round-trip ---
    static const char* src = "Megabonk";
    static char16_t wide[32];
    static char back[32];
    c = CPU{};
    c.rsp = reinterpret_cast<uint64_t>(g_stack);
    c.rcx = 65001;                              // CP_UTF8
    c.rdx = 0;
    c.r8 = reinterpret_cast<uint64_t>(src);
    c.r9 = -1;                                  // strlen
    g_stack[6] = reinterpret_cast<uint64_t>(wide);   // 5o arg (stack[0])
    g_stack[7] = 32;                                 // 6o arg (stack[1])
    sb_shim_MultiByteToWideChar(&c);
    CHECK(wstr_cmp(wide, u"Megabonk") == 0, "MultiByteToWideChar -> UTF-16");

    c = CPU{};
    c.rsp = reinterpret_cast<uint64_t>(g_stack);
    c.rcx = 65001; c.rdx = 0;
    c.r8 = reinterpret_cast<uint64_t>(wide);
    c.r9 = -1;
    g_stack[6] = reinterpret_cast<uint64_t>(back);
    g_stack[7] = 32;
    sb_shim_WideCharToMultiByte(&c);
    CHECK(strcmp(back, "Megabonk") == 0, "WideCharToMultiByte -> UTF-8");

    // --- GetLastError/SetLastError ---
    c = CPU{}; c.rcx = 1234;
    sb_shim_SetLastError(&c);
    c = CPU{};
    sb_shim_GetLastError(&c);
    CHECK(c.rax == 1234, "Set/GetLastError");

    // --- GetCommandLineW ---
    c = CPU{};
    sb_shim_GetCommandLineW(&c);
    CHECK(wstr_cmp((const char16_t*)c.rax, u"Megabonk.exe") == 0,
          "GetCommandLineW = nome do executavel");

    // --- IsDebuggerPresent = 0 ---
    c = CPU{};
    sb_shim_IsDebuggerPresent(&c);
    CHECK(c.rax == 0, "IsDebuggerPresent = 0");

    // --- GetVersion (Windows 7 SP1) ---
    c = CPU{};
    sb_shim_GetVersion(&c);
    CHECK((c.rax & 0xFF) == 6, "GetVersion: major 6");
    CHECK(((c.rax >> 8) & 0xFF) == 1, "GetVersion: minor 1");
    CHECK(((c.rax >> 16) & 0xFFFF) == 7601, "GetVersion: build 7601");

    printf("\n%d FALHAS\n", g_fail);
    fflush(stdout);
    _exit(g_fail ? 1 : 0);
}
