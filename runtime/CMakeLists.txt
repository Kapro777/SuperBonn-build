cmake_minimum_required(VERSION 3.25)
# SKELETON — Fase 3 do roadmap. Ainda não compila contra o jogo.

project(superbonn_rt LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Alvo: libsuperbonn.so para arm64-v8a, rodando dentro do APK (android/).
# O codigo do jogo recompilado (lift/out) entra aqui como objetos no passo de
# link final — ver docs/ARCHITECTURE.md §4.

option(SUPERBONN_FETCH_SDL3 "Baixar SDL3 via FetchContent" ON)

if(SUPERBONN_FETCH_SDL3)
  include(FetchContent)
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG main
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3)
endif()

# Vulkan: o NDK ja expoe <vulkan/vulkan.h>; apenas linkar.
find_package(Vulkan)

add_library(superbonn SHARED
  src/host.cpp          # host Android: janela, loop, input touch
  src/win32_shim.cpp    # shims kernel32/user32 (cresce conforme a Fase 2/4)
  src/steam_shim.cpp    # stub Steamworks (login offline, sem conquistas)
)

target_include_directories(superbonn PRIVATE src)
target_link_libraries(superbonn
  PRIVATE
    SDL3::SDL3
    $<$<BOOL:${Vulkan_FOUND}>:Vulkan::Vulkan>
    android
    log
)

# -w porque codigo levantado de binario gera milhares de warning
target_compile_options(superbonn PRIVATE -O2 -w)
