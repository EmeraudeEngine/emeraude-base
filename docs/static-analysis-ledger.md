# Static-analysis ledger — emeraude-base (clang-tidy, cppcheck, include-what-you-use)

> Owner decision 2026-09-30, plan Ave Robustus § 3 (projet-alpha `docs/plans/ave-robustus.md`). The configuration is
> this repository's `.clang-tidy`. Gate on every change: ZERO NEW finding on the touched translation units; a finding
> is fixed, never silenced. A FULL run is a long process ordered by the owner; its results are recorded here.
> Renamed from `clang-tidy-ledger.md` on 2026-10-19, when cppcheck joined the gate (§ cppcheck, at the end).

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

⚠️ **Never `--fix` a header from several TUs at once.** Each TU applies the header's fix-it again: a
`readability-math-missing-parentheses` fix stacked up to ~90 levels of parentheses around `a * e - b * b` and 21 other
lines (found by the engine triad 11, rewritten 2026-10-07, base item runaway-nested-parentheses). Fix a header from ONE
TU, or review the diff of every header after a mass fix; `/usr/bin/grep -rn "((((((" src` must stay empty.
| `src/VertexFactory/ShapeDecimator.hpp` (`setCancellationFlag()`, `isCancelled()`), test `test_VertexFactoryShapeSimplifier.cpp` | 2026-10-08 | clang-tidy 21.1.6: 2 on the changed lines, fixed (misc-const-correctness on the test flags). 0 left. | engine lod-job shutdown |

## Last full run per module

| Module | Date | Findings by check | Notes |
|---|---|---|---|
| cppcheck lot (P-W (2), 2026-10-19): `Hash/SHA1.cpp` (`std::rotl` for the macro), `PixelFactory/FileFormatJpeg.cpp`, `VertexFactory/FileFormatSTL.hpp`, `WaveFactory/Synthesizer.hpp`, the cppcheck sites of § cppcheck and 17 test files | 2026-10-19 | clang-tidy 21.1.6 on the changed lines: 8 → 1. 7 fixed (avoid-c-arrays: the libjpeg message buffer is a `std::array`; misc-const-correctness ×2 in tests; modernize-use-ranges; readability-math-missing-parentheses ×2; constant-array-index in the SHA-1 rounds, now a range-for). 1 ON PURPOSE (below): the SHA-1 message schedule. | cppcheck sweep |
| Never-instantiated template members (P-W (2), 2026-10-19): `Math/Vector.hpp`, `Quaternion.hpp`, `Matrix.hpp`, `CartesianFrame.hpp`, `OrientedCuboid.hpp`, `DeterminantAverage.hpp`, `Space2D/Triangle.hpp`, `PixelFactory/Color.hpp`, `Processor.hpp`, `Margin.hpp`, `VertexFactory/ShapeProcessor.hpp`, `ShapeAssembler.hpp`, `ShapeBuilder.hpp`, `Grid.hpp`, `XRayAnalyzer.hpp`, `Algorithms/Mandelbrot.hpp`, `DiamondSquare.hpp`, `Time/Elapsed/*`; tests `test_TemplateInstantiation.cpp` (new, the guard), `test_AlgorithmsMandelbrot.cpp` (new) and 10 test files | 2026-10-19 | clang-tidy 21.1.6 on the changed lines of the 12 touched TUs (the guard TU includes every header): 4, all fixed (readability-math-missing-parentheses ×2 in `Color::hue()`, cppcoreguidelines-pro-bounds-constant-array-index in a test, modernize-use-ranges in a test). 0 left. ⚠️ The root cause was a clang-tidy FIX: e96495d6f's const-correctness pass made two maps `const` inside a member nothing instantiated — a fix-it on a template body is unverified until the guard TU compiles it. | cppcheck sweep, caution-points § Templates |
| `src/ConditionVariable.hpp` (new), `src/SharedMutex.hpp` (new), `src/Mutex.hpp` (the friendship), `src/ThreadSafety.hpp` (the shared-capability macros), test `test_Mutex.cpp` (Ave Robustus II P-W, D12) | 2026-10-16 | clang-tidy 21.1.6 on the changed lines: 4 → 3. 1 fixed (readability-math-missing-parentheses in the test); 3 ON PURPOSE: cppcoreguidelines-macro-usage on `EMEN_REQUIRES_SHARED`, `EMEN_ACQUIRE_SHARED`, `EMEN_RELEASE_SHARED` — a Clang attribute can only be spelled where the declaration is, never produced by a function, and the macro must expand to nothing on GCC / MSVC (the existing `EMEN_*` macros of the header have the same reason). `-Wthread-safety` proven to catch the misuses: a write under a `SharedMutexReadLock`, a read without a lock, an unannotated `ConditionVariable` predicate (3 warnings on a negative TU). | Ave Robustus II P-W |
| `src/Time/EventTrait.hpp` (new: `TimerHandle`; `createTimer()` / `fire()` return it, Ave Robustus II P4), test `test_Time.cpp` | 2026-10-16 | clang-tidy 21.1.6: the header 0 on the changed lines. The test: 2 ON PURPOSE (bugprone-use-after-move, reported again by clang-analyzer-cplusplus.Move, in `aMovedHandleKeepsTheTimer`): the test asserts the moved-from handle is empty — the documented contract, as for `ScopedRegistration` and `Thread`. | base P4 timers |
| `src/ScopedRegistration.hpp` (new: the registration token, Ave Robustus II P4), test `test_ScopedRegistration.cpp` (new) | 2026-10-16 | clang-tidy 21.1.6: the header 0. The test: 1 fixed (misc-const-correctness); 2 ON PURPOSE (bugprone-use-after-move, each reported again by clang-analyzer-cplusplus.Move, in `aMoveTransfersTheRegistration` and `aMoveAssignmentUndoesTheReplacedRegistration`): the tests assert the moved-from token is empty — the documented contract, as for `Thread`. | base P4 (plan Ave Robustus II) |
| `src/ThreadPool.*` (`TaskGroup`, `TaskGroupRunner`, `TaskGroupRunScope`, `submit(TaskGroup &, …)`, `waitForAll()`; `TaskHandle`: a task stopped before it starts never runs), `VertexFactory/ShapeDecimator.hpp` (`setCancellationFlag()` removed), tests `test_ThreadPool.cpp`, `test_VertexFactoryShapeSimplifier.cpp` | 2026-10-15 | clang-tidy 21.1.6, changed lines: 2, both fixed (cppcoreguidelines-avoid-non-const-global-variables: the two namespace-scope `thread_local`s became function-local ones behind `innermostGroupScope()` / `isPoolWorker()`). 0 left. | base task-handle-and-stop-token |
| `src/Thread.hpp` / `.cpp` (`std::jthread` model: a `std::stop_token` to the body, `requestStop()` / `stopToken()`, stop + join in the destructor and the move assignment, `detach()` removed), test `test_Thread.cpp` | 2026-10-15 | clang-tidy 21.1.6 (macOS, AppleClang compile database): `Thread.cpp` 0. `test_Thread.cpp`: 1 fixed (performance-unnecessary-value-param: a test body's `std::stop_token` by const reference); 1 ON PURPOSE, the same contract as the 2026-10-07 pair (bugprone-use-after-move with clang-analyzer-cplusplus.Move, `aMoveAssignmentOverARunningThreadStopsAndJoinsIt`: the test asserts the moved-from object is idle and has no stop state). | base task-handle-and-stop-token |
| `src/ThreadPool.*` (new: `TaskHandle`, `TaskRunner`, `submit()`), `VertexFactory/ShapeDecimator.hpp` (`setStopToken()`, a checkpoint in every stage), `ShapeProcessor.hpp` (`deduplicateVertices()` stop token), `PixelFactory/Processor.hpp` (element / byte units), `FileFormatMDx.hpp` (skin size, short MD5 blocks), tests `test_ThreadPool.cpp`, `test_VertexFactoryShapeSimplifier.cpp`, `test_PixelFactoryProcessor.cpp`, `test_VertexFactoryFileFormats.cpp` | 2026-10-08 | clang-tidy 21.1.6: 11 on the changed lines; 9 fixed (performance-unnecessary-value-param ×7: the tests' `std::stop_token` by const reference; readability-redundant-member-init on `TaskHandle::State::runningThread`; bugprone-implicit-widening-of-multiplication-result in a test loop bound). 2 ON PURPOSE (below). | base task-handle-and-stop-token, processor-swap-buffer-wrong-units, processor-move-int32-min, mdx-zero-skin-size |
| ALL `src/` TUs — **move / copy audit** (`modernize-pass-by-value`, `performance-unnecessary-value-param`, `performance-move-const-arg`, `performance-unnecessary-copy-initialization`, nothing else enabled) | 2026-10-07 | clang-tidy 21.1.6: **0** findings — emeraude-base 132 TUs, emeraude-engine 460, projet-alpha 109 (canary file checked: the four checks fire). The triad pass (Allocatus Reduxus, 2026-09-30 / 10-01) had already removed them. | base std-move-function-args (closed) |
| ALL `src/` TUs — **Ave Robustus II warning pass** (full `.clang-tidy` check set) | 2026-10-08 | clang-tidy 21.1.6, 133 TUs: `HEAD` 1 779 findings → after the pass 1 770; per (file, check) **0 increase** (the 8 the pass first introduced were fixed: concise `#ifdef`, explicit `void *`, a `while` instead of a `continue`-only `for`, parentheses, a range-for). The 1 770 legacy findings are open (decision D11; top: `pro-bounds-constant-array-index` 411, `misc-const-correctness` 347, `readability-math-missing-parentheses` 273, `bugprone-unchecked-optional-access` 106). | projet-alpha `docs/plans/ave-robustus-ii.md` § 6.3 |
| — | — | no full run recorded yet under this ledger | |
| `src/Any.hpp` (new: `Base::Any`, `typeHashOf()`, `anyCast()`), `AnyValue.hpp`, `ObservableTrait.*`, `ObserverTrait.hpp`, tests `test_Any.cpp` (new), `test_ObserverPattern.cpp` | 2026-10-07 | clang-tidy 21.1.6: 7 on the new code, 4 fixed by design (bugprone-dynamic-static-initializers ×3: the `TypeHash` / `IsInline` variable templates became `consteval` functions and the per-type operation table a single manager function — no static left; performance-unnecessary-copy-initialization in a test), a `const_cast` avoided by the manager signature; 5 ON PURPOSE (below). | base rtti-free-observer-payload |
| `src/Thread.cpp` / `.hpp` (start handshake `m_published`, test seam `delayNextPublicationForTesting()`), test `test_Thread.cpp` | 2026-10-07 | clang-tidy 21.1.6: 0 on the changed lines (the 4 findings left are the on-purpose use-after-move pair, below). | base Thread start race (macOS peer) |
| `src/Algorithms/PerlinNoise.hpp`, `Math/Matrix.hpp`, `Math/Space3D/SAT.hpp`, `PixelFactory/Color.hpp`, `PixelFactory/Processor.hpp`, `VertexFactory/Shape.hpp`, `VertexFactory/TextureCoordinates.hpp` (21 runaway-parenthesis lines rewritten), through tests `test_AlgorithmsPerlinNoise.cpp`, `test_MathMatrix.cpp`, `test_MathSpace3D.cpp`, `test_PixelFactoryColor.cpp`, `test_PixelFactoryProcessor.cpp`, `test_VertexFactoryShapeGenerator.cpp` | 2026-10-07 | clang-tidy 21.1.6: 0 from the rewrite; 7 older findings on those lines: 5 fixed (misc-const-correctness ×4 on `SAT.hpp` `closest`, readability-redundant-casting in `Color::convertFloatToInteger()`), 2 ON PURPOSE (`Matrix::operator()`, below). | base runaway-nested-parentheses |
| `src/Network/HappyEyeballs.cpp` / `.hpp` (new: `interleaveAddressFamilies()`, `connectFirstReachable()`), `TLSConnection.cpp` (`establishTcp()` through the race), tests `test_NetworkHappyEyeballs.cpp` (new), `test_NetworkHTTPSClient.cpp`, `TLSTestHelpers.hpp` (`listenOnIPv6`) | 2026-10-07 | clang-tidy 21.1.6: 4 on the changed lines, fixed (2 misc-const-correctness and 2 bugprone-unchecked-optional-access in the new HTTPS test). 0 left. | base httpsclient-happy-eyeballs |
| `src/Testing/TLSTestHelpers.hpp` (`HTTPSTestServer` also listens on `::1`, `scheduleAccept(acceptor)`), through `test_NetworkHTTPSClient.cpp` | 2026-10-07 | clang-tidy 21.1.6: 0 on the changed lines (the 3 older findings of the file are unchanged). | base httpsclient-happy-eyeballs |
| `src/VertexFactory/ShapeDecimator.hpp` (`CornerUVTable`, `checkUVFoldOver()`, per-corner output UVs), test `test_VertexFactoryShapeSimplifier.cpp` | 2026-10-07 | clang-tidy 21.1.6: 10 on the changed lines, fixed (5 pro-bounds-constant-array-index and 1 array-to-pointer-decay: corner loops over the triangle's array and iterators, a flat corner vector; 4 avoid-const-or-ref-data-members: the table holds non-owning pointers). 0 left. | base decimator-uv-fold-overs |
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
| `src/Hash/SHA1.cpp` `transform()`, the message schedule | `cppcoreguidelines-pro-bounds-constant-array-index` | `W[t] = ROTL1(W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16])` (FIPS 180-4 § 6.1.2) is an indexed recurrence by definition; the loop bounds prove the indices (16 ≤ t < 80 in an 80-word array). 2026-10-19. |
| `src/Testing/test_VertexFactoryShapeGenerator.cpp` `gemCutsKeepTheirGeometry`, the princess fingerprint line | `modernize-use-designated-initializers` | Pre-existing on the line; only a value changed (2026-10-09, sumAbsNormal +6 after the normalize fix). The 12 fingerprint lines of that table share one positional format — rewriting one alone would break the table's reading. |
| `src/Testing/test_VertexFactoryShapeSimplifier.cpp` `aRequestedStopTokenStopsTheDecimation`, `DISABLED_StopLatencyOnALargeMesh` (`std::stop_source stopSource`) | `misc-const-correctness` (×2) | A libstdc++ artefact: it declares `stop_source::request_stop()` `const`, the standard ([stopsource.general]) and MSVC / libc++ do not — a `const` source would not compile on Windows or macOS. 2026-10-08, confirmed by the owner. |
| `src/Math/Space3D/TriangleMesh.hpp` `visit()` stack, `splitPosition()` bins | `cppcoreguidelines-pro-bounds-constant-array-index` | Bounded by construction: the query stack holds at most the depth (`MaxDepth + 2` slots, one push per inner level); a bin index is `min(bin, Bins - 1)`, a boundary loop runs in `[0, Bins - 1)`. A checked container would add a test per node of a per-query walk. 2026-10-02, as the engine keeps this check's bounded indices. |
| `src/Math/Space3D/TriangleMesh.hpp` its `static constexpr` members | `bugprone-dynamic-static-initializers` | A false positive on a class template: the members are `constexpr` (constant-initialized); the check cannot evaluate a dependent initializer and reports every templated constant of the cascade the same way (`Math/Base.hpp`, the engine's `Constants.hpp`). 2026-10-02. |
| `src/Any.hpp` `manage()` / `construct()` (`new` / `delete` through `void *`) | `cppcoreguidelines-owning-memory` (×3) | Type-erased storage: the held object's type is known only inside the manager, which owns it through `m_object`; RAII is the `Any` itself (destructor, `reset()`). A smart pointer to `void` would need the same type-erased deleter. 2026-10-07. |
| `src/Testing/test_Any.cpp` `smallAndLargeValuesSurviveCopiesAndMoves` | `bugprone-use-after-move`, `clang-analyzer-cplusplus.Move` (×2 each) | The test asserts the moved-from `Any` is empty: the documented contract. 2026-10-07. |
| `src/Testing/test_Thread.cpp` `moveOnlyCallablesAndMovesTransferTheThread`, `aMoveAssignmentOverARunningThreadStopsAndJoinsIt` | `bugprone-use-after-move`, `clang-analyzer-cplusplus.Move` (×3 each) | The tests assert the moved-from `Thread` is idle (not joinable, no stop state): the documented contract. 2026-10-07, 2026-10-15. |
| `src/Math/Matrix.hpp` `operator()(row, col)` (both) | `cppcoreguidelines-pro-bounds-constant-array-index` | The element accessor of every matrix product: a hot path, `assert(row < dim_t && col < dim_t)` in Debug (Ave Robustus: a hot path documents and checks in Debug only). 2026-10-07. |
| `src/IO/IO.hpp` `fileGetRange()` `file.read()` | `cppcoreguidelines-pro-type-reinterpret-cast` | `std::istream::read()` takes a `char *`: the destination is the caller's `std::byte` vector, sized to the range. 2026-10-04. |
| `src/VertexFactory/FileFormatMDx.hpp` `loadMDL()`: the skin / frame group counts, `readSimpleFrame()`, the frame type (6 lines) | `cppcoreguidelines-pro-type-reinterpret-cast` | The same `std::istream::read()` into a binary `int` / `mdl_vertex_t` as every other read of this legacy reader (the 2026-10-04 rule above); each destination is sized before the read. 2026-10-17. |
| `src/Testing/test_PortableRandom.cpp` its generators | `cert-msc51-cpp` | A constant seed is the POINT of a golden test (the same numbers on every platform). 2026-10-02. |
| `src/Testing/test_PortableRandom.cpp` `IntegerBoundsAndUniformity` counts | `cppcoreguidelines-pro-bounds-constant-array-index` | The index is `face - 1`, asserted in [1, 6] on the line above. 2026-10-02. |
| `src/Testing/AsanDefaultOptions.cpp` `__asan_default_options` | `bugprone-reserved-identifier` | The name is ASan's documented hook, looked up by its runtime: it cannot be another. 2026-10-02. |
| `src/Math/Space3D/OrientedBox.hpp` `axis(size_t index)` | `cppcoreguidelines-pro-bounds-constant-array-index` | An indexed accessor with `@pre index < 3`, used by the SAT loops of `Contacts/BoxBox.hpp`; three named accessors would make every loop heavier. Owner decision 2026-10-01: kept, as the engine keeps this check's bounded indices. |

## cppcheck

**Version: cppcheck 2.22.0** on the three OS (owner decision 2026-10-19). Debian's 2.17.1 cannot parse a
`static_cast< T >(x)` default argument (the gem generators of `ShapeGenerator.hpp`) nor `a < static_cast< T >(-1) || a > ...`
(`TriangleMesh.hpp`), and abandons the WHOLE translation unit at that `syntaxError`: every TU including those headers
was only partly analysed. Linux: built from the tag (`cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local
-DUSE_MATCHCOMPILER=ON -DHAVE_RULES=OFF -DBUILD_GUI=OFF`, build, `cmake --install`; `~/.local/bin` first in `PATH`); macOS:
Homebrew, `brew pin cppcheck`; Windows: the official installer (`C:\Program Files\Cppcheck`, not on the `PATH`).

**Configuration**: `cppcheck-suppressions.txt` at the repository root — the checks NOT gated: those clang-tidy or the
compiler already gate (`functionStatic`, `functionConst`, `passedByValue`, `const*`, `shadow*`,
`funcArgNamesDifferentUnnamed`), taste (`useStlAlgorithm`, `variableScope`), `unusedFunction` (a library), the tooling
noise, the third-party code. Owner decision: every error, warning, portability and performance check, and the
bug-prone style ones, are gated. A suppression of ONE site is inline, on the line above, with its reason:
`/* cppcheck-suppress <id> ; <reason> */`.

**Gate**: zero finding on the touched TUs, as clang-tidy. The command (Linux; a Claude build directory's compile
database, never `cmake-build-*`):

```bash
cppcheck --project=<build>/compile_commands.json --file-filter='*/emeraude-base/src/*' \
	--library=googletest --library=posix --platform=unix64 -D__x86_64__ -D__linux__ -D__GNUC__=14 --std=c++20 \
	--check-level=exhaustive --enable=all --inconclusive --inline-suppr \
	--suppressions-list=cppcheck-suppressions.txt --cppcheck-build-dir=<scratch dir> -j $(nproc) \
	--template='{file}:{line}:{column}: {severity}: {message} [{id}]' > cppcheck.out 2> cppcheck.txt
```

⚠️ **A zero from a dropped configuration is not a zero** (Windows peer, 2026-10-19): cppcheck predefines no compiler
macro, and a configuration that reaches an `#error` (`emeraude_platform.hpp`: "Unsupported architecture / compiler")
is dropped SILENTLY — no finding, no message, exit 0. Run `--check-config` on one TU first: it must print no
`#error`. The defines per OS: Linux `-D__x86_64__ -D__linux__ -D__GNUC__=14` (`--library=posix --platform=unix64`);
macOS arm64 `-D__aarch64__ -D__arm64__ -D__APPLE__ -D__clang__ -D__attribute__(x)=` (`--platform=unix64`; ⚠️ without
the last one, `EMEN_API` / `EMEN_CAPABILITY` expand to `__attribute__((…))` between `class` and its name and cppcheck
abandons every such class body: 400 artefacts on the engine, 2026-10-19, macOS peer — 3 information lines with it); Windows `-D_WIN32 -D_WIN64 -D_M_AMD64
-D_MSC_VER=1944 -UEMERAUDE_USE_FULL_EXPORTS -UEMERAUDE_USE_LEAN_EXPORTS` (`--library=windows --platform=win64`; Windows
peer, 2026-10-19: ⚠️ `--platform=win64` does NOT define `_WIN32` — the `*.windows.cpp` that include
`COM.windows.hpp` reached its `#error` and were dropped, 25 files checked of 30; and the MSVC database defines
`EMERAUDE_USE_FULL_EXPORTS`, so `EMEN_API` is `__declspec(dllexport)` between `class` and its name: 338 class bodies
abandoned and 59 false findings — `-D__declspec(x)=` does not help, undefining the export switches does). The gate counts the error,
warning, style, performance and portability lines; an `information: Unmatched suppression` is not a finding (an
inline suppression for an instantiation or a configuration another OS or a library-only run does not reach).
First runs: macOS 0, Windows 0 (library TUs; three unmatched-suppression notes).

⚠️ With `--project`, cppcheck 2.22 does NOT search the compile database's `-isystem` directories (nor a `-I` given on
its own command line): those headers are "not found" — harmless while nothing of them reaches an `#if`, a skipped TU
when it does (projet-alpha's CEF: its ledger § cppcheck rewrites that one `-isystem` to `-I` in a copy of the database).

A touched TU alone: add `--file-filter='*<file name>'` once per file. ⚠️ cppcheck does not honour a `requires` clause:
it evaluates a constrained member for every instantiation of its class (e.g. `rotateTowards()` for an `int` vector).

**First full sweep (2026-10-19, 2.22.0)**: 673 findings with every check; 137 with the configuration; **0** after the
lot. Fixed:
- a member nothing instantiates is never compiled (2.17.1's `knownEmptyContainer`): the LSCM unwrap and 18 others,
  base e683165 — and the template guard `test_TemplateInstantiation.cpp` (caution-points § Templates);
- `uninitMemberVarNoCtor` ×30: every member initialized (ShapeProcessor's chart / loop records, TreeSkinner,
  the MDx records, the libjpeg error manager, 8 test structures);
- `invalidPointerCast` ×5: the binary STL records go through a 50-byte buffer (`std::memcpy`; a `static_assert` makes
  the little-endian assumption explicit), and the writer now checks every write (it reported success on a stream that
  refused them, test `VertexFactorySTL.binaryWriteReadsBackAndRefusesAFailingStream`);
- `sizeofCalculation`: SHA-1's rotation macro → `std::rotl`; `knownArgument` / `knownConditionTrueFalse` for integral
  instantiations: `Matrix::perspectiveProjection()` takes `requires floating point`;
- dead code: `Processor::mirrorY()` returns `void` (it always returned true), the unreachable `sliceIndex == slices` of the
  UV sphere, `Synthesizer::getEffectiveSampleCount()` (unused), the filter's `highpass` scoped to its loop, two unused
  strings in the STL ASCII reader, a redundant cancel test in the decimator, 25 dead `is_integral` branches of
  `test_MathCartesianFrame.cpp` (the suite runs float and double only; the upright billboard test runs both now);
- `returnByReference`: `URIDomain::base()` returns a `const std::string &`; `useInitializationList` (`TimedEvent`);
  `duplicateAssignExpression` (BoxBox, Gradient) written as a copy of the first value; copies kept for a const view
  replaced by `std::as_const()` in the tests.

Kept ON PURPOSE (inline, each with its reason): `accessMoved` ×19 in the tests (the moved-from state is the contract,
as clang-tidy's use-after-move entries above); the generic `AARectangle` code (signed or unsigned `precision_t`, ×7);
`Any`'s implicit converting constructor (as `std::any`'s) and `ArenaList`'s move constructor; `FlatHashMap::operator[]`
(the pointer goes into the map, not into the returned pair); two `isIntersecting(...) > 0` (the three-argument overload
returns a count); `HTTPServer::forEachConnection()`'s copy (the callback may shrink the set); `TimedEvent`'s wake-up
test and the decimator's second cancel test (state another thread changes); the two `PortableRandom` full-range
tests; `rotateTowards()` and `perspectiveProjection()` (requires clauses cppcheck ignores); two exact float casts
(`ShapeGenerator`, `ShapeProcessor`); a reference member of an aggregate (`TreeParametricGrower`); four test patterns
(a Pixmap copying its span, a registration the move assignment must undo, a copy's independence, a lock only the
waiting thread holds).

**Seen from the engine's sweep (2026-10-19)**: the engine analyses this library's headers through its own
instantiations, and found three traps and four sites here:
- ⚠️ **`*/dependencies/*` suppresses EVERYTHING from a consumer.** The engine reaches this repository through its own
  `dependencies/` directory (`…/emeraude-engine/dependencies/emeraude-base/src/…`): the former third-party pattern hid
  every finding of the engine, a false zero. The third-party patterns name each directory (`*/dependencies/asio/*`, the
  ext-deps archive `*/dependencies/*-Release*/*`…), never the parent.
- ⚠️ **A constrained alias template abandons the TU**: `template< typename T > requires (…) using X = …;` is a
  `syntaxError` for cppcheck 2.22 (and 2.23-dev) — every TU including `Math/Space2D/Point.hpp` was skipped, silently but
  for one line. The constraint is a type-constraint (`template< Arithmetic T > using …`, the `Math::Arithmetic` concept of
  `Math/Base.hpp`); `test_TemplateInstantiation.cpp` asserts it still refuses what the requires-clause refused.
- A suppression inline in a header is judged by THIS repository's sweep: the engine instantiates other members and
  arguments, so four of them are `information: Unmatched suppression` in the engine's sweep — not findings (the gate
  counts error / warning / style / performance / portability). A file pattern on `unmatchedSuppression` does not
  silence them (tried).
- Fixed here: `SparseTriplet` (ShapeProcessor) members initialized; `FileFormatOBJ`'s vertex-key hash written as a loop
  (same value — cppcheck misread the unrolled form as dead stores); `cosineInterpolation()` halves by a division by 2
  (exact, no `double → float` cast of 0.5).

## include-what-you-use

**Version: include-what-you-use 0.23** (Debian, clang 19). Owner decisions 2026-10-19: every file a lot touches comes
out IWYU-clean (no mass pass over the untouched ones); forward declarations are allowed (IWYU's default), each one
verified by compiling — a type used inside a template body needs its complete definition, so IWYU's forward
declaration is refused there and the include kept with `// IWYU pragma: keep (<reason>)` (`ThreadPool.hpp` in
`PixelFactory/Processor.hpp` and `VertexFactory/ShapeDecimator.hpp`).

**Configuration**: `iwyu-mapping.imp` at the repository root — `std::abs` / `size_t` homes, FreeType through
`ft2build.h` (its macro includes are `// IWYU pragma: keep`), asio through `asio.hpp` / `asio/ssl.hpp`, `std::vector`
(IWYU 0.23 attributes it to `<format>` with libstdc++ 14). IWYU still names a C header (`<stddef.h>`, `<stdint.h>`):
the cascade writes the C++ one (`<cstddef>`, `<cstdint>`). The guard TU's include list is `// IWYU pragma:
begin_keep` / `end_keep` (it includes every header on purpose).

**The command** (Linux; the touched `.cpp` files as TUs, the touched headers checked through the guard TU, which
includes all of them):

```bash
iwyu_tool -p <build> <touched .cpp files> -- -Wno-unknown-warning-option \
	-Xiwyu --mapping_file=iwyu-mapping.imp -Xiwyu --cxx17ns
iwyu_tool -p <build> src/Testing/test_TemplateInstantiation.cpp -- -Wno-unknown-warning-option \
	-Xiwyu --mapping_file=iwyu-mapping.imp -Xiwyu --cxx17ns -Xiwyu --check_also=<each touched header>
```

Apply a suggestion with projet-alpha `tools/iwyu-apply.py`, inside the cascade's include sections (STL, third-party, local; the forward declarations in a
`/* Forward declarations. */` block), never with IWYU's `fix_includes.py`, which ignores them. A quoted header that
is not in the tree (`"asio.hpp"`, `"ft2build.h"`) is third-party. Run it twice: an include added to a header removes
the template-instantiation blame IWYU put on its tests (the second pass removes what the first added to them).

⚠️ **A library's private configuration header is never included directly** (Windows peer, fd37902): IWYU proposed
`<jmorecfg.h>` before `<jpeglib.h>`; on Windows libjpeg-turbo's `jconfig.h` must come first (it typedefs `boolean` and
`INT32` and guards `jmorecfg.h` against redefining them) — MSVC C2371. The mapping sends `jmorecfg.h` / `jconfig.h`,
`pngconf.h` / `pnglibconf.h`, `tiffconf.h` / `tiff.h` to their public header. A third-party include IWYU adds is
checked on the three OS before it is kept.

⚠️ **A header that is included for its definitions, not its names, carries `// IWYU pragma: always_keep`**:
`Network/asio_throw_exception.hpp` defines asio's `throw_exception()` under `-fno-exceptions`; the engine's first IWYU
lot removed it from two files and the link failed (2026-10-19).

⚠️ A template body's dependent call is blamed on the INSTANTIATING file (a test that instantiates `Vector` was asked
for `BaseUtility.hpp`): the fix is the template's own header including what it uses.

**First lot (2026-10-19)**: the 73 files the three base lots of that day touched (44 headers, 29 `.cpp`): 386
includes added and 167 removed in 65 files at the first pass, then converged. Known IWYU false suggestion, kept: a
namespace-scope forward declaration of `ShapeDecimator::ArenaList` (a nested class template is declared inside its
class only; every use follows its definition).


## D11 — the disabled checks ruled (owner decision 2026-10-19, Ave Robustus II P-W (5))

The `.clang-tidy` (identical in the three repositories) switches ON `cppcoreguidelines-pro-bounds-pointer-arithmetic`,
`cppcoreguidelines-pro-bounds-avoid-unchecked-container-access` (0 finding in the cascade: free), `bugprone-easily-swappable-parameters`
and `readability-function-cognitive-complexity` (default threshold 25), and sets `WarningsAsErrors: '*'` (a finding is
an error when the tool runs: the check names carry `,-warnings-as-errors`). **Gate**: zero NEW finding on the changed
lines, as for every check; the code already there is the BASELINE below, brought clean file by file when a lot touches
it — no mass campaign (owner).

Baseline (2026-10-19, clang-tidy 21.1.6, the cascade's own TUs, findings deduplicated across TUs):

| Check | emeraude-base (55 TUs) | emeraude-engine (471) | projet-alpha (106) |
|---|---|---|---|
| `pro-bounds-pointer-arithmetic` | 170 | 804 | 219 |
| `easily-swappable-parameters` | 81 | 422 | 253 |
| `function-cognitive-complexity` | 37 | 281 | 82 |
| `pro-bounds-avoid-unchecked-container-access` | 0 | 0 | 0 |

Still OFF: `cppcoreguidelines-avoid-magic-numbers` / `readability-magic-numbers` and `readability-identifier-length`
(style, not defects — D11). The others predate the base's extraction from the engine with no written reason (the
history says nothing); the reasons below are INFERRED, not ruled by the owner yet: `misc-include-cleaner`
(include-what-you-use is the gate), `misc-multiple-inheritance` (the trait design), `modernize-use-trailing-return-type`,
`readability-trailing-comma`, `portability-avoid-pragma-once` (the cascade's written style),
`readability-suspicious-call-argument` (names compared, not defects), `clang-diagnostic-unreachable-code` (presumably
the `if constexpr` platform branches).
