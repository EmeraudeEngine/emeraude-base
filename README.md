# emeraude-base

![License](https://img.shields.io/badge/license-LGPLv3-blue.svg)
![Version](https://img.shields.io/badge/version-1.0.0-green.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)
![Standard](https://img.shields.io/badge/C%2B%2B-20-orange.svg)

A modern **C++20 foundation library** for cross-platform projects. Out of the box it reads,
writes and processes real file formats:

- **Images** — PNG, JPEG, Targa
- **Audio** — WAV, FLAC, OGG, MIDI (SoundFont rendering) + procedural synthesis
- **3D meshes** — OBJ, STL, MDx (MD5 / MD3 / MD2 / MDL), native `ee3d`
- **Compression & archives** — zlib, LZMA, ZIP (read/write)
- **Hashing** — MD5, SHA-256, SHA-512, CRC-32, FNV-1a

…on top of a complete math layer, a thread pool, JSON/INI parsing, and a managed set of
**prebuilt external dependencies** for Linux, macOS and Windows.

Think of it as a **"STL++"** — what the standard library would give you if it covered the
multimedia domain — and as a **turnkey starting point** for a multimedia application: you
start from a real foundation instead of a blank `CMakeLists.txt`.

It is the agnostic core of the [Emeraude-Engine](https://github.com/EmeraudeEngine/emeraude-engine)
project, but it is designed to **stand on its own**: build a tool, a simulation, or a small
engine on top of `emeraude-base` **without any rendering runtime** (no Vulkan, no GLFW, no
audio backend). Hence the name — it is meant to be the *base* of a project.

## What's inside

- **Math** — vectors, matrices, quaternions, Cartesian frames, 2D/3D primitives,
  collisions and intersections, Bézier/B-spline curves.
- **PixelFactory** — image load/save/process, text rendering, gradients.
- **WaveFactory** — audio load/process, procedural synthesis, MIDI rendering.
- **VertexFactory** — procedural mesh generation, mesh loaders (OBJ / STL / MDx / native `ee3d`),
  shape processing & analysis.
- **Toolbox** — ThreadPool, hashing, compression & ZIP archives, JSON / INI parsing,
  observer pattern, traits, string utilities.
- **Prebuilt external dependencies** — zlib, libsndfile, FreeType, jsoncpp, libsamplerate,
  and more, produced per-platform/per-config by the companion
  [`ext-deps-generator`](https://github.com/EmeraudeEngine/ext-deps-generator).

All code lives under the `EmEn::Base` namespace.

## Relationship with emeraude-engine

emeraude-base and [emeraude-engine](https://github.com/EmeraudeEngine/emeraude-engine) are a
**real split** — two separate repositories with independent lifecycles. emeraude-base
**evolves on its own**: its own versioning and roadmap, new formats and capabilities added
without waiting on the engine. The engine **builds on a given emeraude-base package** (a
pinned version): it consumes the base, it does not drive its evolution. Any other project
can do exactly the same.

## Quick start

### As a standalone dependency

```cmake
add_subdirectory(path/to/emeraude-base)

# The whole foundation — this is the target to use today:
target_link_libraries(my_app PRIVATE emeraude::base)

# Or, if you only need platform/arch/OS detection (header-only):
target_link_libraries(my_tool PRIVATE emeraude::base::platform)
```

> Currently two targets are available: **`emeraude::base`** (everything) and
> **`emeraude::base::platform`** (header-only). Finer per-module targets
> (`emeraude::base::math`, `::io`, …) are planned — see [`docs/module-map.md`](docs/module-map.md).

CMake resolves (and, if absent, downloads) the matching external dependencies on its own.
Includes use the module path with no root prefix:

```cpp
#include "Math/Vector.hpp"        // namespace EmEn::Base::Math
#include "emeraude_platform.hpp"  // root config header, namespace EmEn
```

See [`docs/integration.md`](docs/integration.md) and [`docs/module-map.md`](docs/module-map.md).

### Build standalone (to run the tests)

```bash
git clone --recurse-submodules https://github.com/EmeraudeEngine/emeraude-base.git
cd emeraude-base
cmake -B build -DCMAKE_BUILD_TYPE=Release -DEMERAUDE_ENABLE_TESTS=ON
cmake --build build -j
ctest --test-dir build
```

## Static or shared

`emeraude-base` builds as a **static** library by default, or as a **shared** one:

```cmake
set(EMERAUDE_BASE_LIBRARY_TYPE SHARED)   # default: STATIC
```

- **Static** is the common case (e.g. emeraude-engine embeds it).
- **Shared** shines in multi-process setups: a main process running the full engine plus
  satellite processes that only need `emeraude-base` can all share a single `.so`/`.dll`.

## Language standards

emeraude-base is an **incentive toward modern C++** — strict and forward-looking by default,
but never an inquisitor. Standards can be raised, not lowered below the floor:

```cmake
set(EMERAUDE_CXX_VERSION 23)   # default 20, floor 20
set(EMERAUDE_C_VERSION   23)   # default 17, floor 17
```

Warnings are errors by default; relax with `-DEMERAUDE_DISABLE_PARANOID_COMPILATION=ON`.

## Requirements

- **CMake** 3.25.1+
- **C++20 compiler** — GCC 13.3+/14.2+, Apple Clang 17+ (SDK 12.0+), or MSVC 19.43+ (VS 2022)
- The matching [`ext-deps-generator`](https://github.com/EmeraudeEngine/ext-deps-generator)
  output for your platform/config (downloaded automatically when absent).

## Project structure

`emeraude-base` mirrors the layout shared by every project in the family
(`projet-alpha`, `emeraude-engine`, `emeraude-base`) — same skeleton, different
hierarchical role:

```
cmake/          CMake helpers (Setup*.cmake for deps, source lists, installers)
dependencies/   ext-deps-generator output (+ asio, tinysoundfont submodules)
docs/           AI + developer documentation
resources/      test fixtures (assets/)
src/            library sources (EmEn::Base) + emeraude_base_config.hpp.in
```

## License

LGPLv3 — see [`LICENSE`](LICENSE). Free for the community; anything built on top stays
fully capable standalone.
