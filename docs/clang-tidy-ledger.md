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
| `src/Network/HTTPSClient.*` (keep-alive pool), `TLSConnection.*` (`isOpenAndIdle()`, close_notify grace), `src/IO/IO.*` (`systemTreePath()`), tests `test_NetworkHTTPSClient.cpp`, `test_NetworkHTTPSClientLive.cpp`, `test_IO.cpp`, `TLSTestHelpers.hpp` | 2026-10-07 | clang-tidy 21.1.6: 12 on the changed lines, fixed (1 modernize-use-designated-initializers on `ConnectionKey`, 7 misc-const-correctness and 3 bugprone-unchecked-optional-access in the new tests, 1 pro-type-reinterpret-cast in the live test). 0 left. | base httpsclient-keep-alive, windows-long-paths |
| `src/PixelFactory/FileFormatHDR.hpp` (legacy RLE shift bound), `src/Fuzzing/fuzz_hdr.cpp` (new), test `test_PixelFactoryFileFormats.cpp` | 2026-10-07 | clang-tidy 21.1.6: 0 on the changed lines (the fuzz target is not in the compile database; read by hand). | base fuzz-hdr-target |
| `src/Math/Space3D/SAT.hpp`, `Collisions/SamePrimitive.hpp`, `Collisions/CapsuleTriangle.hpp`, `Collisions/CapsuleCuboid.hpp` (overlaps on the exact manifolds), test `test_MathSpace3D.cpp` | 2026-10-07 | clang-tidy 21.1.6: 0 on the changed lines. | base collision-pair-test-defects |
| `src/Network/HTTPServer.cpp` / `.hpp` (network thread on `Base::Thread`) | 2026-10-07 | clang-tidy 21.1.6: 0 on the changed lines. | base non-throwing-thread-start |
| `src/ThreadPool.cpp` / `.hpp` (workers on `Base::Thread`, the calling thread runs the tasks without a worker), `src/Time/TimedEvent.hpp`, `EventTrait.hpp` (`start()` returns bool), tests `test_ThreadPool.cpp`, `test_Time.cpp` | 2026-10-07 | clang-tidy 21.1.6: 1 on the changed lines, fixed (performance-inefficient-vector-operation in a test). 0 left. | base non-throwing-thread-start |
| `src/Thread.hpp` / `.cpp` (new), test `test_Thread.cpp` (new) | 2026-10-07 | clang-tidy 21.1.6: 3 fixed (unused-return-value on the task hand-over, avoid-non-const-global-variables: the test seam's counter made function-static, forwarding-reference-overload: `Task` constrained). 2 ON PURPOSE in `test_Thread.cpp` (bugprone-use-after-move, reported twice with clang-analyzer-cplusplus.Move): the test asserts the moved-from object is idle — the documented contract. | base non-throwing-thread-start |
| `src/VertexFactory/Shape.hpp` (`triangleTangentFrame()`, `vertexTangentHandedness()`), `ShapeTriangle.hpp` (handedness), `FileFormatOBJ.hpp` (`uvSide()`, 4-entry key), tests `test_VertexFactoryShapeGenerator.cpp`, `test_VertexFactoryFileFormats.cpp`, `test_VertexFactoryShapeSimplifier.cpp` | 2026-10-07 | clang-tidy 21.1.6: 3 on the changed lines, fixed (pro-bounds-constant-array-index ×3 in `uvSide()`: a per-corner lambda instead of an indexed loop). 0 left. | base todo items (high) |
| `src/IO/IO.cpp` / `.hpp` (`systemPath()`, `windowsExtendedLengthPath()`, `renameFile()`, every wrapper through `systemPath()`), test `test_IO.cpp` | 2026-10-07 | clang-tidy 21.1.6: 2 on the changed lines, fixed (modernize-raw-string-literal ×2: the `\\?\` prefixes). 0 left. | base todo items (high) |
| `src/Time/Time.cpp` / `.hpp` (`processCPUTimeResolutionNanoseconds()`), test `test_Debug.cpp` | 2026-10-07 | clang-tidy 21.1.6: 0 on the changed lines (Linux branch; the Windows branch is a constant). | base todo items (high) |
| `src/Time/EventTrait.hpp`, `TimedEvent.hpp` (timers destroyed outside the lock, self-destroy retired), `src/Math/CartesianFrame.hpp`, `Quaternion.hpp` (Tait-Bryan getters, clamped asin), through tests `test_Time.cpp`, `test_MathCartesianFrame.cpp`, `test_MathQuaternion.cpp` | 2026-10-07 | clang-tidy 21.1.6: 4 on the changed lines, fixed (modernize-use-scoped-lock ×4 on the new locks). 0 left. | base todo items (high) |
| `src/Network/GracefulCloser.*` (new), `HTTPServer.*` (graceful close), tests `test_NetworkHTTPServer.cpp` | 2026-10-06 | clang-tidy 21.1.6: 0 on the new files and on the changed lines; then the accounting / fixed-cap fix and the `NetworkGracefulCloser` tests: 5 fixed (readability-redundant-casting on asio's void error_code overloads). 0 left. | Graceful close |
| `src/Network/HTTPServer.*` (new), `TLSConnection.cpp`, `HTTPSClient.cpp`, `Hash/SHA1|256|512.cpp`, tests `test_NetworkHTTPServer.cpp` (new), `test_Hash.cpp` (resource sharing + the hash length fix) | 2026-10-04 | clang-tidy 21.1.6 on new and changed lines: 30 at first, 20 fixed (statusLine static, a body passed as a view, size_t chunk constants, `std::cmp_not_equal` for `gcount()`, `isPrivateNetworkAddress()` no longer recursive, the test's optional reads compared whole). 10 ON PURPOSE: misc-no-recursion on `HTTPServer`'s asynchronous chain (`readRequest` → handler → `respond…` → `writeNext` → `readRequest`): every link is an asio completion handler, never a nested call on one stack — the same 21 the engine MCP server recorded before the code moved here. | Partial (touched files) |
| `src/FastJSON.hpp` (checked conversions, engine triad 6c) | 2026-09-30 | clang-tidy 21.1.6 through the engine TUs: 0 new finding on the changed lines (a readability-simplify-boolean-expr refused — its De Morgan form would let NaN through the integral range test: rewritten with an explicit `std::isfinite()`). The file's pre-existing `std::endl` debug lines (performance-avoid-endl) are left for a base pass. | Partial (one header) |
| `src/Math/Space3D/OrientedBox.hpp`, `Contacts/` (physics overhaul P1) | 2026-10-01 | clang-tidy 21.1.6 through `test_MathSpace3DContacts.cpp`. Box ↔ box: 28 findings fixed (22 readability-math-missing-parentheses, 2 modernize-use-auto, 3 cppcoreguidelines-pro-bounds-constant-array-index loops rewritten, 1 readability-use-anyofallof raised by a rewrite). Sphere ↔ box and capsule ↔ box: 26 fixed (21 constant-array-index removed by using `Vector< 3 >` triples, 2 nested conditionals → `ContactsDetail::regionDigit()`, 1 parenthesis, 2 modernize-use-std-numbers). Triangles and round shapes: 4 fixed (the edge loops rewritten over an array of edge pairs). Casts: 0. `RigidBody.hpp` (through `test_MathRigidBody.cpp`): 0 in the header; 13 bugprone-unchecked-optional-access in the test (gtest's ASSERT is opaque to it) fixed with `value_or()`. 1 kept ON PURPOSE (below). | Partial (new files) |
| `src/Math/Space3D/Casts/ConvexDistance.hpp` (new), `Casts/ShapeCast.hpp` (`castBox()`) + `test_MathSpace3DConvexDistance.cpp` (physics overhaul P5) | 2026-10-02 | clang-tidy 21.1.6 through the test and the engine TUs: 0 finding on the new and changed lines. | Engine P5, decision 14 |
| `src/Math/Space3D/TriangleMesh.hpp` + its test (physics overhaul P5) | 2026-10-02 | clang-tidy 21.1.6 through `test_MathSpace3DTriangleMesh.cpp` and the engine TUs: 8 fixed (2 constant-array-index on the edge slots → three explicit edges, 1 nested conditional → a switch, 2 missing-std-forward → the visitors by const reference, 3 constant-array-index in the test). Kept ON PURPOSE (below): 7 `cppcoreguidelines-pro-bounds-constant-array-index`, 6 `bugprone-dynamic-static-initializers`. | Engine P5 |
| `src/Math/PiecewiseLinear.hpp` (new) + `test_MathPiecewiseLinear.cpp` (the engine's wheeled vehicle) | 2026-10-02 | clang-tidy 21.1.6 through the test and the engine TUs: 1 fixed (bugprone-dynamic-static-initializers: an unused `static constexpr MaxPoints` removed). 0 left. | Vehicle |
| `src/PortableRandom.hpp` (new), `Randomizer.hpp`, `Algorithms/PerlinNoise.hpp`, `VoronoiNoise.hpp` + `test_PortableRandom.cpp` | 2026-10-02 | clang-tidy 21.1.6 through the test and the engine TUs: 3 fixed (missing-std-forward: `shuffle()` takes its range by reference; init-variables; bugprone-integer-division in the uniformity test); kept ON PURPOSE (below): 12 cert-msc51-cpp, 1 pro-bounds-constant-array-index, in the test. | Portable random |
| `src/VertexFactory/ShapeVertex.hpp` (the secondary texture coordinates), `Shape.hpp` (the buffer writers), `ShapeProcessor.hpp` (the dedupe key), `FileFormatNative.hpp` (version 3) + `test_VertexFactoryShapeVertex.cpp`, `test_VertexFactoryFileFormats.cpp`, `test_VertexFactoryShapeBuilder.cpp` | 2026-10-03 | clang-tidy 21.1.6 through the tests and the engine TUs: 0 finding on the new and changed lines (the headers' reported lines are older). | Multi-UV |
| `src/Math/Matrix.hpp`, `Math/Space3D/` (engine triad 11) | 2026-10-01 | clang-tidy 21.1.6 through the engine physics TUs: 0 new finding. Fix-its applied in the headers: 7 `readability-isolate-declaration` (`OrientedCuboid.hpp`, `CapsuleCuboid.hpp`, `SamePrimitive.hpp`, `SAT.hpp`). `SamePrimitive.hpp` `denom` un-nested (~90 parenthesis levels, item `runaway-nested-parentheses`). | Engine triad 11 |

## Findings kept ON PURPOSE

Each with its file:line, its check and the reason (an owner decision).

| File:line | Check | Reason |
|---|---|---|
| `src/Math/Space3D/TriangleMesh.hpp` `visit()` stack, `splitPosition()` bins | `cppcoreguidelines-pro-bounds-constant-array-index` | Bounded by construction: the query stack holds at most the depth (`MaxDepth + 2` slots, one push per inner level); a bin index is `min(bin, Bins - 1)`, a boundary loop runs in `[0, Bins - 1)`. A checked container would add a test per node of a per-query walk. 2026-10-02, as the engine keeps this check's bounded indices. |
| `src/Math/Space3D/TriangleMesh.hpp` its `static constexpr` members | `bugprone-dynamic-static-initializers` | A false positive on a class template: the members are `constexpr` (constant-initialized); the check cannot evaluate a dependent initializer and reports every templated constant of the cascade the same way (`Math/Base.hpp`, the engine's `Constants.hpp`). 2026-10-02. |
| `src/IO/IO.hpp` `fileGetRange()` `file.read()` | `cppcoreguidelines-pro-type-reinterpret-cast` | `std::istream::read()` takes a `char *`: the destination is the caller's `std::byte` vector, sized to the range. 2026-10-04. |
| `src/Testing/test_PortableRandom.cpp` its generators | `cert-msc51-cpp` | A constant seed is the POINT of a golden test (the same numbers on every platform). 2026-10-02. |
| `src/Testing/test_PortableRandom.cpp` `IntegerBoundsAndUniformity` counts | `cppcoreguidelines-pro-bounds-constant-array-index` | The index is `face - 1`, asserted in [1, 6] on the line above. 2026-10-02. |
| `src/Testing/AsanDefaultOptions.cpp` `__asan_default_options` | `bugprone-reserved-identifier` | The name is ASan's documented hook, looked up by its runtime: it cannot be another. 2026-10-02. |
| `src/Math/Space3D/OrientedBox.hpp` `axis(size_t index)` | `cppcoreguidelines-pro-bounds-constant-array-index` | An indexed accessor with `@pre index < 3`, used by the SAT loops of `Contacts/BoxBox.hpp`; three named accessors would make every loop heavier. Owner decision 2026-10-01: kept, as the engine keeps this check's bounded indices. |
