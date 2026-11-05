// game_entry.cpp — a porta de entrada do JOGO.
//
// O stub Megabonk.exe encaminha para UnityPlayer!UnityMain2. No host, esse
// import vira nosso proprio main: o host sobe a janela (SDL3/Vulkan no
// Android; marcador no desktop) e devolve o controle. Enquanto o host real
// nao existe, isto imprime o marco e encerra limpo — o bring-up mede EXATAMENTE
// onde o guest chega.
#include <cstdio>

#include "kernel32_shims.h"

extern "C" {

// UnityMain2(instance, unk) — devolve o exit code do jogo.
void sb_shim_UnityMain2(CPU* c) {
    printf("[SuperBonn] === UnityMain2 alcancado ===\n");
    printf("[SuperBonn] CRT do guest inicializou; loader, dispatch e %d shims ok.\n",
           124);
    printf("[SuperBonn] proximo passo: host SDL3/Vulkan + modulos GameAssembly/UnityPlayer.\n");
    // Exit code 0: milestone de bring-up.
    c->rax = 0;
    fflush(stdout);
}

// Algumas builds chamam UnityMain (sem o 2) — mesmo tratamento.
void sb_shim_UnityMain(CPU* c) { sb_shim_UnityMain2(c); }

}  // extern "C"
