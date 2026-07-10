# emeraude-base — AI Context & Guidelines

## 1. Identity

**emeraude-base** is the **foundation library** of the Emeraude-Engine project: the
agnostic, reusable core (math, I/O, hashing, factories, traits, threading, …) that
sits *below* the engine runtime.

> [!IMPORTANT]
> **Mission — a turnkey starting point for multimedia applications.** Think of it as a
> **"STL++"**: what the C++ standard library would offer if it covered the multimedia
> domain. It bundles the painful base layer every cross-platform multimedia/graphics
> project re-solves from scratch — vectors/matrices/quaternions & geometry, image / audio /
> mesh loading and processing, hashing, compression, threading, parsing — *plus* a managed,
> prebuilt set of external dependencies for Linux/macOS/Windows. A developer can start a
> real application on top of it instead of a blank `CMakeLists.txt`. The Emeraude-Engine is
> the most prominent consumer, but emeraude-base is meant to be a project starter in its
> own right.

- **Standalone repository.** Independent of `emeraude-engine`, NOT a git submodule.
- **Namespace: `EmEn::Base`.** A file at `src/Foo/Bar.hpp` is `#include "Foo/Bar.hpp"`
  (no prefix in the include path), namespace `EmEn::Base::Foo`. This is the engine's
  former `EmEn::Libs::*`, with `Libs` renamed to `Base`. The `EmEn` root is shared with
  `emeraude-engine`.
- **Language:** C++20. **Indentation:** tabs.
- **Platform:** cross-platform strict (Linux, macOS, Windows).

> [!IMPORTANT]
> **A real split, not a sub-folder.** emeraude-base and emeraude-engine are two separate
> repositories with **independent lifecycles**. emeraude-base **evolves on its own** — its
> own versioning and roadmap, new formats/capabilities added without waiting on the engine.
> The engine **builds on a pinned emeraude-base package**: it consumes the base, it does
> not drive its evolution. Using the engine implies using base, but base is equally
> consumable on its own — a tool, a baker, a converter may need only `EmEn::Base::Math` or
> only image handling, **without the full engine** (no Vulkan, no GLFW, no OpenAL).

## 2. Position in the project

```
ext-deps-generator   → builds external deps (zlib, sndfile, freetype, jsoncpp, …)
                       as per-config static libs under output/<config>/{include,lib}
        ↓ (symlinked into dependencies/)
emeraude-base        → THIS repo. Base utilities + platform/config headers.
                       Single source of truth for resolving the external deps
                       (exposes EMERAUDE_EXT_LIBS_PATH / _INCLUDE_DIR / _LIB_DIR).
        ↓ (add_subdirectory)
consumers            → emeraude-engine (full), standalone tools (per-module),
                       projet-alpha (through the engine).
```

## 3. CMake architecture — which target to link

> [!IMPORTANT]
> **Targets available NOW (link these):**
> - **`emeraude::base`** — the whole foundation (umbrella, `STATIC` or `SHARED` per
>   `EMERAUDE_BASE_LIBRARY_TYPE`). **This is the one to use** for any consumer that wants
>   math + factories + I/O + threading, etc. The engine links this.
> - **`emeraude::base::platform`** — header-only `INTERFACE` target, just the
>   platform/arch/OS detection (`emeraude_platform.hpp`). Link it when you need *only* that.
>
> **Do NOT link `emeraude::base::math`, `::io`, `::pixel`, … yet — they do NOT exist.**
> The per-module split is **planned** (see §7 and [`docs/module-map.md`](docs/module-map.md)):
> until it lands, the single umbrella `emeraude::base` is the way to consume the library.

Planned design (internal refactor, no API break — `emeraude::base` keeps working):
- Header-only modules → `INTERFACE` targets; compiled modules → `OBJECT` libraries; each
  aliased `emeraude::base::<name>` and aggregated into the `emeraude::base` umbrella, so a
  consumer will be able to link only what it needs (e.g. `emeraude::base::math`).

## 3a. Shared precompiled header (STL hot-set)

emeraude-base owns the **project-wide precompiled header**, the same way it owns the
external-dependency Setup scripts and the compile policy: a single source of truth reused
by every consumer.

- **`STLPrecompiledHeaders.hpp`** (repo root) — **STL only** (containers, memory, streams,
  threading, `<filesystem>`, …). It also defines `_USE_MATH_DEFINES` before `<cmath>` so MSVC
  exposes `M_PI` etc. to every PCH-using TU (the PCH is force-included, so a TU's own define
  arrives too late). The strict rule lives in the file header: **never** add a project header
  (base's own or a consumer's) nor a third-party header — editing one would invalidate the PCH
  and force a full rebuild. Consumer-specific heavy headers (Eigen for app_kernel, CEF for
  app_system) belong in that consumer's own PCH, layered on top.
- **`cmake/EnablePrecompiledHeaders.cmake`** — exposes `emeraude_base_target_enable_pch(target)`.
  Attaches **only** the shared STL hot-set, **PRIVATE** (never propagates to consumers) and
  **CXX-only** (`$<COMPILE_LANGUAGE:CXX>`, so it never lands on C/ASM TUs). Each target compiles
  its own PCH binary — **no `REUSE_FROM`**, so targets with divergent compile definitions never
  clash. base stays agnostic of consumer-specific headers: a consumer that needs heavy
  third-party headers on top appends them with its own second `target_precompile_headers()` call.
- **`EMERAUDE_BASE_PCH_FILE`** (cache) — absolute path to the header, set next to
  `EMERAUDE_BASE_CMAKE_DIR`. **`EMERAUDE_ENABLE_PCH`** (option, default **On** since 2026-07)
  gates the whole feature; when Off the helper is a no-op.
- **Applied across the whole cascade.** `emeraude_base_target_enable_pch()` is now called right
  after each target is declared: base's own compiled modules + the `emeraude_base` umbrella + the
  unit-test / benchmark executables (here), the **engine** SHARED library, and projet-alpha's
  executable + CEF helper. Plus the external consumers below. One switch, `EMERAUDE_ENABLE_PCH`.
  - **WINDOWS — RESOLVED (2026-07).** The predicted `WINDOWS_EXPORT_ALL_SYMBOLS` + PCH failure
    was real: the PCH object's marker symbols leak into the auto-generated `exports.def` and
    break the DLL link (`LNK2001` on a bogus `__`). Fixed the intended way — the engine's
    explicit-export migration is complete (`EMERAUDE_USE_EXPLICIT_EXPORTS` default **On**,
    `EMERAUDE_API` on the consumer-referenced surface) and C4251/C4275 are disabled in
    `EMERAUDE_COMPILE_OPTIONS` (MSVC) per decision "2b" (no `EMERAUDE_BASE_API`: base types stay
    unexported, executables keep their static base copy — see
    `emeraude-engine/docs/windows-export-api.md`). The MSVC guard in
    `EnablePrecompiledHeaders.cmake` is **kept**: it silently disables the PCH for anyone
    reverting to `EMERAUDE_USE_EXPLICIT_EXPORTS=OFF`. Full MSVC cascade verified (build + link
    with PCH). Linux verified earlier: ≈12 % faster in Release, 1874-test suite green.
  - **GCC false positive.** Enabling the PCH shifts the STL inlining context enough to trip GCC 14's
    `-Wstringop-overread` on a `std::string` move whose inferred length exceeds the SSO buffer (seen
    in the engine's `Saphir/LightGenerator.cpp`). Fix at the source — force a heap buffer with
    `reserve()` — never silence the warning. See engine `docs/caution-points.md`.
  - **app_kernel** — calls the base helper directly (guarded `if (EMERAUDE_BASE_CMAKE_DIR)` in its
    `CMakeLists.txt`, right after `add_library`). STL hot-set only for now; a second
    `target_precompile_headers` call for an Eigen-only layer remains a possible follow-up. Kernel
    is STATIC, linked into an app_system **executable** (no export-all), so its PCH object is
    never export-scanned.
  - **app_system** — `cmake/EnableAppSystemPCH.cmake` applies the base helper to the browser
    binary and the renderer (helper) binary. STL hot-set only for now; consumer-specific layers
    (CEF, CEF + Eigen) remain possible follow-ups. Both are **executables**. On macOS, `.mm`/`.m`
    sources opt out via `SKIP_PRECOMPILE_HEADERS` (the CXX genex does not exclude Objective-C++).
  - All gated by `EMERAUDE_ENABLE_PCH` — one project-wide switch, no per-repo PCH option.

## 4. Core Axioms

1. **Agnostic foundation.** Base depends on NOTHING high-level. No engine subsystem
   (Graphics, Scenes, Physics, Audio runtime, …) may ever be `#include`-d here. The
   dependency arrow only points *into* Base, never out.
2. **External deps via ext-deps-generator only.** Never hardcode or vendor an external
   library path; resolve through `EMERAUDE_EXT_LIBS_PATH`.
3. **Stability matters.** Everything depends on Base — a bug here ripples everywhere.
   Exhaustive tests, careful API.

## 5. Conventions (project-wide)

- **Tabs** for indentation, never spaces.
- **Acronyms always uppercase** in identifiers: `TLAS`, `VBO`, `MD5`, `CRC32`.
- **No public data members** — getters/setters instead.
- **Booleans last** in class/struct member lists (avoids padding holes).
- **Always use braces**, even for single-line `if`/`for`/`while`.
- **Include layout** in `.hpp`: `/* Project config */ /* STL */ /* Third-party */ /* Local for inheritances */ /* Local for usages */`; in `.cpp` the self-header first. Empty sections are dropped (comment included).
- Use the library's own types everywhere (`EmEn::Base::Math::Vector`, `EmEn::Base::PixelFactory::Color`), not raw `float x,y,z`.

## 6. Documentation directive (doc-first)

> [!IMPORTANT]
> AI documentation maintenance is **as important as the code**. Every change ships
> with its doc update in the same session. After modifying a module, update its
> `AGENTS.md` and any affected `docs/` file, and tell the user what changed.

- Each module gets an `src/<Module>/AGENTS.md` as it is migrated/created.
- Cross-cutting topics live in `docs/`.
- The doc index is [`docs/ai_documentation_map.md`](docs/ai_documentation_map.md).

## 7. Status

**Extraction from `emeraude-engine/src/Libs/` is complete.** All foundation code, the
test suite and the test fixtures now live here; `emeraude_platform.hpp` and
`emeraude_base_config.hpp` are the root config headers. The engine consumes
`emeraude::base` and inherits its external dependencies, compile options and language
standards. Build is green; the test suite (`EmeraudeBaseUnitTests`) passes.

Today the library ships as a **single umbrella target** `emeraude::base`
(STATIC by default, SHARED via `EMERAUDE_BASE_LIBRARY_TYPE`), producing `libEmeraudeBase`.
`emeraude::base::platform` is also exposed as a header-only INTERFACE target.

**Planned (internal refactor, no consumer impact):** split the umbrella into per-module
targets so a consumer can link only what it needs:

| Kind | Modules |
|------|---------|
| INTERFACE (header-only) | math, algorithms, animation, platform |
| OBJECT | core, hash, gametools, time, debug, compression, io, network, pixel, vertex, wave |

See [`docs/module-map.md`](docs/module-map.md) for the full mapping.