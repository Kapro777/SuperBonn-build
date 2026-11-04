// kernel32_shims.h — declaracao dos shims Win32 do SuperBonn.
#pragma once

#include "winabi.h"

// Assinatura de todo shim: void f(CPU*) com args/retorno via CPU (ver
// winabi.h). Implementacoes em kernel32_*.cpp.

// Constantes Win32 que os shims e os testes usam.
namespace sb {
constexpr uint32_t GENERIC_READ = 0x80000000u;
constexpr uint32_t GENERIC_WRITE = 0x40000000u;
constexpr uint32_t CREATE_ALWAYS = 2u;
constexpr uint32_t OPEN_EXISTING = 3u;
constexpr uint32_t OPEN_ALWAYS = 4u;
constexpr uint32_t FILE_ATTRIBUTE_DIRECTORY = 0x10u;
constexpr uint32_t INVALID_HANDLE = 0xFFFFFFFFu;
}  // namespace sb

extern "C" {

// kernel32_file.cpp
void sb_shim_CreateFileW(CPU* c);
void sb_shim_CreateFileA(CPU* c);
void sb_shim_ReadFile(CPU* c);
void sb_shim_WriteFile(CPU* c);
void sb_shim_SetFilePointer(CPU* c);
void sb_shim_GetFileSize(CPU* c);
void sb_shim_CloseHandle(CPU* c);
void sb_shim_FlushFileBuffers(CPU* c);
void sb_shim_DeleteFileW(CPU* c);
void sb_shim_CreateDirectoryW(CPU* c);
void sb_shim_GetFileAttributesW(CPU* c);
void sb_shim_GetTempPathW(CPU* c);
void sb_shim_GetCurrentDirectoryW(CPU* c);
void sb_shim_SetCurrentDirectoryW(CPU* c);
void sb_shim_FindFirstFileExW(CPU* c);
void sb_shim_FindNextFileW(CPU* c);
void sb_shim_FindClose(CPU* c);
void sb_shim_CreateFileMappingW(CPU* c);

// kernel32_memory.cpp
void sb_shim_VirtualAlloc(CPU* c);
void sb_shim_VirtualFree(CPU* c);
void sb_shim_VirtualProtect(CPU* c);
void sb_shim_VirtualQuery(CPU* c);
void sb_shim_HeapAlloc(CPU* c);
void sb_shim_HeapFree(CPU* c);
void sb_shim_HeapReAlloc(CPU* c);
void sb_shim_HeapSize(CPU* c);
void sb_shim_HeapCreate(CPU* c);
void sb_shim_HeapDestroy(CPU* c);
uint32_t sb_shim_TlsAlloc(CPU* c);
void sb_shim_TlsGetValue(CPU* c);
int  sb_shim_TlsSetValue(CPU* c);
void sb_shim_TlsFree(CPU* c);
void sb_shim_FlsAlloc(CPU* c);
void sb_shim_FlsGetValue(CPU* c);
void sb_shim_FlsSetValue(CPU* c);
void sb_shim_FlsFree(CPU* c);
void sb_shim_EncodePointer(CPU* c);
void sb_shim_DecodePointer(CPU* c);

}  // extern "C"
