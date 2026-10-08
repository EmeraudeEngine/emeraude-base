## 3a. Shared precompiled header (STL hot-set)

emeraude-base owns the **project-wide precompiled header**, the same way it owns the
external-dependency Setup scripts and the compile policy: a single source of truth reused
by every consumer.

- **`cmake/STLPrecompiledHeaders.cmake`** — defines **`EMERAUDE_BASE_STL_PCH_HEADERS`**, the
  **STL-only** hot-set as a CMake list of `"<header>"` entries — **deliberately short**: the
  heaviest, most ubiquitous headers only, since every entry enlarges the per-target PCH binary and
  the time to build it. The strict rule lives in the file header: **never** add a project
  header (base's own or a consumer's) nor a third-party header — editing one would invalidate the
  PCH and force a full rebuild. Consumer-specific heavy headers (Eigen, CEF in downstream
  applications) are passed **alongside** this list at the call site.
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
  no build passes a `-D` for it, so base and its downstream consumers all compile with their PCH.
  Turning it off is a deliberate `-DEMERAUDE_ENABLE_PCH=OFF`, worth doing periodically: the PCH
  masks missing `#include`s, and **both configs must stay green** (see
  [`docs/caution-points.md`](../caution-points.md)).
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
    and their correct fixes are in [`docs/caution-points.md`](../caution-points.md).
