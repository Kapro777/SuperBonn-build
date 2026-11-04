# runtime/ — runtime nativo (NDK arm64-v8a)

Substitui a camada Win32/D3D12 que os binários do jogo esperam. SKELETON — ver
`docs/ROADMAP.md` Fase 3.

## Arquivos

| Arquivo | Estado |
| --- | --- |
| `CMakeLists.txt` | esqueleto (SDL3 via FetchContent, Vulkan do NDK) |
| `src/host.cpp` | host SDL3 + loop + toque, sem código de jogo ainda |
| `src/win32_shim.cpp` | **a criar** na Fase 3 (kernel32: arquivos/threads/TLS/sockets) |
| `src/steam_shim.cpp` | **a criar** (stub Steamworks: login offline, sem conquistas) |
| `src/gfx_vkd3d.cpp` | **a criar** quando escolhermos vkd3d-arm64 vs `-force-gles` |

## Build (CI)

`.github/workflows/runtime.yml` (a criar na Fase 3) compila com NDK r27 +
CMake/Ninja e sobe `libsuperbonn.so` como artefato privado.

Nada aqui é compilado localmente no Celeron N4020.
