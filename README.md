# emeraude-base

![License](https://img.shields.io/badge/license-LGPLv3-blue.svg)
![Version](https://img.shields.io/badge/version-1.2.1-green.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)
![Standard](https://img.shields.io/badge/C%2B%2B-20-orange.svg)

A modern **C++20 foundation library** for cross-platform projects. Out of the box it reads,
writes and processes real file formats:

- **Images** — PNG, JPEG, Targa
- **Audio** — WAV, FLAC, OGG, MIDI (SoundFont rendering) + procedural synthesis
- **3D meshes** — OBJ, STL, FBX (ufbx), glTF (fastgltf), MDx (MD5 / MD3 / MD2 / MDL), native `ee3d`
- **Compression & archives** — zlib, zstd, brotli, LZMA, bzip2, ZIP (read/write)
- **Hashing** — MD5, SHA-256, SHA-512, CRC-32, FNV-1a

…on top of a complete math layer, a thread pool, JSON/INI parsing, networking, and a managed
set of **prebuilt external dependencies** for Linux, macOS and Windows.

Think of it as a **"STL++"** — what the standard library would give you if it covered the
multimedia domain — and as a **turnkey starting point** for a multimedia application: you
start from a real foundation instead of a blank `CMakeLists.txt`.

It is the agnostic core of the [Emeraude-Engine](https://github.com/EmeraudeEngine/emeraude-engine)
project, but it is designed to **stand on its own**: build a tool, a simulation, or a small
engine on top of `emeraude-base` **without any rendering runtime** (no Vulkan, no GLFW, no
audio backend). Hence the name — it is meant to be the *base* of a project.

All code lives under the `EmEn::Base` namespace.

## Where to read what

This repository is the **bottom of the Emeraude cascade**, and each level documents only what
it owns:

```
ext-deps-generator  →  emeraude-base  →  emeraude-engine  →  your application
```

| Level | Documents |
|---|---|
| [`ext-deps-generator`](https://github.com/EmeraudeEngine/ext-deps-generator/blob/main/README.md) | How the prebuilt external-dependency archives are **produced** (recipes, patches, per-platform build). |
| **`emeraude-base`** (here) | The foundation API, the **toolchain requirements**, the **compile policy**, and how external dependencies are **resolved and consumed**. |
| [`emeraude-engine`](https://github.com/EmeraudeEngine/emeraude-engine/blob/main/README.md) | The Vulkan runtime: graphics/audio/physics systems, Vulkan SDK, the application lifecycle, GPU debugging. |
| [`projet-nihil`](https://github.com/EmeraudeEngine/projet-nihil/blob/main/README.md) | A minimal, complete example application built on the engine — the newcomer's entry door. |

> [!IMPORTANT]
> **This README is the single source of truth for external dependencies and for the
> toolchain/compile policy of the whole family.** The engine and the applications above do not
> repeat it — they link here. Conversely, nothing about Vulkan or rendering is documented here.

## What's inside

| Module | Content |
|---|---|
| **Math** | vectors, matrices, quaternions, Cartesian frames, 2D/3D primitives, collisions & intersections, Bézier/B-spline curves. |
| **PixelFactory** | image load/save/process, text rendering (FreeType + HarfBuzz), gradients, BC7 encoding. |
| **WaveFactory** | audio load/process, procedural synthesis, MIDI/SoundFont rendering. |
| **VertexFactory** | procedural mesh generation, mesh loaders (OBJ / STL / FBX / glTF / MDx / native `ee3d`), shape processing & analysis. |
| **IO / Compression / Hash** | filesystem helpers, ZIP archives, stream compression, checksums & digests. |
| **Network** | HTTPS client over TLS (LibreSSL via `asio::ssl`, system trust store, bounded redirects, CONNECT proxy), RFC 3986 URI layer. |
| **Animation / Algorithms / GameTools** | interpolation & skeletal data types, generic algorithms, gameplay helpers. |
| **Toolbox** | ThreadPool, JSON / INI parsing, `Variant`, observer pattern, traits, string & time utilities. |
| **Testing / Benchmarking / Fuzzing / Debug** | GoogleTest & Google Benchmark harnesses, fuzzing entry points, diagnostics. |

## Relationship with emeraude-engine

emeraude-base and [emeraude-engine](https://github.com/EmeraudeEngine/emeraude-engine) are a
**real split** — two separate repositories with independent lifecycles. emeraude-base
**evolves on its own**: its own versioning and roadmap, new formats and capabilities added
without waiting on the engine. The engine **builds on a given emeraude-base package** (a
pinned version): it consumes the base, it does not drive its evolution. Any other project
can do exactly the same.

## Requirements

These are the **toolchain requirements of the entire family** — every project downstream
inherits them and does not restate them.

- **CMake** 3.25.1+
- **C++20 compiler**
  - **Linux:** GCC 13.3+ / 14.2+ — tested on Debian 13 and Ubuntu 24.04 LTS
  - **macOS:** Apple Clang 17+ with SDK 12.0+ — tested on macOS Sequoia 15.5
  - **Windows:** MSVC 19.43+ (Visual Studio 2022) — tested on Windows 11
- **Git** — submodules, and the clone-if-absent steps of the cascade
- **Python** 3 — build helper scripts

**IDE support:** the family is developed with CLion, but any CMake-aware IDE works (Visual
Studio, VSCode, Qt Creator…).

## Quick start

### As a dependency

```cmake
add_subdirectory(path/to/emeraude-base)

# The whole foundation:
target_link_libraries(my_app PRIVATE emeraude::base)

# Or a single module, when you don't need everything:
target_link_libraries(my_tool PRIVATE emeraude::base::platform)
```

Available targets: the **`emeraude::base`** umbrella, plus per-module targets
**`emeraude::base::`** `platform`, `flags`, `core`, `math`, `algorithms`, `animation`, `pixel`,
`hash`, `io`, `compression`, `network`, `time`, `vertex`, `wave`, `gametools`, `debug`.

> [!NOTE]
> `platform`, `flags`, `math`, `algorithms`, `animation` and `pixel` are header-only
> (INTERFACE) targets; the others are OBJECT libraries aggregated into the umbrella. Linking a
> single compiled module standalone may still require its sibling modules — when in doubt, link
> `emeraude::base`.

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

## External dependencies

emeraude-base owns **every** `Setup*.cmake` of the family and the resolution of the prebuilt
archives — including libraries only the engine uses (OpenAL-Soft, reproc, ufbx, fastgltf,
taglib, bc7enc, cpu_features, hwloc, libvpx…). Nothing downstream configures this.

Two categories:

**1. Prebuilt archives** produced by
[`ext-deps-generator`](https://github.com/EmeraudeEngine/ext-deps-generator) and extracted into
`dependencies/`. Current package version: **`v013`**.

**2. Git submodules**, compiled with the library:

| Library           | Repository                                                              |
|-------------------|-------------------------------------------------------------------------|
| **Asio**          | [chriskohlhoff/asio](https://github.com/chriskohlhoff/asio)             |
| **json**          | [nlohmann/json](https://github.com/nlohmann/json.git)                   |
| **TinySoundFont** | [schellingb/TinySoundFont](https://github.com/schellingb/TinySoundFont) |

### How resolution works

At configure time, `cmake/InstallDependencies.cmake`:

1. Computes the folder name for your exact platform/config (see the grammar below).
2. If that folder already exists in `dependencies/` — **nothing happens**.
3. If it is a **symbolic link** (POSIX) or a **directory junction** (Windows `mklink /J`) to a
   directory, the local setup is trusted and the download is skipped entirely. This is the
   ext-deps-generator build host's own case: it already has the libraries.
4. Otherwise the archive is downloaded from the ext-deps-generator
   [GitHub releases](https://github.com/EmeraudeEngine/ext-deps-generator/releases) and
   extracted in place.

So on a fresh clone you do **nothing** — configure and the libraries appear.

### Folder and archive grammar

The **folder name** carries the OS, the architecture, the build type and a per-OS ABI tag; the
**archive name** is exactly that folder name plus the package version:

```
<folder>                       <folder>.v013.zip
```

| Platform | Folder | ABI tag |
|---|---|---|
| Linux | `linux.<arch>-<Config>-glibc<ver>` | host **glibc** version, e.g. `glibc2.35` |
| macOS | `macos.<arch>-<Config>-sdk<ver>` | **deployment target**, e.g. `sdk12.0` |
| Windows | `windows.<arch>-<Config>-<CRT>` | MSVC runtime: `MT` (static) or `MD` (dynamic) |

`<arch>` is `x86_64` or `arm64`; `<Config>` is `Release` or `Debug`. Examples:
`linux.x86_64-Release-glibc2.35.v013.zip`, `macos.arm64-Debug-sdk12.0.v013.zip`,
`windows.x86_64-Release-MD.v013.zip`.

**Linux fallback:** the published archives are built against a floor glibc
(`EMERAUDE_EXT_LIBS_LINUX_LIBC_FALLBACK_TAG`, currently `glibc2.35`). When no archive matches
your exact host glibc, that floor is used instead — a static library linked against an older
glibc runs on any newer host. If the host glibc cannot be detected, the floor is targeted
directly.

> [!NOTE]
> The old per-distro naming (`linux-Debian.x86_64-Release-010.zip` → `linux.x86_64-Release`) is
> gone: the glibc tag superseded the `lsb_release` logic in **v013**. A stale symlink using the
> pre-v013 tag-less name is ignored and a real archive is fetched instead.

### Overriding the resolution

| Variable | Purpose |
|---|---|
| `EMERAUDE_EXT_LIBS_LINUX_LIBC_TAG` | Force the glibc tag (e.g. `-DEMERAUDE_EXT_LIBS_LINUX_LIBC_TAG=glibc2.35`). |
| `EMERAUDE_EXT_LIBS_LINUX_LIBC_FALLBACK_TAG` | Change the published floor used as fallback. |
| `EMERAUDE_EXT_LIBS_PATH` | Resolved root of the extracted output (`include/`, `lib/`). |
| `EMERAUDE_USE_STATIC_RUNTIME` | Windows: selects `/MT` vs `/MD`, and therefore the `MT`/`MD` archive. |

To work against your **own** build of the libraries, clone
[`ext-deps-generator`](https://github.com/EmeraudeEngine/ext-deps-generator), build it, and
symlink its output as `dependencies/<folder>` using the exact folder name above — step 3 of the
resolution then bypasses the download.

## Build configuration

### Static or shared

`emeraude-base` builds as a **static** library by default, or as a **shared** one:

```cmake
set(EMERAUDE_BASE_LIBRARY_TYPE SHARED)   # default: STATIC
```

- **Static** is the common case (e.g. emeraude-engine embeds it).
- **Shared** shines in multi-process setups: a main process running the full engine plus
  satellite processes that only need `emeraude-base` can all share a single `.so`/`.dll`.

### Language standards

emeraude-base is an **incentive toward modern C++** — strict and forward-looking by default,
but never an inquisitor. Standards can be raised, not lowered below the floor:

```cmake
set(EMERAUDE_CXX_VERSION 23)   # default 20, floor 20
set(EMERAUDE_C_VERSION   23)   # default 17, floor 17
```

### Options

Declared here, inherited by the whole cascade:

| Option | Default | Effect |
|---|---|---|
| `EMERAUDE_DISABLE_PARANOID_COMPILATION` | `Off` | Warnings are **errors** by default; turn On to relax. |
| `EMERAUDE_DISABLE_EXCEPTIONS` | `On` | C++ exceptions are disabled by default. |
| `EMERAUDE_DISABLE_RTTI` | `Off` | Drop RTTI. |
| `EMERAUDE_ENABLE_FP_OPTIMIZATION` | `Off` | Fast (non-strict) floating point. |
| `EMERAUDE_ENABLE_PCH` | `On` | Shared STL precompiled header (`EMERAUDE_BASE_STL_PCH_HEADERS`, see `cmake/STLPrecompiledHeaders.cmake`). Applies to every target of the cascade that asks for it; the one exception is the emeraude-engine target on MSVC, excluded by its own export-all guard. |
| `EMERAUDE_ENABLE_TESTS` | `Off` | Build the GoogleTest suites (`EmeraudeBaseUnitTests`). |
| `EMERAUDE_ENABLE_BENCHMARKS` | `Off` | Build the Google Benchmark suites. |
| `EMERAUDE_ENABLE_SANITIZERS` | `Off` | AddressSanitizer + UndefinedBehaviorSanitizer. |
| `EMERAUDE_ENABLE_OPENMP` | `On` | OpenMP (used by VertexFactory). |
| `EMERAUDE_USE_STATIC_RUNTIME` | `Off` | Windows: static MSVC runtime (`/MT`). |

Per-subsystem debug output is also available (`EMERAUDE_DEBUG_OBSERVER_PATTERN`,
`EMERAUDE_DEBUG_PIXEL_FACTORY`, `EMERAUDE_DEBUG_VERTEX_FACTORY`, `EMERAUDE_DEBUG_WAVE_FACTORY`,
`EMERAUDE_DEBUG_THREAD_POOL`…).

## Project structure

`emeraude-base` mirrors the layout shared by every project in the family — same skeleton,
different hierarchical role:

```
cmake/          CMake helpers (Setup*.cmake for deps, source lists, installers)
dependencies/   ext-deps-generator output (+ asio, tinysoundfont submodules)
docs/           AI + developer documentation
resources/      test fixtures (assets/)
src/            library sources (EmEn::Base) + emeraude_base_config.hpp.in
```

## Documentation

- [`docs/integration.md`](docs/integration.md) — consuming the library from another project
- [`docs/module-map.md`](docs/module-map.md) — module layout and targets
- [`docs/error-handling.md`](docs/error-handling.md) — error strategy (exceptions off by default)
- [`docs/caution-points.md`](docs/caution-points.md) — traps and non-obvious contracts
- [`AGENTS.md`](AGENTS.md) — guidelines for AI coding assistants

## License

LGPLv3 — see [`LICENSE`](LICENSE). Free for the community; anything built on top stays
fully capable standalone.
