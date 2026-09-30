# MAJEED AEX — CMake Build Template

This project combines the MAJEED native C++ engine with a CMake After Effects plugin build layout based on the `sameobake/aex-with-cmake` template.

## Target
Adobe After Effects 2023 / 23.x, Windows x64, Visual Studio 2022 / MSVC.

## Build
Use the GitHub Actions workflow:
**Actions → Build MAJEED.aex → Run workflow**

The workflow configures CMake with Visual Studio 2022 x64, builds Release, verifies `MAJEED.aex`, and uploads it as the `MAJEED-aex` artifact.

## Important
The bundled Adobe After Effects SDK is included here for a private build environment only. Do not publish the SDK in a public repository. Follow Adobe's SDK license/terms.

The MAJEED rendering engine remains in `core/`. The AE layer in `ae/` is the native plugin glue and does not use AE built-in effects as the rendering engine.
