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

- **`cmake/STLPrecompiledHeaders.cmake`** — defines **`EMERAUDE_BASE_STL_PCH_HEADERS`**, the
  **STL-only** hot-set as a CMake list of `"<header>"` entries — **deliberately short**: the
  heaviest, most ubiquitous headers only, since every entry enlarges the per-target PCH binary and
  the time to build it. The strict rule lives in the file header: **never** add a project
  header (base's own or a consumer's) nor a third-party header — editing one would invalidate the
  PCH and force a full rebuild. Consumer-specific heavy headers (Eigen for app_kernel, CEF for
  app_system) are passed **alongside** this list at the call site.
  > The hot-set is a **CMake variable, never a header file**: a consumer composes it with its own
  > headers at the call site, without needing a header of its own.
- **`cmake/EnablePrecompiledHeaders.cmake`** — exposes
  `emeraude_base_target_enable_pch(<target> "<header-list>")` and `include()`s the hot-set above, so
  a single `include(EnablePrecompiledHeaders)` brings both the list and the function. The list is
  **explicit at the call site** — nothing is attached implicitly — and is a **named parameter, so it
  must be ONE quoted argument**:
  `emeraude_base_target_enable_pch(MyTarget "${EMERAUDE_BASE_STL_PCH_HEADERS}")`. Stack a consumer
  layer by concatenating: `"${EMERAUDE_BASE_STL_PCH_HEADERS};${MY_EIGEN_HEADERS}"`.
  > **Always quote the list.** Unquoted, only the first header binds to the parameter and every
  > other one spills into `ARGN`, which the helper ignores — you get a single-header PCH with **no
  > diagnostic**. The helper deliberately carries no guard for this (it trusts its callers); the
  > symptom to recognise is a build that is mysteriously no faster with `EMERAUDE_ENABLE_PCH=ON`.
  Applied **PRIVATE** (never propagates to consumers). Each target compiles its own PCH binary —
  **no `REUSE_FROM`**, so targets with divergent compile definitions never clash.
  **No export-all guard here (moved out 2026-08):** on MSVC a SHARED library using
  `WINDOWS_EXPORT_ALL_SYMBOLS` must not get a PCH (marker symbols leak into the generated
  `exports.def` → `LNK2001`), but that concerns the *caller's* own link mode, so the check belongs at
  the call site — see the engine's `CMakeLists.txt`. Keeping it here punished every unrelated target.
  **Repeated calls append, with no de-duplication** — the helper trusts its callers: pass each
  header exactly once. Attaching the same list twice would just duplicate the `#include`s in the
  generated `cmake_pch.hxx` (harmless, include guards, but pointless).
  **No `$<COMPILE_LANGUAGE:CXX>` wrapping** (dropped 2026-08): a genex cannot carry a system
  header — the `>` of `<vector>` closes it early. The whole cascade is C++-only (audited: zero
  `.c`/`.S`/`.asm` in any PCH-attached target), so the restriction was unnecessary. If a C or
  assembly source ever joins such a target, extend the `SKIP_PRECOMPILE_HEADERS` sweep — do not
  bring the genex back.
  **Objective-C(++) sources are auto-skipped (2026-07):** the helper sets
  `SKIP_PRECOMPILE_HEADERS ON` on every `.m`/`.mm`/`.M` in the target's `SOURCES`. Nothing else
  can exclude them — without `enable_language(OBJCXX)` CMake classifies `.mm` as CXX, yet clang
  compiles them as Objective-C++ (extension-driven) and rejects the pure-C++ PCH
  (`error: Objective-C was disabled in PCH file but is currently enabled`). Consumers need no
  manual opt-out; sources added to the target *after* the helper call are the one case still
  needing a manual `SKIP_PRECOMPILE_HEADERS`.
  > **KNOWN LIMITATION — the sweep does not reach a target owned by another directory when its
  > sources are declared relative.** `SOURCES` yields paths exactly as the target declared them,
  > and `set_source_files_properties` resolves a relative one against the **caller's** directory,
  > never the target's — the property then lands on `<consumer-root>/<relative-path>`, which
  > nothing compiles, and the opt-out silently does nothing (no diagnostic: CMake does not require
  > a source property's file to exist). `TARGET_DIRECTORY` does not help; it scopes the property's
  > *visibility*, not the path resolution. Same-directory targets and absolute source lists are
  > unaffected, which is what makes the failure cross-directory-and-macOS-only.

- **`EMERAUDE_ENABLE_PCH`** (option, default **On**) gates the whole feature; when Off the helper
  is a no-op and the call sites stay unconditional. It is **live by default across the cascade** —
  no build passes a `-D` for it, so base, app_kernel and app_system all compile with their PCH.
  Turning it off is a deliberate `-DEMERAUDE_ENABLE_PCH=OFF`, worth doing periodically: the PCH
  masks missing `#include`s, and **both configs must stay green** (see
  [`docs/caution-points.md`](docs/caution-points.md)).
- **Applied across the whole cascade.** `emeraude_base_target_enable_pch()` is now called right
  after each target is declared: base's own compiled modules + the `emeraude_base` umbrella + the
  unit-test / benchmark executables (here), the **engine** SHARED library, and projet-alpha's
  executable + CEF helper. Plus the external consumers below. One switch, `EMERAUDE_ENABLE_PCH`.
  **The single target that opts out is the engine, and only on MSVC** — its own guard, because the
  default `EMERAUDE_USE_EXPLICIT_EXPORTS=Off` restores `WINDOWS_EXPORT_ALL_SYMBOLS` (next bullet).
  On Linux/macOS the engine keeps its PCH like everyone else.
  - **WINDOWS — RESOLVED (2026-07).** The predicted `WINDOWS_EXPORT_ALL_SYMBOLS` + PCH failure
    was real: the PCH object's marker symbols leak into the auto-generated `exports.def` and
    break the DLL link (`LNK2001` on a bogus `__`). Solved the intended way — the engine's
    explicit-export migration is complete (`EMERAUDE_API` on the consumer-referenced surface,
    verified with the PCH on) and C4251/C4275 are disabled in
    `EMERAUDE_COMPILE_OPTIONS` (MSVC) per decision "2b" (no `EMERAUDE_BASE_API`: base types stay
    unexported, executables keep their static base copy — see
    `emeraude-engine/docs/windows-export-api.md`). The MSVC export-all guard is **kept but no longer
    lives here** (moved 2026-08 to the engine's own call site): the incompatibility concerns only a
    SHARED library using `WINDOWS_EXPORT_ALL_SYMBOLS`, i.e. the engine target alone — every other
    PCH target in the cascade is `STATIC`/`OBJECT` or an executable and is never export-scanned, so
    a guard in this shared helper stripped their PCH for nothing as soon as
    `EMERAUDE_USE_EXPLICIT_EXPORTS` went OFF. **Do not move it back**: the option belongs to the
    engine, not to the foundation. A consumer declaring a SHARED library with export-all must carry
    the same guard at its own call site. Full MSVC cascade verified (build + link
    with PCH). Linux verified earlier: ≈12 % faster in Release, 1874-test suite green.
  - **GCC false positive.** Enabling the PCH shifts the STL inlining context enough to trip GCC 14's
    `-Wstringop-overread` on a `std::string` move whose inferred length exceeds the SSO buffer (seen
    in the engine's `Saphir/LightGenerator.cpp`). Fix at the source — force a heap buffer with
    `reserve()` — never silence the warning. A sibling `-Wstringop-overflow` variant hits base
    under `_FORTIFY_SOURCE=2` (fixed by `+=` on a named local, no `reserve` needed) — both triggers
    and their correct fixes are in [`docs/caution-points.md`](docs/caution-points.md).

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

## 6b. PixelFactory — supported image formats

| Format | Read | Write | Backend |
|--------|------|-------|---------|
| JPEG (`.jpg`, `.jpeg`) | ✅ | ✅ | libjpeg-turbo |
| PNG (`.png`) | ✅ | ✅ | libpng |
| Targa (`.tga`) | ✅ | ✅ | in-house |
| HDR / RGBE (`.hdr`) | ✅ | ✅ | in-house — the only floating-point citizen |
| **TIFF (`.tif`, `.tiff`)** | ✅ | ❌ | **libtiff (added 2026-08-09)** |

**Why TIFF was added, breaking the "Ave robustus!" freeze on purpose (owner decision,
2026-08-09):** Intel's Jungle Ruins — the engine's gold-goal scene — stores the base colour and
translucency of its **entire vegetation** as 16-bit TIFF. Ten files, all under
`textures/Plants/`. Without a decoder those materials fall back to a flat colour and every plant
renders lit, detailed and pure WHITE, however much work goes into instancing. Pre-converting the
asset was excluded by a standing owner rule: a load failure is an engine defect to fix, never an
asset to pre-chew.

> [!WARNING]
> **TIFF is read-only here, and deliberately.** It is an interchange format — DCC tools emit it,
> the engine consumes it. PNG covers lossless output, HDR covers floating point.

> [!CAUTION]
> **A 16-bit TIFF is DOWN-CONVERTED to 8 bits per channel.** `FileFormatTIFF` decodes through
> `TIFFReadRGBAImageOriented()` rather than the strip/tile API, because TIFF is a container
> rather than a format: any bit depth, any photometric interpretation, a dozen compressions,
> strips or tiles, planar or interleaved. That entry point collapses all of it to 8-bit RGBA,
> which is what a texture becomes anyway. Full precision would be a separate reader, not a flag.

> [!CAUTION]
> `ORIENTATION_TOPLEFT` is what makes the output canonical. The plain `TIFFReadRGBAImage()`
> returns the image **bottom-up**, which renders as a vertically mirrored texture — right shape,
> wrong content, nothing in the log.

> [!CAUTION]
> **12-bit JPEG must be disabled TWICE, and the second one is the trap.** `jpeg12: false` only
> guards the "separate 12-bit libjpeg" branch. The other branch probes `jpeglib.h` with
> `check_symbol_exists()`, and **libjpeg-turbo 3.x DECLARES `jpeg12_read_scanlines` while the
> archive we build does not EXPORT it** — libtiff then believes it has dual mode, compiles
> `tif_jpeg_12.c`, and the link dies on `jpeg12_*`. The cache entry
> `HAVE_JPEGTURBO_DUAL_MODE_12: false` is what stops the probe.
>
> ⚠️⚠️ **This did NOT break the cascade build** — the shared library never pulled that object in.
> It surfaced only when linking `EmeraudeBaseUnitTests`. That is precisely why verification is
> done in two steps (cascade, then unit suite): step 1 passing is not evidence that step 2 will.

> [!CAUTION]
> **The WebP and JBIG codecs are OFF in the libtiff build** (`libraries/libtiff.yaml`). libtiff
> enables them whenever it finds the libraries, and the engine then fails to link on
> `WebPGetFeaturesInternal` and `jbg_dec_in`. Deflate, LZMA, Zstd and JPEG ride on libraries the
> cascade already links and stay enabled.

> [!IMPORTANT]
> **A format this table does not list must be rejected BEFORE the resource manager sees it.**
> An `ImageResource` that fails to load takes its texture and then its material down with it, so
> the object stops rendering ENTIRELY — strictly worse than a missing file, which merely falls
> back to a flat colour. `Scenes::Loaders::USDLoader` checks the extension first for that reason.

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