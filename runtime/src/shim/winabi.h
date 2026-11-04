// winabi.h — ABI dos shims Win32 do SuperBonn.
//
// O lifter modela `call [IAT]` como `dispatch(c, ct)` com o estado do guest
// no CPU struct. No host ARM64, nossaLoader escreve na IAT um endereco
// sintetico (SB_SHIM_BASE + id) e o dispatch chama o shim abaixo.
//
// Assinatura de TODO shim:  void sb_shim_x(CPU *c)
//   argumentos: c->rcx, c->rdx, c->r8, c->r9, stack a partir de c->rsp
//   retorno:    c->rax  (e c->rdx quando cabem 128 bits)
//
// Este header compila em 2 modos:
//   - build real (NDK/recomp64_cpu): CPU vem de cpu64.h do pcrecomp
//   - build de teste (host mingw):    CPU vem do mock em shim/test/host_mock.h
#pragma once

#include <cstdint>
#include <cstddef>

#ifdef SB_TEST_HOST
#include "host_mock.h"          // CPU mock + rd64/wr64/etc para teste local
#else
#include "cpu64.h"              // struct CPU do runtime recomp64_cpu (pcrecomp)
#endif

namespace sb {

// Base sintetica dos shims no espaco de enderecos do guest. O loader (que
// mapeia o PE) escreve SB_SHIM_BASE + id em cada slot da IAT; o dispatch
// reconhece a faixa e chama o shim com o CPU do guest.
constexpr uint64_t SB_SHIM_BASE = 0x0000700000000000ull;
constexpr uint64_t SB_SHIM_MAX  = 4096;

// ---- argumentos e retorno ------------------------------------------------
inline uint64_t arg1(const CPU* c) { return c->rcx; }
inline uint64_t arg2(const CPU* c) { return c->rdx; }
inline uint64_t arg3(const CPU* c) { return c->r8; }
inline uint64_t arg4(const CPU* c) { return c->r9; }

// Argumento de stack (0-based, apos o shadow space de 32 bytes do Win64).
inline uint64_t arg_stack(const CPU* c, int i) {
    return rd64(c->rsp + 8 * (i + 5));   // retorno + 4 slots de shadow
}

inline void ret(CPU* c, uint64_t v) { c->rax = v; }
inline void ret_pair(CPU* c, uint64_t lo, uint64_t hi) {
    c->rax = lo;
    c->rdx = hi;
}

// ---- memoria do guest -----------------------------------------------------
// rd16/32/64/wr32/wr64 vem do runtime (cpu64.h no build real). Strings do
// guest sao UTF-16; convercao via helpers abaixo.
inline const char16_t* guest_wstr(const CPU* c, uint64_t addr) {
    return addr ? reinterpret_cast<const char16_t*>(sb_map_guest(addr)) : nullptr;
}

// Copia no maximo n caracteres (ou NULL) de uma string do guest.
// Retorna o comprimento (estilo wcslen).
size_t guest_wstrlen(const CPU* c, uint64_t addr);
std::u16string guest_wstr_copy(const CPU* c, uint64_t addr);
std::string guest_astr_copy(const CPU* c, uint64_t addr);
uint64_t guest_alloc_wstr(const CPU* c, const std::u16string& s);

// Tamanho util de um bloco do heap do guest (bionic tem malloc_usable_size).
#ifdef SB_TEST_HOST
// o mock (host_mock.h) ja prove sb_malloc_usable_size
#else
size_t sb_malloc_usable_size(void* p);   // definida no runtime real (guestmem.cpp)
#endif

// ---- tabela de shims ------------------------------------------------------
struct ShimEntry {
    const char* dll;      // "KERNEL32.dll" (ou api-ms-win-*)
    const char* api;      // "CreateFileW"
    void (*fn)(CPU*);
};

// Registro por modulo. O loader casa com as tabelas de imports que o
// generate64 emite (recomp_imports.c de cada modulo levantado).
const ShimEntry* shim_lookup(const char* dll, const char* api);

}  // namespace sb
