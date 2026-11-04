// handles.h — tabela de HANDLEs do guest -> objetos do host.
//
// O guest guarda HANDLE como uint64 (ponteiro). Nosso host guarda objetos
// reais (FILE*, pthread, eventos). A tabela traduz: o valor devolvido ao
// guest e um indice sintetico (HANDLE_BIAS + i); o shim converte de volta.
#pragma once

#include <cstdint>
#include <vector>
#include <mutex>

namespace sb {

// Faixa sintetica: nunca colide com enderecos reais da imagem do guest
// (que estao abaixo de 0x00007F0000000000).
constexpr uint64_t HANDLE_BIAS = 0x00006F0000000000ull;
constexpr uint32_t HANDLE_NONE = 0xFFFFFFFFu;

enum class HandleKind : uint32_t {
    File = 1,
    Event,
    Mutex,
    Semaphore,
    Thread,
    FindFile,
    FileMapping,
    Timer,
};

struct GuestHandle {
    HandleKind kind;
    void* p = nullptr;        // FILE*, pthread_t, sb_event_t*, ...
    uint64_t tag = 0;         // metadados (ex.: tamanho do find)
    uint32_t refs = 1;
};

class HandleTable {
public:
    static HandleTable& instance() {
        static HandleTable t;
        return t;
    }

    uint64_t create(HandleKind kind, void* p) {
        std::lock_guard<std::mutex> lk(mu_);
        uint64_t h = HANDLE_BIAS + slots_.size();
        slots_.push_back(GuestHandle{kind, p, 0, 1});
        return h;
    }

    GuestHandle* get(uint64_t h) {
        if (h < HANDLE_BIAS) return nullptr;
        uint64_t i = h - HANDLE_BIAS;
        if (i >= slots_.size()) return nullptr;
        return &slots_[i];
    }

    bool close(uint64_t h) {
        std::lock_guard<std::mutex> lk(mu_);
        GuestHandle* gh = get(h);
        if (!gh) return false;
        gh->refs = 0;
        gh->p = nullptr;      // host destrue o objeto no shim especifico
        return true;
    }

private:
    HandleTable() = default;
    std::mutex mu_;
    std::vector<GuestHandle> slots_;
};

inline uint64_t sb_new_handle(HandleKind k, void* p) {
    return HandleTable::instance().create(k, p);
}

inline GuestHandle* sb_get_handle(uint64_t h) {
    return HandleTable::instance().get(h);
}

}  // namespace sb
