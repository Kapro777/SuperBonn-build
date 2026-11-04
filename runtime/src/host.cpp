// SKELETON — Fase 3. Host nativo Android do SuperBonn.
//
// Papel (docs/ARCHITECTURE.md §2.2): substituir a camada Win32/D3D12 que o
// binario original espera por SDL3 + Vulkan + toque, e apontar o codigo
// recompilado (libsuperbonn) para esses servicos.
//
// Ainda nao ha codigo de jogo linkado aqui: primeiro marco da Fase 2.

#include <android/log.h>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

#include <cstdint>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "superbonn", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "superbonn", __VA_ARGS__)

namespace sb {

// Ponto de entrada que o codigo recompilado chama no lugar de UnityMain().
// Na Fase 4 o lifter reescreve as chamadas do stub para ca.
extern "C" int32_t superbonn_guest_main(int32_t argc, char** argv);

}  // namespace sb

static bool running = true;

static void handle_event(const SDL_Event& e) {
  switch (e.type) {
    case SDL_EVENT_QUIT:
      running = false;
      break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
      // TODO: traduzir toque em input Win32 (mouse/button) no shim.
      break;
    default:
      break;
  }
}

extern "C" __attribute__((visibility("default")))
int32_t superbonn_host_main() {
  LOGI("SuperBonn host iniciando");

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
    LOGE("SDL_Init falhou: %s", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow("SuperBonn", 1280, 720,
                                        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    LOGE("SDL_CreateWindow falhou: %s", SDL_GetError());
    return 1;
  }

  // Loop principal: apresenta frames, bombeia input.
  uint64_t last = SDL_GetPerformanceCounter();
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) handle_event(e);

    const uint64_t now = SDL_GetPerformanceCounter();
    const double dt = static_cast<double>(now - last) / SDL_GetPerformanceFrequency();
    last = now;

    // TODO: guest frame hook — sera chamado pelo codigo recompilado (Fase 4).
    (void)dt;
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  LOGI("SuperBonn host encerrando");
  return 0;
}
