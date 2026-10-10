# Integration Guide

How to consume **emeraude-base** in your own CMake project.

## 1. External dependencies

emeraude-base relies on prebuilt static libraries produced by
[`ext-deps-generator`](https://github.com/EmeraudeEngine/ext-deps-generator), one tree per
build configuration:

```
<ext-deps-generator>/output/linux.x86_64-Release/{include,lib}
<ext-deps-generator>/output/mac.arm64-Release/{include,lib}
<ext-deps-generator>/output/windows.x86_64-Release-MD/{include,lib}
```

Make that output visible to emeraude-base under `dependencies/<config>` (a symlink is the
usual approach). emeraude-base resolves it automatically and exposes:

| Variable | Meaning |
|----------|---------|
| `EMERAUDE_EXT_LIBS_PATH` | Root of the resolved output for the current config. |
| `EMERAUDE_EXT_LIBS_INCLUDE_DIR` | `…/include` |
| `EMERAUDE_EXT_LIBS_LIB_DIR` | `…/lib` |

emeraude-base is the **single source of truth** for this path — your project reads these
variables, it never recomputes them.

The `<config>` architecture comes from `APP_ARCH`. If not passed on the command line, it
defaults to `x86_64` on Linux/Windows; on macOS it follows `CMAKE_OSX_ARCHITECTURES` when
set, otherwise the host architecture (`CMAKE_SYSTEM_PROCESSOR`) — so a native build on
Apple Silicon picks `mac.arm64-*` automatically. A mismatch here is silent at compile time
and only surfaces at link time (`ld: ignoring file … found architecture 'x86_64', required
architecture 'arm64'` followed by a wall of undefined symbols).

### Linux glibc tag & download fallback

On Linux the resolved folder/archive also carries a glibc floor tag (e.g.
`linux.x86_64-Release-glibc2.41`): the ext-deps archives are ABI-tagged by the glibc they
were built against. emeraude-base detects the host glibc (`getconf GNU_LIBC_VERSION`) and
first tries the archive for that exact tag; when none is published it falls back to the
**floor tag** (`EMERAUDE_EXT_LIBS_LINUX_LIBC_FALLBACK_TAG`, default `glibc2.35`) and repoints
`EMERAUDE_EXT_LIBS_*` at whichever archive actually resolved. This is safe because a static
lib linked against an older glibc runs on any newer host (symbol versioning is
forward-compatible). When the host glibc cannot be detected at all, the floor tag is
targeted directly. A missing GitHub asset (HTTP 404) is caught by validating the downloaded
payload's ZIP magic, not merely the transport status — so a stale/absent exact-tag asset
falls through to the floor instead of being extracted as garbage.

## 2. Add it to your build

```cmake
add_subdirectory(path/to/emeraude-base)
```

Options:

| Option | Default | Effect |
|--------|---------|--------|
| `EMERAUDE_BASE_LIBRARY_TYPE` | `STATIC` | Umbrella library type (`STATIC` or `SHARED`). |
| `EMERAUDE_CXX_VERSION` | `20` | C++ standard (floor 20). |
| `EMERAUDE_C_VERSION` | `17` | C standard (floor 17). |
| `EMERAUDE_DISABLE_EXCEPTIONS` | `On` | Build with `-fno-exceptions` (MSVC: `/EHs- /EHc-` **and** `_HAS_EXCEPTIONS=0`, C4530 not suppressed — see `docs/error-handling.md` § 1). |
| `EMERAUDE_DISABLE_RTTI` | `Off` | Build with `-fno-rtti`. |
| `EMERAUDE_DISABLE_PARANOID_COMPILATION` | `Off` | Relax warnings-as-errors (`-Werror`). |
| `EMERAUDE_ENABLE_AGGRESSIVE_OPTIMIZATION` | `Off` | Raise the **Release** optimisation one notch: `-O3` instead of `-O2` (GCC/Clang), `/Ob3` instead of `/Ob2` (MSVC — there is no `/O3`, so the inline expansion level is the whole lever there). `Off` — **owner decision 2026-10-07: stay at -O2** (the CEF level), after the Linux measurement (RTX 3070 Ti workstation, one frozen source snapshot, clean builds, base 3c16641 / engine 0b112e32): build 92.8 s → 97.1 s (+4.7 %); code size `libEmeraude` +2.1 %, `projet-alpha` +5.7 %; frame time, median of 5 alternated launches, `tree-generator` 212 → 212 FPS, `forest` 31 → 32 FPS (runs 29–34: noise, under the 5 % gate); `EmeraudeBaseUnitTests` green both ways; and `-O3` BREAKS the Linux build: GCC 14 `-Werror=free-nonheap-object` false positive in engine `TextureCompressor.cpp` (a `vector::resize` inlined from `Pixmap::initialize()`). MSVC `/Ob3` was not measured (the decision covers it). macOS keeps its projet-alpha `On`. ⚠️ MSVC processes options left to right and `/O2` *resets* the level to `/Ob2`, so the `/Ob` entry must stay **after** `/O2` in `EMERAUDE_COMPILE_OPTIONS` — reordering disables it in silence. |
| `EMERAUDE_ENABLE_PCH` | `On` | Precompiled headers. Pass `${EMERAUDE_BASE_STL_PCH_HEADERS}` to `emeraude_base_target_enable_pch()`; `.m`/`.mm` sources are auto-skipped. |
| `EMERAUDE_ENABLE_TESTS` | `Off` | Build the GoogleTest suite. |
| `EMERAUDE_EXT_LIBS_LINUX_LIBC_TAG` | *(auto)* | Linux only. Host glibc tag (e.g. `glibc2.41`) selecting the exact ext-deps archive; auto-detected via `getconf GNU_LIBC_VERSION`. Override to force a specific published tag. |
| `EMERAUDE_EXT_LIBS_LINUX_LIBC_FALLBACK_TAG` | `glibc2.35` | Linux only. Floor tag tried when no archive matches the host tag, and the direct target when the host glibc is undetectable. |

> Note: only `EMERAUDE_BASE_LIBRARY_TYPE` keeps the `_BASE_` prefix (native to this repo).
> The compile-policy options are project-wide (`EMERAUDE_*`) — emeraude-base owns them and
> propagates them to consumers (engine, projet-alpha).

Two cache variables carry the resulting compiler policy for a consumer's own targets:

| Variable | Meaning |
|----------|---------|
| `EMERAUDE_COMPILE_OPTIONS` | The cascade's options: code generation (exceptions, RTTI, optimisation, …) AND the warning set (paranoid by default, `-Werror` / `/WX`). |
| `EMERAUDE_THIRD_PARTY_COMPILE_OPTIONS` | The same code generation WITHOUT any warning option, plus `-w` (`/w` on MSVC). For vendored sources compiled into a cascade binary: give them their own OBJECT library with these options and link its `$<TARGET_OBJECTS:…>` — never a per-source `-w` / `/w` on top of `/W4` (MSVC D9025 on every file). The engine's Dear ImGui is the reference use (`EmeraudeImGui`). |

## 3. Link what you need

Two targets exist today:

```cmake
target_link_libraries(my_app  PRIVATE emeraude::base)            # the whole foundation
target_link_libraries(my_tool PRIVATE emeraude::base::platform)  # header-only platform detection
```

> **Planned (not yet available):** per-module targets such as `emeraude::base::math` or
> `emeraude::base::io`, so a consumer could pull only what it needs without the rest.
> Until that split lands, link the umbrella `emeraude::base`. See
> [`module-map.md`](module-map.md) for the module → target plan and status.

Includes use the module path without a root prefix (the `Base` namespace lives in C++,
not in the include path):

```cpp
#include "Math/Vector.hpp"        // namespace EmEn::Base::Math
#include "emeraude_platform.hpp"  // root config header, namespace EmEn
```

See [`module-map.md`](module-map.md) for every target and its external dependencies.

### A consumer that also links one of the foundation's external libraries

`emeraude::base` is a static library: its `PRIVATE` external dependencies still reach the consumer's
link line (`$<LINK_ONLY:…>`). When the consumer uses the same library directly (the engine includes
`meshoptimizer.h` to decode `EXT_meshopt_compression`), it must link the SAME imported target. A
non-`GLOBAL` imported target exists only in the directory that ran `find_package()`, so a second
`find_package()` in the consumer's directory creates a second target. CMake cannot merge two
distinct targets naming one archive, and the library is emitted twice. Apple's ld then reports
`ld: warning: ignoring duplicate libraries: '…/libmeshoptimizer.a'`. CMake policy `CMP0156` does
not help, because it de-duplicates link items, not paths.

The rule: a `cmake/Setup*.cmake` script that the consumer runs too imports its targets `GLOBAL`
(`find_package(… CONFIG REQUIRED GLOBAL …)`, CMake 3.24). The consumer's `find_package()` then
returns early on the existing target. Applied to `SetupMeshOptimizer.cmake` (2026-10-09, macOS
arm64 / Xcode 27: the warning gone, one `libmeshoptimizer.a` on the `Emeraude.framework` link
line). Detector: list the `LINK_LIBRARIES` of a target in `build.ninja` and count the repeated
`.a` files.

## 4. find_package (planned)

An installable `EmeraudeBaseConfig.cmake` for `find_package(EmeraudeBase CONFIG)` is
planned but not yet available; use `add_subdirectory` for now.