// kernel32_file.cpp — shims de arquivo do KERNEL32.
// Censo (docs/RECON-DAY1.md §6): CreateFileA/W, ReadFile, WriteFile,
// SetFilePointer(Ex), CloseHandle, GetFileSize(Ex), FindFirstFileExW,
// FindNextFileW, FindClose, DeleteFileW, CreateDirectoryW, GetFileAttributesW,
// FlushFileBuffers, GetTempPathW, GetCurrentDirectoryW, SetCurrentDirectoryW,
// CreateFileMapping (stub), MultiByteToWideChar/WideCharToMultiByte.
//
// Bionic: open/read/write/lseek/close; dirent para Find*; mkdir.

#include "kernel32_shims.h"
#include "handles.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#ifndef _WIN32
#include <sys/mman.h>   // mmap do CreateFileMappingW
#endif

namespace sb {

// Constantes Win32 que o guest usa (definidas em kernel32_shims.h).
// mkdir: MinGW tem 1 arg; POSIX tem 2.
#ifdef _WIN32
#define sb_mkdir(p) ::mkdir(p)
#else
#define sb_mkdir(p) ::mkdir(p, 0777)
#endif

// Converte flags de abertura Win32 -> POSIX.
static int win32_to_poss_flags(uint32_t access, uint32_t disp, uint32_t attr) {
    int f = 0;
    if (access & GENERIC_READ) f |= O_RDONLY;
    if ((access & GENERIC_WRITE) && !(access & GENERIC_READ)) f |= O_WRONLY;
    if ((access & GENERIC_WRITE) && (access & GENERIC_READ)) f |= O_RDWR;
    switch (disp) {
        case CREATE_ALWAYS: f |= O_CREAT | O_TRUNC; break;
        case OPEN_ALWAYS:   f |= O_CREAT; break;
        default:            f |= 0; break;   // OPEN_EXISTING
    }
    (void)attr;
    return f;
}

// UTF-16 do guest -> UTF-8 do host.
static std::string to_utf8(const CPU* c, uint64_t gaddr, bool wide) {
    if (!gaddr) return {};
    if (!wide) {
        return reinterpret_cast<const char*>(sb_map_guest(gaddr));
    }
    const char16_t* w = reinterpret_cast<const char16_t*>(sb_map_guest(gaddr));
    std::string out;
    for (int i = 0; w[i]; ++i) {
        char32_t cp = w[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && w[i + 1] >= 0xDC00 && w[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (w[i + 1] - 0xDC00);
            ++i;
        }
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

extern "C" {

void sb_shim_CreateFileW(CPU* c) {
    std::string path = to_utf8(c, arg1(c), true);
    uint32_t access = (uint32_t)arg2(c);         // dwDesiredAccess (rdx)
    // dwShareMode (r8) / lpSecurityAttributes (r9): ignorados
    uint32_t disp = (uint32_t)arg_stack(c, 0);   // dwCreationDisposition
    uint32_t attr = (uint32_t)arg_stack(c, 1);   // dwFlagsAndAttributes
    int fl = win32_to_poss_flags(access, disp, attr);
    int fd = ::open(path.c_str(), fl, 0666);
    if (fd < 0) {
        ret(c, (uint64_t)(int64_t)-1);          // INVALID_HANDLE_VALUE
        return;
    }
    FILE* f = fdopen(fd, (access & GENERIC_WRITE) ? "r+b" : "rb");
    ret(c, f ? sb_new_handle(HandleKind::File, f) : (uint64_t)(int64_t)-1);
}

void sb_shim_CreateFileA(CPU* c) {
    std::string path = to_utf8(c, arg1(c), false);
    uint32_t access = (uint32_t)arg2(c);
    uint32_t disp = (uint32_t)arg_stack(c, 0);
    uint32_t attr = (uint32_t)arg_stack(c, 1);
    int fl = win32_to_poss_flags(access, disp, attr);
    int fd = ::open(path.c_str(), fl, 0666);
    if (fd < 0) { ret(c, (uint64_t)(int64_t)-1); return; }
    FILE* f = fdopen(fd, (access & GENERIC_WRITE) ? "r+b" : "rb");
    ret(c, f ? sb_new_handle(HandleKind::File, f) : (uint64_t)(int64_t)-1);
}

void sb_shim_ReadFile(CPU* c) {
    GuestHandle* gh = sb_get_handle(arg1(c));
    if (!gh || gh->kind != HandleKind::File) { ret(c, 0); return; }
    size_t n = (size_t)arg2(c);
    uint64_t buf = arg3(c);
    void* dst = buf ? sb_map_guest(buf) : nullptr;
    size_t got = dst ? fread(dst, 1, n, (FILE*)gh->p) : 0;
    // lpNumberOfBytesRead (4o arg, r9) — ponteiro; escreve quando nao-NULL
    uint64_t pRead = arg4(c);
    if (pRead) wr32(pRead, (uint32_t)got);
    ret(c, 1);
}

void sb_shim_WriteFile(CPU* c) {
    GuestHandle* gh = sb_get_handle(arg1(c));
    if (!gh || gh->kind != HandleKind::File) { ret(c, 0); return; }
    size_t n = (size_t)arg2(c);
    const void* src = arg3(c) ? sb_map_guest(arg3(c)) : nullptr;
    size_t put = src ? fwrite(src, 1, n, (FILE*)gh->p) : 0;
    uint64_t pWrote = arg4(c);
    if (pWrote) wr32(pWrote, (uint32_t)put);
    ret(c, 1);
}

void sb_shim_SetFilePointer(CPU* c) {
    GuestHandle* gh = sb_get_handle(arg1(c));
    if (!gh || gh->kind != HandleKind::File) { ret(c, (uint64_t)-1); return; }
    long dist = (long)(int64_t)arg2(c);
    uint64_t pHigh = arg3(c);
    uint32_t method = (uint32_t)arg4(c);
    int whence = method == 0 ? SEEK_SET : method == 1 ? SEEK_CUR : SEEK_END;
    if (fseek((FILE*)gh->p, dist, whence) != 0) { ret(c, (uint64_t)-1); return; }
    // 64-bit real: combina com a parte alta quando o guest passa dist como 64-bit
    if (pHigh) wr32(pHigh, 0);
    ret(c, (uint64_t)ftell((FILE*)gh->p));
}

void sb_shim_GetFileSize(CPU* c) {
    GuestHandle* gh = sb_get_handle(arg1(c));
    if (!gh || gh->kind != HandleKind::File) { ret(c, (uint64_t)-1); return; }
    long cur = ftell((FILE*)gh->p);
    fseek((FILE*)gh->p, 0, SEEK_END);
    long size = ftell((FILE*)gh->p);
    fseek((FILE*)gh->p, cur, SEEK_SET);
    ret(c, (uint64_t)(uint32_t)size);
}

void sb_shim_CloseHandle(CPU* c) {
    GuestHandle* gh = sb_get_handle(arg1(c));
    if (!gh) { ret(c, 0); return; }
    bool ok = false;
    if (gh->kind == HandleKind::File) {
        ok = fclose((FILE*)gh->p) == 0;
    }
    // outros tipos: fechados pelos shims especificos (evento/mutex/thread)
    gh->p = nullptr;
    ret(c, ok ? 1 : 0);
}

void sb_shim_FlushFileBuffers(CPU* c) {
    GuestHandle* gh = sb_get_handle(arg1(c));
    if (!gh || gh->kind != HandleKind::File) { ret(c, 0); return; }
    ret(c, fflush((FILE*)gh->p) == 0 ? 1 : 0);
}

void sb_shim_DeleteFileW(CPU* c) {
    std::string path = to_utf8(c, arg1(c), true);
    ret(c, ::remove(path.c_str()) == 0 ? 1 : 0);
}

void sb_shim_CreateDirectoryW(CPU* c) {
    std::string path = to_utf8(c, arg1(c), true);
    ret(c, sb_mkdir(path.c_str()) == 0 ? 1 : 0);
}

void sb_shim_GetFileAttributesW(CPU* c) {
    std::string path = to_utf8(c, arg1(c), true);
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) { ret(c, (uint64_t)-1); return; }
    uint32_t attrs = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : 0x80u;
    ret(c, attrs);
}

void sb_shim_GetTempPathW(CPU* c) {
    // Devolve /tmp do host; o guest escreve logs/saves la.
    const char* tmp = "/tmp/";
    uint64_t buf = arg2(c);
    if (buf) {
        char16_t* w = reinterpret_cast<char16_t*>(sb_map_guest(buf));
        for (int i = 0; tmp[i]; ++i) w[i] = (char16_t)(uint8_t)tmp[i];
        w[4] = 0;
    }
    ret(c, 5);
}

void sb_shim_GetCurrentDirectoryW(CPU* c) {
    uint32_t n = (uint32_t)arg1(c);
    uint64_t buf = arg2(c);
    char host[4096];
    if (!getcwd(host, sizeof(host))) { ret(c, 0); return; }
    if (buf && n > strlen(host) + 1) {
        char16_t* w = reinterpret_cast<char16_t*>(sb_map_guest(buf));
        for (size_t i = 0; host[i]; ++i) w[i] = (char16_t)(uint8_t)host[i];
        w[strlen(host)] = 0;
    }
    ret(c, strlen(host) + 1);
}

void sb_shim_SetCurrentDirectoryW(CPU* c) {
    std::string path = to_utf8(c, arg1(c), true);
    ret(c, ::chdir(path.c_str()) == 0 ? 1 : 0);
}

// FindFirst/Next: dirent simplificado (so nomes; o resto do WIN32_FIND_DATAW
// fica zerado — o Unity usa principalmente os nomes dos bundles).
struct SbFindState {
    DIR* dir;
    std::string pattern;
    std::string base;
};

void sb_shim_FindFirstFileExW(CPU* c) {
    // (lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, filter, flags)
    std::string path = to_utf8(c, arg1(c), true);
    std::string base = path.substr(0, path.find_last_of('/') + 1);
    auto* st = new SbFindState{opendir(base.c_str()), path, base};
    if (!st->dir) { delete st; ret(c, (uint64_t)-1); return; }
    ret(c, sb_new_handle(HandleKind::FindFile, st));
}

void sb_shim_FindNextFileW(CPU* c) {
    GuestHandle* gh = sb_get_handle(arg1(c));
    if (!gh || gh->kind != HandleKind::FindFile || !gh->p) { ret(c, 0); return; }
    auto* st = (SbFindState*)gh->p;
    struct dirent* de;
    while ((de = readdir(st->dir)) != nullptr) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        uint64_t out = arg2(c);
        if (out) {
            char16_t* w = reinterpret_cast<char16_t*>(sb_map_guest(out));
            for (int i = 0; de->d_name[i]; ++i) w[i] = (char16_t)(uint8_t)de->d_name[i];
            w[strlen(de->d_name)] = 0;
        }
        ret(c, 1);
        return;
    }
    ret(c, 0);
}

void sb_shim_FindClose(CPU* c) {
    GuestHandle* gh = sb_get_handle(arg1(c));
    if (!gh || gh->kind != HandleKind::FindFile || !gh->p) { ret(c, 0); return; }
    auto* st = (SbFindState*)gh->p;
    if (st->dir) closedir(st->dir);
    delete st;
    gh->p = nullptr;
    ret(c, 1);
}

void sb_shim_CreateFileMappingW(CPU* c) {
    // Mmap do arquivo inteiro; devolve handle com o endereco mapeado.
    GuestHandle* gh = sb_get_handle(arg1(c));
    if (!gh || gh->kind != HandleKind::File) { ret(c, 0); return; }
#ifdef _WIN32
    // Sem mmap no teste MinGW: o host Android usa a versao POSIX abaixo.
    ret(c, 0);
    return;
#else
    long cur = ftell((FILE*)gh->p);
    fseek((FILE*)gh->p, 0, SEEK_END);
    long size = ftell((FILE*)gh->p);
    fseek((FILE*)gh->p, 0, SEEK_SET);
    void* base = ::mmap(nullptr, size ? (size_t)size : 1, PROT_READ,
                        MAP_PRIVATE, fileno((FILE*)gh->p), 0);
    fseek((FILE*)gh->p, cur, SEEK_SET);
    ret(c, base == MAP_FAILED ? 0 : sb_new_handle(HandleKind::FileMapping, base));
#endif
}

}  // extern "C"

}  // namespace sb
