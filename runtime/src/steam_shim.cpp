// SKELETON (Fase 3/4) — stub Steamworks.NET / Discord / Rewired.
//
// Megabonk usa Steamworks.NET (login, conquistas, leaderboards), o Discord SDK
// e Rewired (input). No celular: login offline, sem conquistas, sem rich
// presence, sem telemetria — e o jogo nao pode saber a diferenca (hooks no
// ponto de entrada de cada API).
//
// REGRA (docs/LEGAL.md §3): nada de credencial de usuario aqui; o shim so
// responde "Steam nao disponivel" e o jogo segue.

#include <android/log.h>
#include <cstdint>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "superbonn_steam", __VA_ARGS__)

namespace sb::steam {

// Steamworks.NET chama a API nativa via P/Invoke em steam_api64.dll.
// Nosso host responde 0/"false" controladamente e loga uma vez por API.
bool LoggedOnce(const char* api) {
    static bool any = false;
    (void)api;
    if (!any) {
        LOGI("steam stub ativo: respostas neutras (sem conquistas/leaderboard)");
        any = true;
    }
    return false;
}

}  // namespace sb::steam

// Esqueletos preenchidos no Dia 3/4 conforme o censo de imports:
//   SteamAPI_Init -> false (jogo entra em modo offline)
//   SteamAPI_RestartAppIfNecessary -> false
//   SteamUserStats_SetAchievement / StoreStats -> no-op
//   SteamFriends_GetPersonaName -> "SuperBonn"
//   Rewired: substituido pela ponte SDL3 (runtime/src/host.cpp)
