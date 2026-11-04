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
constexpr uint32_t INFINITE = 0xFFFFFFFFu;
constexpr uint32_t WAIT_OBJECT_0 = 0u;
constexpr uint32_t WAIT_TIMEOUT = 0x102u;
constexpr uint32_t WAIT_FAILED = 0xFFFFFFFFu;
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

// kernel32_misc.cpp
void sb_shim_QueryPerformanceCounter(CPU* c);
void sb_shim_QueryPerformanceFrequency(CPU* c);
void sb_shim_GetTickCount64(CPU* c);
void sb_shim_GetSystemTimeAsFileTime(CPU* c);
void sb_shim_MultiByteToWideChar(CPU* c);
void sb_shim_WideCharToMultiByte(CPU* c);
void sb_shim_GetCommandLineW(CPU* c);
void sb_shim_GetCommandLineA(CPU* c);
void sb_shim_GetModuleHandleW(CPU* c);
void sb_shim_GetModuleHandleExW(CPU* c);
void sb_shim_GetModuleFileNameW(CPU* c);
void sb_shim_LoadLibraryExW(CPU* c);
void sb_shim_FreeLibrary(CPU* c);
void sb_shim_GetProcAddress(CPU* c);
void sb_shim_GetLastError(CPU* c);
void sb_shim_SetLastError(CPU* c);
void sb_shim_IsDebuggerPresent(CPU* c);
void sb_shim_OutputDebugStringW(CPU* c);
void sb_shim_OutputDebugStringA(CPU* c);
void sb_shim_WriteConsoleW(CPU* c);
void sb_shim_GetStdHandle(CPU* c);
void sb_shim_ExitProcess(CPU* c);
void sb_shim_TerminateProcess(CPU* c);
void sb_shim_GetExitCodeProcess(CPU* c);
void sb_shim_GetSystemInfo(CPU* c);
void sb_shim_GetVersion(CPU* c);
void sb_shim_GetVersionExW(CPU* c);
void sb_shim_GetACP(CPU* c);
void sb_shim_GetOEMCP(CPU* c);
void sb_shim_IsValidCodePage(CPU* c);
void sb_shim_GetCPInfo(CPU* c);
void sb_shim_CompareStringW(CPU* c);

// kernel32_thread.cpp
void sb_shim_CreateThread(CPU* c);
void sb_shim_ExitThread(CPU* c);
void sb_shim_GetCurrentThread(CPU* c);
void sb_shim_GetCurrentThreadId(CPU* c);
void sb_shim_GetCurrentProcess(CPU* c);
void sb_shim_GetCurrentProcessId(CPU* c);
void sb_shim_InitializeCriticalSection(CPU* c);
void sb_shim_InitializeCriticalSectionAndSpinCount(CPU* c);
void sb_shim_EnterCriticalSection(CPU* c);
int  sb_shim_TryEnterCriticalSection(CPU* c);
void sb_shim_LeaveCriticalSection(CPU* c);
void sb_shim_DeleteCriticalSection(CPU* c);
void sb_shim_InitializeSRWLock(CPU* c);
void sb_shim_AcquireSRWLockExclusive(CPU* c);
void sb_shim_ReleaseSRWLockExclusive(CPU* c);
void sb_shim_AcquireSRWLockShared(CPU* c);
void sb_shim_ReleaseSRWLockShared(CPU* c);
void sb_shim_CreateEventW(CPU* c);
void sb_shim_CreateEventA(CPU* c);
void sb_shim_CreateEventExW(CPU* c);
void sb_shim_SetEvent(CPU* c);
void sb_shim_ResetEvent(CPU* c);
void sb_shim_WaitForSingleObject(CPU* c);
void sb_shim_Sleep(CPU* c);
void sb_shim_SwitchToThread(CPU* c);
void sb_shim_CreateMutexW(CPU* c);
void sb_shim_CreateMutexA(CPU* c);
void sb_shim_ReleaseMutex(CPU* c);
void sb_shim_CreateSemaphoreExW(CPU* c);
void sb_shim_ReleaseSemaphore(CPU* c);
void sb_shim_InitializeSListHead(CPU* c);
void sb_shim_SetThreadPriority(CPU* c);
void sb_shim_SetThreadAffinityMask(CPU* c);

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
