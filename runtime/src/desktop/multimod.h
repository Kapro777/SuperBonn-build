// multimod.h — host multi-módulo: dispatch, imports e exports entre módulos.
//
// Um port real tem vários módulos levantados (exe, UnityPlayer, GameAssembly...).
// Cada um tem sua tabela de dispatch e sua tabela de imports. O dispatch
// procura em todas; a IAT de cada módulo é preenchida com:
//   1. endereço de função LEVANTADA de outro módulo (imports internos:
//      exe -> UnityPlayer!UnityMain2), ou
//   2. sentinel HLE (shims do host) para o que é do sistema.
//
// NOTA de linkage: cpu64.h (do runtime) declara dispatch/dispatch_jmp/
// g_image_delta com linkage C++; todo o build compila como C++ —
// não use extern "C" neste header.
#pragma once

#include <cstdint>
#include "cpu64.h"

// Assinaturas compatíveis com as tabelas geradas pelo generate64 (namespaced).
typedef struct { uint64_t va; void (*fn)(CPU*); } sb_entry_t;
typedef struct { uint64_t slot; const char* dll; const char* name; } sb_import_t;

typedef struct {
    const char*        dll_name;     // "UnityPlayer.dll" (como os imports citam)
    uint64_t           image_base;   // base real do modulo no guest
    uint64_t           hle_base;     // faixa de sentinelas deste modulo
    uint32_t           hle_stride;   // 8
    const sb_import_t* imports;      // tabela gerada (namespaced)
    uint32_t           import_count;
    const sb_entry_t*  table;        // funcoes levantadas (namespaced)
    uint32_t           table_count;
} sb_module_t;

// Export de um modulo (nome -> endereco no guest), preenchido pelo loader.
typedef struct { const char* name; uint64_t va; } sb_export_t;

// ---- API do host -------------------------------------------------------------
void        sb_hle_init(void);
void        sb_hle_register_module(const sb_module_t* m);
void        sb_hle_set_exports(int mod_idx, const sb_export_t* exps, int n);
sb_module_t* sb_hle_module(int i);
uint64_t    sb_hle_slot_addr(sb_module_t* m, const char* dll, const char* name);
uint64_t    sb_hle_module_handle(const char* dll_name);
uint64_t    sb_hle_get_proc(uint64_t hmodule, const char* name);
void        sb_set_module_delta(int mod_idx, int64_t delta);
void        dispatch(CPU* c, uint64_t addr);
void        dispatch_jmp(CPU* c, uint64_t target);
