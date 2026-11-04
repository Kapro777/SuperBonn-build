// test_file.cpp — teste local dos shims de arquivo (host mingw).
// Cria/le/escreve um arquivo real no host e valida o comportamento do guest.
#include "../kernel32_shims.h"

#include <cstdio>
#include <cstring>

using namespace sb;

static int g_fail = 0;
#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (!(cond)) { printf("FALHOU: %s\n", msg); ++g_fail; }             \
        else { printf("ok: %s\n", msg); }                                   \
    } while (0)

// Escreve uma string UTF-16 no buffer do "guest" (mock: memoria identidade).
static uint64_t guest_wstring(const char* s) {
    size_t n = strlen(s);
    auto* p = (char*)malloc((n + 1) * 2);
    for (size_t i = 0; i < n; ++i) { p[i * 2] = s[i]; p[i * 2 + 1] = 0; }
    p[n * 2] = 0; p[n * 2 + 1] = 0;
    return reinterpret_cast<uint64_t>(p);
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);

    const char* path = "sb_shim_test_file.bin";
    uint64_t gpath = guest_wstring(path);

    // Pilha do guest para argumentos de stack do Win64:
    // [rsp]=retorno, [rsp+8..40)=shadow, [rsp+48)=5o arg, [rsp+56)=6o.
    static uint64_t guest_stack[16] = {0};

    // --- CreateFileW + WriteFile ---
    CPU c{};
    c.rsp = reinterpret_cast<uint64_t>(guest_stack);
    guest_stack[6] = CREATE_ALWAYS;       // 5o arg: dwCreationDisposition
    c.rcx = gpath;                        // lpFileName
    c.rdx = GENERIC_READ | GENERIC_WRITE; // dwDesiredAccess
    c.r8 = 0;                             // dwShareMode
    c.r9 = 0;                             // lpSecurityAttributes
    sb_shim_CreateFileW(&c);
    uint64_t h = c.rax;
    CHECK(h != (uint64_t)-1, "CreateFileW abre arquivo");

    const char* data = "SUPERBONN-SHIM-FILE-TEST";
    static char guest_buf[64];
    strcpy(guest_buf, data);
    c = CPU{};
    c.rcx = h;
    c.rdx = strlen(data);
    c.r8 = reinterpret_cast<uint64_t>(guest_buf);
    sb_shim_WriteFile(&c);
    CHECK(c.rax == 1, "WriteFile escreve");

    c = CPU{};
    c.rcx = h;
    sb_shim_GetFileSize(&c);
    CHECK((uint32_t)c.rax == strlen(data), "GetFileSize = tamanho escrito");

    c = CPU{}; c.rcx = h;
    sb_shim_CloseHandle(&c);
    CHECK(c.rax == 1, "CloseHandle fecha");

    // --- leitura ---
    c = CPU{};
    c.rsp = reinterpret_cast<uint64_t>(guest_stack);
    guest_stack[6] = OPEN_EXISTING;
    c.rcx = gpath; c.rdx = GENERIC_READ; c.r8 = 0; c.r9 = 0;
    sb_shim_CreateFileW(&c);
    uint64_t h2 = c.rax;
    CHECK(h2 != (uint64_t)-1, "CreateFileW reabre para leitura");

    static char readbuf[64] = {0};
    c = CPU{};
    c.rcx = h2; c.rdx = sizeof(readbuf);
    c.r8 = reinterpret_cast<uint64_t>(readbuf);
    sb_shim_ReadFile(&c);
    CHECK(c.rax == 1, "ReadFile le");
    CHECK(strcmp(readbuf, data) == 0, "conteudo lido confere");

    // --- SetFilePointer(SEEK_SET 0) + reler ---
    c = CPU{}; c.rcx = h2; c.rdx = 0; c.r9 = SEEK_SET;
    sb_shim_SetFilePointer(&c);
    CHECK((uint32_t)c.rax == 0, "SetFilePointer para 0");

    c = CPU{};
    c.rcx = h2; c.rdx = 5; c.r8 = reinterpret_cast<uint64_t>(readbuf);
    sb_shim_ReadFile(&c);
    CHECK(strncmp(readbuf, "SUPER", 5) == 0, "le 5 bytes do inicio");

    c = CPU{}; c.rcx = h2;
    sb_shim_CloseHandle(&c);

    // --- FindFirst sobre o proprio diretorio ---
    // FindNext devolve nomes em UTF-16 (WIN32_FIND_DATAW): comparar como wide.
    uint64_t gdir = guest_wstring("./");
    c = CPU{}; c.rcx = gdir;
    sb_shim_FindFirstFileExW(&c);
    uint64_t hf = c.rax;
    CHECK(hf != (uint64_t)-1, "FindFirstFileExW abre diretorio");

    static char16_t wname[64];
    auto wide_equals_ascii = [](const char16_t* w, const char* a) {
        for (int i = 0; a[i]; ++i) if (w[i] != (char16_t)(uint8_t)a[i]) return false;
        return true;
    };

    c = CPU{}; c.rcx = hf; c.rdx = reinterpret_cast<uint64_t>(wname);
    bool found_self = false;
    while (sb_shim_FindNextFileW(&c), c.rax == 1) {
        if (wide_equals_ascii(wname, path)) { found_self = true; break; }
        c.rcx = hf; c.rdx = reinterpret_cast<uint64_t>(wname);
    }
    CHECK(found_self, "FindNext encontra o arquivo criado");

    c = CPU{}; c.rcx = hf;
    sb_shim_FindClose(&c);
    CHECK(c.rax == 1, "FindClose fecha");

    remove(path);
    printf("\n%d FALHAS\n", g_fail);
    fflush(stdout);
    _exit(g_fail ? 1 : 0);
}
