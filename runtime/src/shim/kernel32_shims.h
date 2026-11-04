// kernel32_shims.h — declaracao dos shims Win32 do SuperBonn.
#pragma once

#include "winabi.h"

// Assinatura de todo shim: void f(CPU*) com args/retorno via CPU (ver
// winabi.h). Implementacoes em kernel32_*.cpp.

extern "C" {

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
