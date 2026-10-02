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
| `src/Math/Space3D/OrientedBox.hpp`, `Contacts/` (physics overhaul P1) | 2026-10-01 | clang-tidy 21.1.6 through `test_MathSpace3DContacts.cpp`. Box ↔ box: 28 findings fixed (22 readability-math-missing-parentheses, 2 modernize-use-auto, 3 cppcoreguidelines-pro-bounds-constant-array-index loops rewritten, 1 readability-use-anyofallof raised by a rewrite). Sphere ↔ box and capsule ↔ box: 26 fixed (21 constant-array-index removed by using `Vector< 3 >` triples, 2 nested conditionals → `ContactsDetail::regionDigit()`, 1 parenthesis, 2 modernize-use-std-numbers). Triangles and round shapes: 4 fixed (the edge loops rewritten over an array of edge pairs). Casts: 0. `RigidBody.hpp` (through `test_MathRigidBody.cpp`): 0 in the header; 13 bugprone-unchecked-optional-access in the test (gtest's ASSERT is opaque to it) fixed with `value_or()`. 1 kept ON PURPOSE (below). | Partial (new files) |
| `src/Math/Space3D/Casts/ConvexDistance.hpp` (new), `Casts/ShapeCast.hpp` (`castBox()`) + `test_MathSpace3DConvexDistance.cpp` (physics overhaul P5) | 2026-10-02 | clang-tidy 21.1.6 through the test and the engine TUs: 0 finding on the new and changed lines. | Engine P5, decision 14 |
| `src/Math/Space3D/TriangleMesh.hpp` + its test (physics overhaul P5) | 2026-10-02 | clang-tidy 21.1.6 through `test_MathSpace3DTriangleMesh.cpp` and the engine TUs: 8 fixed (2 constant-array-index on the edge slots → three explicit edges, 1 nested conditional → a switch, 2 missing-std-forward → the visitors by const reference, 3 constant-array-index in the test). Kept ON PURPOSE (below): 7 `cppcoreguidelines-pro-bounds-constant-array-index`, 6 `bugprone-dynamic-static-initializers`. | Engine P5 |
| `src/Math/Matrix.hpp`, `Math/Space3D/` (engine triad 11) | 2026-10-01 | clang-tidy 21.1.6 through the engine physics TUs: 0 new finding. Fix-its applied in the headers: 7 `readability-isolate-declaration` (`OrientedCuboid.hpp`, `CapsuleCuboid.hpp`, `SamePrimitive.hpp`, `SAT.hpp`). `SamePrimitive.hpp` `denom` un-nested (~90 parenthesis levels, item `runaway-nested-parentheses`). | Engine triad 11 |

## Findings kept ON PURPOSE

Each with its file:line, its check and the reason (an owner decision).

| File:line | Check | Reason |
|---|---|---|
| `src/Math/Space3D/TriangleMesh.hpp` `visit()` stack, `splitPosition()` bins | `cppcoreguidelines-pro-bounds-constant-array-index` | Bounded by construction: the query stack holds at most the depth (`MaxDepth + 2` slots, one push per inner level); a bin index is `min(bin, Bins - 1)`, a boundary loop runs in `[0, Bins - 1)`. A checked container would add a test per node of a per-query walk. 2026-10-02, as the engine keeps this check's bounded indices. |
| `src/Math/Space3D/TriangleMesh.hpp` its `static constexpr` members | `bugprone-dynamic-static-initializers` | A false positive on a class template: the members are `constexpr` (constant-initialized); the check cannot evaluate a dependent initializer and reports every templated constant of the cascade the same way (`Math/Base.hpp`, the engine's `Constants.hpp`). 2026-10-02. |
| `src/Math/Space3D/OrientedBox.hpp` `axis(size_t index)` | `cppcoreguidelines-pro-bounds-constant-array-index` | An indexed accessor with `@pre index < 3`, used by the SAT loops of `Contacts/BoxBox.hpp`; three named accessors would make every loop heavier. Owner decision 2026-10-01: kept, as the engine keeps this check's bounded indices. |
