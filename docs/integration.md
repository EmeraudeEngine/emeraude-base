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
| `EMERAUDE_DISABLE_EXCEPTIONS` | `On` | Build with `-fno-exceptions`. |
| `EMERAUDE_DISABLE_RTTI` | `Off` | Build with `-fno-rtti`. |
| `EMERAUDE_DISABLE_PARANOID_COMPILATION` | `Off` | Relax warnings-as-errors (`-Werror`). |
| `EMERAUDE_ENABLE_PCH` | `Off` | Precompiled headers. |
| `EMERAUDE_ENABLE_TESTS` | `Off` | Build the GoogleTest suite. |
| `EMERAUDE_EXT_LIBS_LINUX_LIBC_TAG` | *(auto)* | Linux only. Host glibc tag (e.g. `glibc2.41`) selecting the exact ext-deps archive; auto-detected via `getconf GNU_LIBC_VERSION`. Override to force a specific published tag. |
| `EMERAUDE_EXT_LIBS_LINUX_LIBC_FALLBACK_TAG` | `glibc2.35` | Linux only. Floor tag tried when no archive matches the host tag, and the direct target when the host glibc is undetectable. |

> Note: only `EMERAUDE_BASE_LIBRARY_TYPE` keeps the `_BASE_` prefix (native to this repo).
> The compile-policy options are project-wide (`EMERAUDE_*`) — emeraude-base owns them and
> propagates them to consumers (engine, projet-alpha).

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

## 4. find_package (planned)

An installable `EmeraudeBaseConfig.cmake` for `find_package(EmeraudeBase CONFIG)` is
planned but not yet available; use `add_subdirectory` for now.