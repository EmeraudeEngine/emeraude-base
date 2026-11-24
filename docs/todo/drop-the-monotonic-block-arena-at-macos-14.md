---
id: drop-the-monotonic-block-arena-at-macos-14
title: Drop MonotonicBlockArena once every supported macOS has std::pmr (14.0)
status: blocked
priority: unranked
scope: src/MonotonicArena.hpp, src/Testing/test_MonotonicArena.cpp, CMakeLists.txt (APP_OSX_DEPLOYMENT_TARGET)
opened: 2026-11-24
tags: [macos, cpp, cleanup]
---

# Drop MonotonicBlockArena once every supported macOS has std::pmr (14.0)

## Why

`MonotonicArena` is `std::pmr::monotonic_buffer_resource` everywhere except on a macOS deployment target below 14.0,
where Apple's libc++ does not export std::pmr and `MonotonicBlockArena` stands in
(`docs/subsystems/source-tree/30-monotonic-arena.md`). The fallback is a second allocator to keep and test for one
platform floor.

Blocked by an external condition, not by another item: the base's default deployment target is 13.0 (`CMakeLists.txt`),
and its consumers target 13.0 too (2026-11-24) — the floor CEF 156 imposes on the applications that embed it.

## What remains

When the base's default and every consumer's `APP_OSX_DEPLOYMENT_TARGET` are 14.0 or more:
- `EMERAUDE_STD_PMR_AVAILABLE` is then 1 on every build: replace `MonotonicArena.hpp` by a plain
  `using MonotonicArena = std::pmr::monotonic_buffer_resource;` (or use the standard type directly in
  `ShapeDecimator.hpp`), delete `MonotonicBlockArena` and `test_MonotonicArena.cpp` (and its line in
  `cmake/PrepareBaseSourceFiles.cmake`);
- update the source-tree doc 30, `src/AGENTS.md`, the caution point § Build / Compiler *`std::pmr` on macOS before
  14.0* (keep the trap: it is still true for any other older target), and delete this file.

## ⚠️ Traps

- Do not drop it on the base's own default alone: a consumer that sets `APP_OSX_DEPLOYMENT_TARGET` lower (it wins, the
  base follows it) would load-fail again, with no compile error to say so.
- The check that proves it on macOS: `nm -um <binary> | grep pmr` on a build whose target is below 14.0 must stay empty.
