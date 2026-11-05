// kernel32_misc.cpp — tempo, strings, modulos, erros, console, processo.
// Censo (docs/RECON-DAY1.md §6): QueryPerformanceCounter/Frequency,
// GetTickCount64, GetSystemTimeAsFileTime, MultiByteToWideChar,
// WideCharToMultiByte, GetCommandLineA/W, GetModuleHandleW/ExW, GetProcAddress,
// LoadLibraryExW, FreeLibrary, GetModuleFileNameW, GetLastError/SetLastError,
// OutputDebugStringA/W, WriteConsoleW, GetStdHandle, ExitProcess,
// TerminateProcess, GetExitCodeProcess, GetSystemInfo, GetVersion(Ex),
// GetACP/GetOEMCP/GetCPInfo, IsValidCodePage, CompareStringW, GetStartupInfoW,
// GetEnvironmentStringsW, ExpandEnvironmentStringsW, GetComputerNameW,
// GetConsoleMode/SetConsoleMode, GetConsoleCP/OutputCP, IsDebuggerPresent,
// IsProcessorFeaturePresent, GetProcessAffinityMask.
//
// Sem locale de verdade: CP_UTF8 (65001) para MultiByte/WideChar, GetACP=1252.

#include "kernel32_shims.h"

#include <ctime>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#ifndef _WIN32
#include <sys/utsname.h>
#endif

// Resolucao dinamica de imports (multimod.c do host) — escopo global.
uint64_t hle_dyn_addr(const char* name);
uint64_t sb_hle_module_handle(const char* dll_name);
uint64_t sb_hle_get_proc(uint64_t hmodule, const char* name);

namespace sb {

// ---- comparacao UTF-16 -----------------------------------------------------
static int wstr_cmp(const char16_t* a, const char16_t* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return (int)*a - (int)*b;
}

extern "C" {

// ---- tempo ----------------------------------------------------------------
void sb_shim_QueryPerformanceCounter(CPU* c) {
    uint64_t out = arg1(c);
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ns = (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
    // Frequencia de 1 GHz: o Unity converte com a Frequency.
    if (out) wr64(out, ns);
    ret(c, 1);
}

void sb_shim_QueryPerformanceFrequency(CPU* c) {
    uint64_t out = arg1(c);
    if (out) wr64(out, 1000000000ull);     // 1 GHz (ver Counter)
    ret(c, 1);
}

void sb_shim_GetTickCount64(CPU* c) {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ret(c, (uint64_t)ts.tv_sec * 1000ull + ts.tv_nsec / 1000000ull);
}

void sb_shim_GetSystemTimeAsFileTime(CPU* c) {
    uint64_t out = arg1(c);
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    // FILETIME: 100ns desde 1601-01-01. Unix epoch -> 1601: 11644473600 s.
    uint64_t ft = ((uint64_t)ts.tv_sec + 11644473600ull) * 10000000ull
                  + ts.tv_nsec / 100;
    if (out) wr64(out, ft);
}

// ---- conversao de caracteres ------------------------------------------------
static std::u16string utf8_to_utf16(const char* s, size_t n) {
    std::u16string out;
    for (size_t i = 0; i < n;) {
        uint8_t b = (uint8_t)s[i];
        uint32_t cp = 0;
        int extra = 0;
        if (b < 0x80) { cp = b; extra = 0; }
        else if ((b & 0xE0) == 0xC0) { cp = b & 0x1F; extra = 1; }
        else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; extra = 2; }
        else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; extra = 3; }
        else { cp = '?'; extra = 0; }
        for (int k = 0; k < extra && i + 1 + k < n; ++k)
            cp = (cp << 6) | ((uint8_t)s[i + 1 + k] & 0x3F);
        i += 1 + extra;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            out += (char16_t)(0xD800 + (cp >> 10));
            out += (char16_t)(0xDC00 + (cp & 0x3FF));
        } else {
            out += (char16_t)cp;
        }
    }
    return out;
}

static std::string utf16_to_utf8(const char16_t* w, size_t n) {
    std::string out;
    for (size_t i = 0; i < n; ++i) {
        char32_t cp = w[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < n
            && w[i + 1] >= 0xDC00 && w[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (w[i + 1] - 0xDC00);
            ++i;
        }
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

void sb_shim_MultiByteToWideChar(CPU* c) {
    // (cp, flags, mbstr, cbMultiByte, widestr, cchWideChar) — 6 args (2 stack)
    uint64_t src = arg3(c);
    int n = (int)(int32_t)arg4(c);
    uint64_t dst = arg_stack(c, 0);
    int cap = (int)(int32_t)arg_stack(c, 1);
    if (n < 0) n = src ? (int)strlen((const char*)sb_map_guest(src)) : 0;
    const char* s = src ? (const char*)sb_map_guest(src) : "";
    std::u16string w = utf8_to_utf16(s, (size_t)n);
    if (dst) {
        auto* out = (char16_t*)sb_map_guest(dst);
        for (int i = 0; i < (int)w.size() && i < cap; ++i) out[i] = w[i];
        if (cap > (int)w.size()) out[w.size()] = 0;
    }
    ret(c, w.size() + 1);     // bytes escritos/necessarios (chars, como o Win32)
}

void sb_shim_WideCharToMultiByte(CPU* c) {
    uint64_t src = arg3(c);
    int n = (int)(int32_t)arg4(c);
    uint64_t dst = arg_stack(c, 0);
    int cap = (int)(int32_t)arg_stack(c, 1);
    if (n < 0) n = src ? (int)guest_wstrlen(c, src) : 0;
    const auto* w = src ? (const char16_t*)sb_map_guest(src) : u"";
    std::string s = utf16_to_utf8(w, (size_t)n);
    if (dst) {
        auto* out = (char*)sb_map_guest(dst);
        for (int i = 0; i < (int)s.size() && i < cap; ++i) out[i] = s[i];
        if (cap > (int)s.size()) out[s.size()] = 0;
    }
    ret(c, s.size() + 1);
}

// ---- modulos ---------------------------------------------------------------
static std::u16string g_cmdline;      // preenchido pelo host no boot

void sb_shim_GetCommandLineW(CPU* c) {
    ret(c, reinterpret_cast<uint64_t>(g_cmdline.empty() ? u"Megabonk.exe" : g_cmdline.c_str()));
}

void sb_shim_GetCommandLineA(CPU* c) {
    static std::string cmdline;
    cmdline = "Megabonk.exe";
    ret(c, reinterpret_cast<uint64_t>(cmdline.c_str()));
}

void sb_shim_GetModuleHandleW(CPU* c) {
    // NULL ou "UnityPlayer.dll": devolvemos o base da imagem principal.
    // O host registra os bases reais; para o jogo, qualquer valor nao-NULL
    // serve como modulo "carregado".
    (void)c;
    ret(c, 0x180000000ull);           // base do UnityPlayer.dll no guest
}

void sb_shim_GetModuleHandleExW(CPU* c) { sb_shim_GetModuleHandleW(c); }

void sb_shim_GetModuleFileNameW(CPU* c) {
    uint64_t out = arg2(c);
    uint32_t cap = (uint32_t)arg3(c);
    const char* name = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Megabonk\\Megabonk.exe";
    if (out && cap > strlen(name)) {
        auto* w = (char16_t*)sb_map_guest(out);
        for (size_t i = 0; name[i]; ++i) w[i] = (char16_t)(uint8_t)name[i];
        w[strlen(name)] = 0;
    }
    ret(c, strlen(name));
}

void sb_shim_LoadLibraryExW(CPU* c) {
    // (lpLibFileName, hFile, dwFlags) — se for um modulo que levantamos,
    // devolve o handle dele (linking dinamico de verdade).
    uint64_t name_addr = arg1(c);
    uint64_t handle = 0;
    if (name_addr) {
        std::u16string w = guest_wstr_copy(c, name_addr);
        std::string a = utf16_to_utf8(w.data(), w.size());
        handle = sb_hle_module_handle(a.c_str());
        if (handle)
            printf("[guest] LoadLibrary(\"%s\") -> modulo levantado (0x%llx)\n",
                   a.c_str(), (unsigned long long)handle);
    }
    if (!handle) {
        // DLL do sistema (steam_api64, discord...): handle sintetico.
        handle = 0x0000500000000000ull;
    }
    ret(c, handle);
}

void sb_shim_FreeLibrary(CPU* c) { (void)c; ret(c, 1); }

void sb_shim_GetProcAddress(CPU* c) {
    // (hModule, lpProcName) — resolve no modulo levantado ou nos shims.
    uint64_t hmodule = arg1(c);
    uint64_t name_addr = arg2(c);
    if (!name_addr) { ret(c, 0); return; }
    const char* name = reinterpret_cast<const char*>(sb_map_guest(name_addr));
    uint64_t r = sb_hle_get_proc(hmodule, name);
    printf("[guest] GetProcAddress(0x%llx, \"%s\") -> 0x%llx\n",
           (unsigned long long)hmodule, name, (unsigned long long)r);
    ret(c, r);
}

// ---- erro / console --------------------------------------------------------
static thread_local uint32_t g_last_error = 0;

void sb_shim_GetLastError(CPU* c) { ret(c, g_last_error); }
void sb_shim_SetLastError(CPU* c) { g_last_error = (uint32_t)arg1(c); }

void sb_shim_IsDebuggerPresent(CPU* c) { ret(c, 0); }

void sb_shim_OutputDebugStringW(CPU* c) {
    std::u16string w = guest_wstr_copy(c, arg1(c));
    std::string s = utf16_to_utf8(w.data(), w.size());
    printf("[guest] %s\n", s.c_str());
    ret(c, 0);
}

void sb_shim_OutputDebugStringA(CPU* c) {
    std::string s = guest_astr_copy(c, arg1(c));
    printf("[guest] %s\n", s.c_str());
    ret(c, 0);
}

void sb_shim_WriteConsoleW(CPU* c) {
    // (hConsoleOutput, lpBuffer, nChars, lpWritten, reserved)
    const char16_t* w = (const char16_t*)sb_map_guest(arg2(c));
    size_t n = (size_t)arg3(c);
    std::string s = utf16_to_utf8(w, n);
    fwrite(s.data(), 1, s.size(), stdout);
    uint64_t pWritten = arg4(c);
    if (pWritten) wr32(pWritten, (uint32_t)n);
    ret(c, 1);
}

void sb_shim_GetStdHandle(CPU* c) {
    // -10 input, -11 output, -12 error -> handles sinteticos
    int32_t which = (int32_t)arg1(c);
    ret(c, which == -10 ? 0x0000500000000010ull : 0x0000500000000011ull);
}

// ---- processo --------------------------------------------------------------
void sb_shim_ExitProcess(CPU* c) {
    uint32_t code = (uint32_t)arg1(c);
    printf("[SuperBonn] ExitProcess(%u)\n", code);
    fflush(stdout);
    _exit(code);
}

void sb_shim_TerminateProcess(CPU* c) { (void)c; abort(); }
void sb_shim_GetExitCodeProcess(CPU* c) { (void)c; ret(c, 0); }

// ---- sistema ---------------------------------------------------------------
void sb_shim_GetNativeSystemInfo(CPU* c) { sb_shim_GetSystemInfo(c); }

void sb_shim_GetSystemInfo(CPU* c) {
    // SYSTEM_INFO: preenchemos o minimo (page size, #procs, allocationgranularity)
    uint64_t out = arg1(c);
    if (!out) { ret(c, 0); return; }
    for (int i = 0; i < 13; ++i) wr64(out + 8 * i, 0);   // zera a struct
    wr32(out + 4, 0x1000);          // dwPageSize
    wr32(out + 8, 4096);            // lpMinimumApplicationAddress (baixo)
    wr32(out + 12, 0);              // (parte alta)
    wr32(out + 16, 0x7FFEFFFF);     // lpMaximumApplicationAddress
    wr32(out + 20, 0);
    wr32(out + 24, 0x00010000);     // dwAllocationGranularity (64 KB)
    wr32(out + 28, 4);              // dwNumberOfProcessors (conservador)
    ret(c, 0);
}

void sb_shim_GetVersion(CPU* c) {
    // Windows 7 SP1 (o Unity 2022 exige >= 6.1).
    // LOWORD = major (lo byte) | minor (hi byte); HIWORD = build.
    ret(c, (7601ull << 16) | (1ull << 8) | 6ull);
}

void sb_shim_GetVersionExW(CPU* c) { (void)c; ret(c, 0); }

// ---- code pages ------------------------------------------------------------
void sb_shim_GetACP(CPU* c) { ret(c, 1252); }
void sb_shim_GetOEMCP(CPU* c) { ret(c, 1252); }
void sb_shim_IsValidCodePage(CPU* c) { (void)c; ret(c, 1); }
void sb_shim_GetCPInfo(CPU* c) { (void)c; ret(c, 1); }

// ---- strings comparadas -----------------------------------------------------
void sb_shim_CompareStringW(CPU* c) {
    // CompareStringW(locale, flags, a, lenA, b, lenB) — 6 args (2 no stack)
    const char16_t* a = (const char16_t*)sb_map_guest(arg3(c));
    const char16_t* b = (const char16_t*)sb_map_guest(arg_stack(c, 2));
    int r = wstr_cmp(a, b);
    ret(c, (uint64_t)(r == 0 ? 2 : r < 0 ? 1 : 3));   // CSTR_LESS_THAN/EQUAL/GREATER
}

// ---- CRT do startup ---------------------------------------------------------
void sb_shim_GetProcessHeap(CPU* c) { ret(c, 0x48454150ull); }   // "HEAP"

void sb_shim_GetStartupInfoW(CPU* c) {
    // STARTUPINFOW: 72+ bytes; zerar o suficiente (o CRT so le lpReserved etc.)
    uint64_t out = arg1(c);
    if (out) for (int i = 0; i < 8; ++i) wr64(out + 8 * i, 0);
}

void sb_shim_IsProcessorFeaturePresent(CPU* c) { ret(c, 1); }

void sb_shim_RtlCaptureContext(CPU* c) { (void)c; /* CONTEXT nao modelado */ ret(c, 0); }
void sb_shim_RtlLookupFunctionEntry(CPU* c) { (void)c; ret(c, 0); }
void sb_shim_RtlVirtualUnwind(CPU* c) { (void)c; ret(c, 0); }
void sb_shim_RtlPcToFileHeader(CPU* c) { (void)c; ret(c, 0); }
void sb_shim_RtlUnwindEx(CPU* c) { (void)c; ret(c, 0); }

void sb_shim_SetUnhandledExceptionFilter(CPU* c) { (void)c; ret(c, 0); }
void sb_shim_UnhandledExceptionFilter(CPU* c) { (void)c; ret(c, 1); }   // EXCEPTION_EXECUTE_HANDLER

void sb_shim_RaiseException(CPU* c) {
    uint32_t code = (uint32_t)arg1(c);
    printf("[guest] RaiseException(0x%x) — ignorada no host\n", code);
    ret(c, 0);
}

void sb_shim_GetEnvironmentStringsW(CPU* c) {
    // Bloco UTF-16 terminado em duplo-NULL. Devolvemos um bloco vazio.
    static char16_t empty[2] = {0, 0};
    ret(c, reinterpret_cast<uint64_t>(empty));
}
void sb_shim_FreeEnvironmentStringsW(CPU* c) { (void)c; ret(c, 1); }

void sb_shim_SetStdHandle(CPU* c) { (void)c; ret(c, 1); }
void sb_shim_GetFileType(CPU* c) { (void)c; ret(c, 2); }   // FILE_TYPE_CHAR (console)
void sb_shim_GetStringTypeW(CPU* c) { (void)c; ret(c, 1); }
void sb_shim_LCMapStringW(CPU* c) {
    // Copia a string sem transformar (lowercase/uppercase nao modelados).
    uint64_t src = arg4(c);
    int n = (int)(int32_t)arg_stack(c, 0);
    uint64_t dst = arg_stack(c, 1);
    const char16_t* s = (const char16_t*)sb_map_guest(src);
    char16_t* d = dst ? (char16_t*)sb_map_guest(dst) : nullptr;
    if (d) for (int i = 0; i < n; ++i) d[i] = s[i];
    ret(c, n);
}
void sb_shim_GetConsoleOutputCP(CPU* c) { ret(c, 1252); }
void sb_shim_GetConsoleMode(CPU* c) { (void)c; ret(c, 0); }
void sb_shim_SetConsoleMode(CPU* c) { (void)c; ret(c, 1); }
void sb_shim_SetFilePointerEx(CPU* c) { (void)c; ret(c, 1); }

}  // extern "C"

}  // namespace sb
