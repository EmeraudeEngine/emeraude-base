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