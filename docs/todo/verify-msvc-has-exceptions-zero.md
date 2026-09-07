---
id: verify-msvc-has-exceptions-zero
title: Verify the MSVC no-exceptions policy (`_HAS_EXCEPTIONS=0`, `/wd4530` removed) on a Windows toolchain
status: open
priority: high
scope: CMakeLists.txt (EMERAUDE_COMPILE_DEFINITIONS / EMERAUDE_COMPILE_OPTIONS, MSVC branch)
opened: 2026-09-08
tags: [windows, msvc, build, exceptions]
---

# Verify the MSVC no-exceptions policy (`_HAS_EXCEPTIONS=0`, `/wd4530` removed) on a Windows toolchain

## Why

On 2026-09-08 the owner chose to make the MSVC build a real `-fno-exceptions` counterpart:
`_HAS_EXCEPTIONS=0` was added to `EMERAUDE_COMPILE_DEFINITIONS` and `/wd4530` removed from
`EMERAUDE_COMPILE_OPTIONS` (both under `EMERAUDE_DISABLE_EXCEPTIONS`). The change was written on a
Linux workstation and **has not been compiled by MSVC yet**. The engine's two Windows-only
`try`/`catch` sites were rewritten at the same time (`Helpers.windows.cpp`, `SystemInfo.windows.cpp`,
also uncompiled here).

## What remains

1. Configure + build the whole cascade with MSVC (recipe: projet-alpha `docs/`, Ninja + `vcvars64.bat`).
2. Confirm **zero C4530** with `/WX` on. Any hit is a `try` in a `/EHs-` TU: fix the code, never the flag.
3. Check `ThreadPool.hpp` / `StaticVector.hpp`: their `#if __cpp_exceptions` branches must stay
   compiled OUT under `/EHs-` (MSVC defines `__cpp_exceptions` only when `_CPPUNWIND` is set).
4. Link + run `projet-alpha.exe` and `EmeraudeBaseUnitTests`: `_HAS_EXCEPTIONS=0` changes the
   `std::exception` definition, so a prebuilt C++ third-party library compiled with the default
   (`_HAS_EXCEPTIONS=1`) is an ODR/ABI suspect — list the C++ archives in the Windows ext-libs
   bundle and the CEF wrapper flags, and exercise a native file dialog (`_beginthreadex` path).
5. Record the outcome in `docs/error-handling.md` § 1 (drop the "unverified" note), then delete this file.

## ⚠️ Traps

- A green build with `EMERAUDE_DISABLE_PARANOID_COMPILATION=On` proves nothing: `/WX` is off there.
- `-j$(nproc)` does not expand under `cmd`; write the count.

## References

- `docs/error-handling.md` § 1, `docs/caution-points.md` § Build / Compiler (MSVC entry).
- emeraude-engine `docs/cpp-conventions.md` § "No Exceptions — Anywhere, On Every Platform".
