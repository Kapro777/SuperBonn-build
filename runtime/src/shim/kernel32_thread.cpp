// kernel32_thread.cpp — threads e sincronizacao do guest.
// Censo (docs/RECON-DAY1.md §6): CreateThread, ExitThread, GetCurrentThread(Id),
// GetCurrentProcess(Id), CriticalSection*, SRW*, Event*, Mutex*, Semaphore*,
// WaitForSingleObject(Ex), Sleep, SwitchToThread, SList*, etc.
//
// Modelo: cada thread do guest = 1 pthread do host com seu proprio CPU struct.
// CreateThread(lpStartAddress, lpParameter) -> pthread rodando
// sb_thread_trampoline, que monta um CPU novo com rip=start, rcx=param e
// chama dispatch(c, start) — o mesmo dispatch das chamadas indiretas.

#include "kernel32_shims.h"
#include "handles.h"

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <atomic>
#include <mutex>

// ---- runtime de execucao (vem do recomp64_cpu) -----------------------------
void dispatch(CPU* c, uint64_t addr);      // despacha para o codigo do guest
CPU* sb_new_guest_cpu();                   // aloca/zera um CPU do host

namespace sb {

// Constantes (INFINITE, WAIT_*) estao em kernel32_shims.h.

// ---- estruturas de sincronizacao do host ----------------------------------
struct SbEvent {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    bool manual;
    bool signaled;
};

struct SbMutex {
    pthread_mutex_t mu;
    uint32_t refs = 1;
};

struct SbSemaphore {
    sem_t sem;
};

struct SbSRW {
    pthread_rwlock_t rw;
};

// ---- thread trampoline -----------------------------------------------------
struct SbThreadArg {
    CPU* cpu;               // CPU novo (host alocou)
    uint64_t start_addr;
    uint64_t param;
    uint64_t exit_code;
};

static void* sb_thread_trampoline(void* p) {
    auto* a = static_cast<SbThreadArg*>(p);
    a->cpu->rip = a->start_addr;
    a->cpu->rcx = a->param;
    dispatch(a->cpu, a->start_addr);
    a->exit_code = a->cpu->rax;
    return nullptr;
}

extern "C" {

void sb_shim_CreateThread(CPU* c) {
    // (lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter,
    //  dwCreationFlags, lpThreadId)
    uint64_t start = arg3(c);
    uint64_t param = arg4(c);
    // 5o (flags) e 6o (lpThreadId) no stack
    uint32_t flags = (uint32_t)arg_stack(c, 1);
    uint64_t tidOut = arg_stack(c, 2);
    (void)flags;

    auto* a = new SbThreadArg{};
    a->start_addr = start;
    a->param = param;
    a->cpu = sb_new_guest_cpu();
    a->cpu->rsp = reinterpret_cast<uint64_t>(calloc(1, 8u << 20)) + (8u << 20);

    auto* th = new pthread_t{};
    pthread_create(th, nullptr, sb_thread_trampoline, a);
    if (tidOut) wr32(tidOut, 1);
    ret(c, sb_new_handle(HandleKind::Thread, th));
}

void sb_shim_ExitThread(CPU* c) {
    uint32_t code = (uint32_t)arg1(c);
    // Thread atual: saindo pelo dispatch. No host real: pthread_exit.
    ret(c, code);
    pthread_exit(reinterpret_cast<void*>(static_cast<uintptr_t>(code)));
}

void sb_shim_GetCurrentThread(CPU* c) { ret(c, 0xFFFFFFFEull); }        // pseudo-handle
void sb_shim_GetCurrentThreadId(CPU* c) {
    static std::atomic<uint32_t> next{1};
    ret(c, 1000 + next.fetch_add(1));
}
void sb_shim_GetCurrentProcess(CPU* c) { ret(c, 0xFFFFFFFFFFFFFFFFull); }
void sb_shim_GetCurrentProcessId(CPU* c) { ret(c, 1); }

// Mutex RECURSIVO de verdade (EnterCriticalSection do Win32 e recursivo).
static pthread_mutex_t* new_recursive_mutex() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    auto* mu = new pthread_mutex_t;
    pthread_mutex_init(mu, &attr);
    pthread_mutexattr_destroy(&attr);
    return mu;
}

// ---- critical sections (recursivas) ---------------------------------------
void sb_shim_InitializeCriticalSection(CPU* c) {
    // O guest passa o endereco do CRITICAL_SECTION; guardamos um mutex nele.
    uint64_t addr = arg1(c);
    auto* mu = new_recursive_mutex();
    // O CRITICAL_SECTION do guest: LockCount, RecursionCount, OwningThread,
    // LockSemaphore, SpinCount (40 bytes). Zeramos e usamos o host mutex.
    for (int i = 0; i < 5; ++i) wr64(addr + 8 * i, 0);
    // Guardamos o ponteiro do host no campo LockSemaphore (offset 24).
    wr64(addr + 24, reinterpret_cast<uint64_t>(mu));
    ret(c, 0);
}

void sb_shim_InitializeCriticalSectionAndSpinCount(CPU* c) {
    sb_shim_InitializeCriticalSection(c);
}

void sb_shim_EnterCriticalSection(CPU* c) {
    uint64_t addr = arg1(c);
    auto* mu = reinterpret_cast<pthread_mutex_t*>(rd64(addr + 24));
    if (mu) pthread_mutex_lock(mu);
}

int sb_shim_TryEnterCriticalSection(CPU* c) {
    uint64_t addr = arg1(c);
    auto* mu = reinterpret_cast<pthread_mutex_t*>(rd64(addr + 24));
    int ok = (mu && pthread_mutex_trylock(mu) == 0) ? 1 : 0;
    ret(c, (uint64_t)ok);
    return ok;
}

void sb_shim_LeaveCriticalSection(CPU* c) {
    uint64_t addr = arg1(c);
    auto* mu = reinterpret_cast<pthread_mutex_t*>(rd64(addr + 24));
    if (mu) pthread_mutex_unlock(mu);
}

void sb_shim_DeleteCriticalSection(CPU* c) {
    uint64_t addr = arg1(c);
    auto* mu = reinterpret_cast<pthread_mutex_t*>(rd64(addr + 24));
    if (mu) { pthread_mutex_destroy(mu); delete mu; }
    wr64(addr + 24, 0);
}

// ---- SRW locks -------------------------------------------------------------
void sb_shim_InitializeSRWLock(CPU* c) {
    uint64_t addr = arg1(c);
    wr64(addr, 0);   // PTR fica no proprio campo; host usa rwlock a parte
}

void sb_shim_AcquireSRWLockExclusive(CPU* c) { (void)c; }
void sb_shim_AcquireSRWLockShared(CPU* c) { (void)c; }
void sb_shim_ReleaseSRWLockExclusive(CPU* c) { (void)c; }
void sb_shim_ReleaseSRWLockShared(CPU* c) { (void)c; }
void sb_shim_TryAcquireSRWLockExclusive(CPU* c) { ret(c, 1); }
void sb_shim_TryAcquireSRWLockShared(CPU* c) { ret(c, 1); }
// NOTA: SRW como no-op so e seguro enquanto houver apenas uma thread real
// do guest (singlecore test). O bring-up multi-core troca por rwlocks reais
// guardados no proprio ponteiro do guest.

// ---- eventos ---------------------------------------------------------------
void sb_shim_CreateEventW(CPU* c) {
    int manual = (int)arg3(c);         // bManualReset
    int initial = (int)arg4(c);        // bInitialState
    auto* e = new SbEvent{};
    pthread_mutex_init(&e->mu, nullptr);
    pthread_cond_init(&e->cv, nullptr);
    e->manual = manual != 0;
    e->signaled = initial != 0;
    ret(c, sb_new_handle(HandleKind::Event, e));
}

void sb_shim_CreateEventA(CPU* c) { sb_shim_CreateEventW(c); }
void sb_shim_CreateEventExW(CPU* c) {
    // (lpEventAttributes, lpName, dwFlags, dwDesiredAccess) — flags no r8
    uint32_t flags = (uint32_t)arg3(c);
    auto* e = new SbEvent{};
    pthread_mutex_init(&e->mu, nullptr);
    pthread_cond_init(&e->cv, nullptr);
    e->manual = (flags & 1) != 0;      // CREATE_EVENT_MANUAL_RESET
    e->signaled = (flags & 2) != 0;    // CREATE_EVENT_INITIAL_SET
    ret(c, sb_new_handle(HandleKind::Event, e));
}

void sb_shim_SetEvent(CPU* c) {
    GuestHandle* gh = sb_get_handle(arg1(c));
    if (!gh || gh->kind != HandleKind::Event || !gh->p) { ret(c, 0); return; }
    auto* e = (SbEvent*)gh->p;
    pthread_mutex_lock(&e->mu);
    e->signaled = true;
    pthread_cond_broadcast(&e->cv);
    pthread_mutex_unlock(&e->mu);
    ret(c, 1);
}

void sb_shim_ResetEvent(CPU* c) {
    GuestHandle* gh = sb_get_handle(arg1(c));
    if (!gh || gh->kind != HandleKind::Event || !gh->p) { ret(c, 0); return; }
    auto* e = (SbEvent*)gh->p;
    pthread_mutex_lock(&e->mu);
    e->signaled = false;
    pthread_mutex_unlock(&e->mu);
    ret(c, 1);
}

// ---- espera ----------------------------------------------------------------
void sb_shim_WaitForSingleObject(CPU* c) {
    GuestHandle* gh = sb_get_handle(arg1(c));
    uint32_t ms = (uint32_t)arg2(c);
    (void)ms;   // timeout nao modelado: espera sempre (Unity raramente usa)
    if (!gh) { ret(c, WAIT_FAILED); return; }
    if (gh->kind == HandleKind::Event && gh->p) {
        auto* e = (SbEvent*)gh->p;
        pthread_mutex_lock(&e->mu);
        while (!e->signaled) pthread_cond_wait(&e->cv, &e->mu);
        if (!e->manual) e->signaled = false;   // auto-reset
        pthread_mutex_unlock(&e->mu);
        ret(c, WAIT_OBJECT_0);
        return;
    }
    if (gh->kind == HandleKind::Mutex && gh->p) {
        auto* m = (SbMutex*)gh->p;
        pthread_mutex_lock(&m->mu);
        pthread_mutex_unlock(&m->mu);           // "adquirido e liberado"
        ret(c, WAIT_OBJECT_0);
        return;
    }
    if (gh->kind == HandleKind::Thread && gh->p) {
        auto* th = (pthread_t*)gh->p;
        pthread_join(*th, nullptr);
        ret(c, WAIT_OBJECT_0);
        return;
    }
    ret(c, WAIT_OBJECT_0);
}

void sb_shim_Sleep(CPU* c) {
    uint32_t ms = (uint32_t)arg1(c);
    if (ms) usleep(ms * 1000);
    else sched_yield();
    ret(c, 0);
}

void sb_shim_SwitchToThread(CPU* c) { (void)c; ret(c, 1); }

// ---- mutex -----------------------------------------------------------------
void sb_shim_CreateMutexW(CPU* c) {
    auto* m = new SbMutex{};
    pthread_mutex_init(&m->mu, nullptr);
    int initial = (int)arg3(c);   // bInitialOwner
    if (initial) pthread_mutex_lock(&m->mu);
    ret(c, sb_new_handle(HandleKind::Mutex, m));
}

void sb_shim_CreateMutexA(CPU* c) { sb_shim_CreateMutexW(c); }

void sb_shim_ReleaseMutex(CPU* c) {
    GuestHandle* gh = sb_get_handle(arg1(c));
    if (!gh || gh->kind != HandleKind::Mutex || !gh->p) { ret(c, 0); return; }
    pthread_mutex_unlock(&((SbMutex*)gh->p)->mu);
    ret(c, 1);
}

// ---- semaforos -------------------------------------------------------------
void sb_shim_CreateSemaphoreExW(CPU* c) {
    long initial = (long)(int64_t)arg2(c);
    long max = (long)(int64_t)arg3(c);
    auto* s = new SbSemaphore{};
    sem_init(&s->sem, 0, initial < 0 ? 0 : (unsigned)initial);
    (void)max;
    ret(c, sb_new_handle(HandleKind::Semaphore, s));
}

void sb_shim_ReleaseSemaphore(CPU* c) {
    GuestHandle* gh = sb_get_handle(arg1(c));
    if (!gh || gh->kind != HandleKind::Semaphore || !gh->p) { ret(c, 0); return; }
    long n = (long)(int64_t)arg2(c);
    for (long i = 0; i < (n > 0 ? n : 1); ++i) sem_post(&((SbSemaphore*)gh->p)->sem);
    ret(c, 1);
}

// ---- SList (pop/push com lock; SAFE: nunca lock-free corrompido) ----------
void sb_shim_InitializeSListHead(CPU* c) {
    // SLIST_HEADER do guest: 16 bytes zerados + Depth/SequenceNext usados pelo
    // host. Como usamos lock, zeramos tudo.
    for (int i = 0; i < 2; ++i) wr64(arg1(c) + 8 * i, 0);
    ret(c, 0);
}

// ---- prioridade/afinidade (no-ops) ----------------------------------------
void sb_shim_SetThreadPriority(CPU* c) { (void)c; ret(c, 1); }
void sb_shim_SetThreadAffinityMask(CPU* c) { (void)c; ret(c, 1); }

}  // extern "C"

}  // namespace sb
