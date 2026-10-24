---
id: iwyu-debt-since-2026-10-21
title: IWYU debt — the 5 C++ files touched while include-what-you-use did not run (2026-10-21 to 2026-10-25)
status: open
priority: high
scope: emeraude-base src/
opened: 2026-10-25
tags: [iwyu, static-analysis, gate, debt]
---

# IWYU debt — the 5 C++ files touched while include-what-you-use did not run

## Why

From emeraude-base 145fda6 (2026-10-21, 16:51) to the fix of 2026-10-25, IWYU aborted every TU on the base mapping's
`<features.h>` entry and the gate read the empty output as "IWYU-clean" (static-analysis ledger § include-what-you-use,
this ledger). The files below were touched in that window and were never really checked. Owner decision (2026-10-25): the
fix first, this debt as an item; each lot cleans the files it touches, and a dedicated pass empties the rest when the
owner orders it.

## What remains

- [ ] Run `iwyu_tool` on the files below (the command of the ledger § include-what-you-use), apply with projet-alpha
      `tools/iwyu-apply.py`, add the forward declarations by hand, build the three cascades, run the pass again: it
      must converge. Remove a file from the list when a lot brings it clean.
- [ ] A removed include is checked against the file's platform branches (`#ifdef _WIN32`, macOS, `__cpp_exceptions`)
      before it goes; the peers build it.

The files (as of 2026-10-25; the sky lots' files were brought clean that day):

- `src/Math/Astronomy.hpp`
- `src/PixelFactory/TextureName.cpp`
- `src/PixelFactory/TextureName.hpp`
- `src/Testing/test_PixelFactoryTextureName.cpp`
- `src/Testing/test_TemplateInstantiation.cpp`

## ⚠️ Traps

- A forward declaration IWYU proposes for a type held BY VALUE in a member (`std::array< T, N >`) or used in a template
  body is wrong: keep the include with `// IWYU pragma: keep (<reason>)`.
- IWYU sends `std::size_t` to `<cstdint>` when that header is already needed: libstdc++ only; keep `<cstddef>`.
