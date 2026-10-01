# clang-tidy ledger — emeraude-base

> Owner decision 2026-09-30, plan Ave Robustus § 3 (projet-alpha `docs/plans/ave-robustus.md`). The configuration is
> this repository's `.clang-tidy`. Gate on every change: ZERO NEW finding on the touched translation units; a finding
> is fixed, never silenced. A FULL run is a long process ordered by the owner; its results are recorded here.

## How to run

**Version: clang-tidy 21.1.6** (owner decision 2026-09-30). clang-tidy 19 (Debian's) SEGFAULTS in
`modernize-use-designated-initializers` on engine `src/CoreTypes.hpp` (`EngineContext`: a reference member
brace-initialised with a forward-declared type) — 8 of the 12 Console TUs, most of the engine. 21 does not crash.
Without sudo, from PyPI (the official LLVM binaries) in a scratch virtual environment:

```bash
python3 -m venv <scratch>/ctvenv && <scratch>/ctvenv/bin/pip install "clang-tidy==21.1.6"
# One translation unit, with the compile database of a Claude build directory (never cmake-build-*) — cleaned of the
# GCC-only flags (-flto*, -fno-fat-lto-objects, -fuse-linker*) and with clang++ as the driver:
<scratch>/ctvenv/bin/clang-tidy -p <cleaned db dir> --quiet <file.cpp> > tidy.log 2>&1; echo EXIT=$?
```

Redirect, never pipe. Count the findings by check (`[check-name]` at the end of each warning line), keeping only
those whose file is inside the module (the header filter also reports every included header).

## Last full run per module

| Module | Date | Findings by check | Notes |
|---|---|---|---|
| — | — | no full run recorded yet under this ledger | |
| `src/FastJSON.hpp` (checked conversions, engine triad 6c) | 2026-09-30 | clang-tidy 21.1.6 through the engine TUs: 0 new finding on the changed lines (a readability-simplify-boolean-expr refused — its De Morgan form would let NaN through the integral range test: rewritten with an explicit `std::isfinite()`). The file's pre-existing `std::endl` debug lines (performance-avoid-endl) are left for a base pass. | Partial (one header) |
| `src/Math/Space3D/OrientedBox.hpp`, `Contacts/` (physics overhaul P1) | 2026-10-01 | clang-tidy 21.1.6 through `test_MathSpace3DContacts.cpp`. Box ↔ box: 28 findings fixed (22 readability-math-missing-parentheses, 2 modernize-use-auto, 3 cppcoreguidelines-pro-bounds-constant-array-index loops rewritten, 1 readability-use-anyofallof raised by a rewrite). Sphere ↔ box and capsule ↔ box: 26 fixed (21 constant-array-index removed by using `Vector< 3 >` triples, 2 nested conditionals → `ContactsDetail::regionDigit()`, 1 parenthesis, 2 modernize-use-std-numbers). Triangles and round shapes: 4 fixed (the edge loops rewritten over an array of edge pairs). 1 kept ON PURPOSE (below). | Partial (new files) |
| `src/Math/Matrix.hpp`, `Math/Space3D/` (engine triad 11) | 2026-10-01 | clang-tidy 21.1.6 through the engine physics TUs: 0 new finding. Fix-its applied in the headers: 7 `readability-isolate-declaration` (`OrientedCuboid.hpp`, `CapsuleCuboid.hpp`, `SamePrimitive.hpp`, `SAT.hpp`). `SamePrimitive.hpp` `denom` un-nested (~90 parenthesis levels, item `runaway-nested-parentheses`). | Engine triad 11 |

## Findings kept ON PURPOSE

Each with its file:line, its check and the reason (an owner decision).

| File:line | Check | Reason |
|---|---|---|
| `src/Math/Space3D/OrientedBox.hpp` `axis(size_t index)` | `cppcoreguidelines-pro-bounds-constant-array-index` | An indexed accessor with `@pre index < 3`, used by the SAT loops of `Contacts/BoxBox.hpp`; three named accessors would make every loop heavier. Owner decision 2026-10-01: kept, as the engine keeps this check's bounded indices. |
