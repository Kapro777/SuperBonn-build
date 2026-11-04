// host_mock.h — CPU mock + helpers de memoria do guest para testar os shims
// no host mingw, sem o runtime recomp64_cpu e sem o jogo. Build local apenas.
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>

// Espelha os campos do CPU do pcrecomp que os shims usam.
struct CPU {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip;
};

// Mock de memoria do guest: um arena simples (identidade: endereco do guest
// == endereco do host no teste). O runtime real mapeia a imagem do PE.
inline void* sb_map_guest(uint64_t a) { return reinterpret_cast<void*>(static_cast<uintptr_t>(a)); }

inline uint64_t rd64(uint64_t a) { uint64_t v; memcpy(&v, sb_map_guest(a), 8); return v; }
inline uint32_t rd32(uint64_t a) { uint32_t v; memcpy(&v, sb_map_guest(a), 4); return v; }
inline uint16_t rd16(uint64_t a) { uint16_t v; memcpy(&v, sb_map_guest(a), 2); return v; }
inline uint8_t rd8(uint64_t a) { return *reinterpret_cast<uint8_t*>(sb_map_guest(a)); }
inline void wr64(uint64_t a, uint64_t v) { memcpy(sb_map_guest(a), &v, 8); }
inline void wr32(uint64_t a, uint32_t v) { memcpy(sb_map_guest(a), &v, 4); }
inline void wr16(uint64_t a, uint16_t v) { memcpy(sb_map_guest(a), &v, 2); }
inline void wr8(uint64_t a, uint8_t v) { *reinterpret_cast<uint8_t*>(sb_map_guest(a)) = v; }

// Strings do guest (teste: memoria identidade, UTF-16 literal).
namespace sb {
inline size_t guest_wstrlen(const CPU*, uint64_t addr) {
    auto* p = reinterpret_cast<const char16_t*>(sb_map_guest(addr));
    size_t n = 0;
    while (p[n]) ++n;
    return n;
}
inline std::u16string guest_wstr_copy(const CPU*, uint64_t addr) {
    auto* p = reinterpret_cast<const char16_t*>(sb_map_guest(addr));
    return std::u16string(p ? p : u"");
}
inline std::string guest_astr_copy(const CPU*, uint64_t addr) {
    auto* p = reinterpret_cast<const char*>(sb_map_guest(addr));
    return std::string(p ? p : "");
}
inline uint64_t guest_alloc_wstr(const CPU*, const std::u16string& s) {
    void* p = malloc((s.size() + 1) * sizeof(char16_t));
    memcpy(p, s.c_str(), (s.size() + 1) * sizeof(char16_t));
    return reinterpret_cast<uint64_t>(p);
}
}  // namespace sb

// Rastreia tamanhos para sb_malloc_usable_size sem depender de _msize.
#include <vector>
#include <utility>
struct sb_alloc_tracker {
    std::vector<std::pair<void*, size_t>> items;
    void* add(size_t n) {
        void* p = malloc(n ? n : 1);
        items.emplace_back(p, n);
        return p;
    }
    void* grow(void* p, size_t n) {
        size_t old = size_of(p);
        void* q = realloc(p, n ? n : 1);
        for (auto& it : items)
            if (it.first == p) { it.first = q; it.second = n; return q; }
        items.emplace_back(q, n);
        (void)old;
        return q;
    }
    void erase(void* p) {
        for (auto it = items.begin(); it != items.end(); ++it)
            if (it->first == p) { items.erase(it); free(p); return; }
    }
    size_t size_of(void* p) const {
        for (auto& it : items) if (it.first == p) return it.second;
        return 0;
    }
};
inline sb_alloc_tracker& sb_tracker() { static sb_alloc_tracker t; return t; }

// O runtime real (NDK) implementa estas tres sobre mmap/malloc do bionic.
inline void* sb_host_alloc(size_t n) { return sb_tracker().add(n); }
inline void sb_host_free(void* p) { sb_tracker().erase(p); }
inline void* sb_host_realloc(void* p, size_t n) { return sb_tracker().grow(p, n); }
inline size_t sb_malloc_usable_size(void* p) { return sb_tracker().size_of(p); }

// Mocks do runtime de execucao (recomp64_cpu): no teste, o "guest" que roda
// numa thread do guest e uma funcao do host com a mesma assinatura.
inline void dispatch(CPU* c, uint64_t addr) {
    reinterpret_cast<void (*)(CPU*)>(addr)(c);
}
inline CPU* sb_new_guest_cpu() {
    static CPU cpu;
    memset(&cpu, 0, sizeof(cpu));
    return &cpu;
}
