// SKELETON (Fase 3) — shims Win32 -> bionic/POSIX/SDL3.
//
// Regra de seguranca (herdada do pcrecomp): TODO shim de import NAO
// implementado ABORTA com mensagem clara, nunca retorna valor falsos.
// Retornar 0/TRUE silenciosamente esconde o bug e corrompe estado do guest.

#include <android/log.h>
#include <cstdio>
#include <cstdlib>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "superbonn", __VA_ARGS__)

namespace sb::shim {

[[noreturn]] void unimplemented(const char* api, const char* file, int line) {
    LOGE("shim NAO implementado: %s (%s:%d) — abortando (nunca retornar falso)",
         api, file, line);
    __builtin_trap();
}

}  // namespace sb::shim

// Cada shim implementado entra aqui com assinatura stdcall/x64 do Windows.
// A lista exata sai do censo real (scripts/lift/import-census.py, Dia 1 —
// rodado nos binarios do jogador, ver docs/ARCHITECTURE §6):
//
//   CENSO MEDIDO (por modulo):
//     UnityPlayer.dll 513 | GameAssembly.dll 253 | baselib.dll 122
//     Megabonk.exe 72   | steam_api64.dll 92 | discord_game_sdk.dll 124
//     Rewired_* (ARM64/x64): 68-87 cada | D3D12Core.dll 306
//
//   SUPERFICIE TOTAL (APIs distintas por DLL):
//     KERNEL32 264 | USER32 122 | WS2_32 52 | WINMM 23 | ADVAPI32 35
//     api-ms-win-core-synch-l1-1-0 25 | processthreads-l1-1-0 19 | rtlsupport 7
//     ext-ms-win-dx-d3dkmt-dxcore-l1-1-0 (delay) 89  <- DXCore/D3DKMT
//     winrt-string 9 | ole32/OLEAUT32 29 | WINHTTP 4 | WININET 7 | HID 14
//     d3d12.dll (delay) 3 | dxgi.dll 2 | d3d11.dll 2 | OPENGL32 6 | DINPUT8 1
//
//   ORDEM DE IMPLEMENTACAO (Dia 3): kernel32 (arquivo/thread/TLS/tempo) ->
//   user32 (janela/mensagem) -> ws2_32 (rede) -> advapi32 (registro) ->
//   winmm/hid/dinput8 (input) -> winhttp/wininet (rede) ->
//   camada grafica (d3d12/dxgi/dxcore) em gfx, nao aqui.
//
//   Plugins (steam_api64, discord_game_sdk, Rewired_*): stubs em steam_shim.cpp.

#define SB_UNIMPLEMENTED(api) sb::shim::unimplemented(api, __FILE__, __LINE__)

// Exemplos de esqueleto — preenchidos no Dia 3 com o censo real:
extern "C" {
void shim_kernel32_TODO_Sleep(unsigned ms) {
    // SDL_Delay(ms);
    (void)ms;
    SB_UNIMPLEMENTED("kernel32!Sleep");
}
}  // extern "C"
