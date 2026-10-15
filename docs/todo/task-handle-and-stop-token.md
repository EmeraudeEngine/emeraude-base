---
id: task-handle-and-stop-token
title: ThreadPool tasks owned through a TaskHandle with std::stop_token cancellation
status: in-progress
priority: high
scope: src/ThreadPool.hpp, src/VertexFactory/ShapeDecimator.hpp
opened: 2026-10-08
tags: [ave-robustus-ii, concurrency, lifetime]
---

# ThreadPool tasks owned through a TaskHandle with std::stop_token cancellation

## Why
`ThreadPool::enqueue()` returns no handle: no job has an owner, a cancellation token or an individual wait. Five
ad-hoc cancellation mechanisms grew instead, and `Core` drains the pool on behalf of objects it does not own
(class C of the census, 51 sites). This is the root defect of the plan.

## Done (2026-10-08)
- `TaskHandle` + `ThreadPool::submit()` (`src/ThreadPool.hpp`): the task receives a `std::stop_token`; the handle's
  destructor / move-assignment requests the stop and waits for that task only; when `wait()` returns, the callable and
  its captures are destroyed. 8 tests (`ThreadPoolTaskHandle.*`), 20 rounds under TSan, 0 report.
- **D2 decided and done** (`26128bc`): a task waiting for (or destroying) its own handle is a contract fault —
  `Logging::fatal()` then `std::abort()` in every build (`src/ThreadPool.cpp:74-81`, `ThreadPool.hpp:63-65`, `:155`);
  a `Base::Thread` joining itself aborts too. Death tests `ThreadPoolTaskHandleDeathTest.TaskWaitingForItsOwnHandleAborts`,
  `BaseThreadDeathTest.aThreadJoiningItselfAborts`; `docs/caution-points.md` § A self-join ABORTS.
- `ShapeDecimator::setStopToken()`: every stage loop checks the stop every 4096 iterations (initialisation, corner UV
  table, quadrics, penalties, collapse queue, collapses, output, orientation check, normal-map bake);
  `ShapeProcessor::deduplicateVertices()` takes an optional `std::stop_token` for its read phase (the shape stays
  untouched when stopped). Test `aRequestedStopTokenStopsTheDecimation`.
- Measured stop latency (`DISABLED_StopLatencyOnALargeMesh`, 2.24 M-triangle sphere, Linux i9-14900K Release): worst
  **1.52 s → 0.74 s**. Stage timing of a full decimation: copy + dedup 1 288 ms, init 405, corner UV 33, quadrics 22,
  penalties 672, queue 442, collapses 2 148, output 643.

## Done — D7, the decimator rewrite (owner decision 2026-10-08: flat work data, a stop bound of 50 ms or less)

- **Steps 1-2 DONE (2026-10-09, Linux i9-14900K, Release):** base `FlatHashMap` (open addressing, one allocation) and a
  lean WORK MESH (`ShapeDecimator::buildWorkMesh()`: a representative source vertex per quantized position + the
  remapped corners, no copy of the source). Output BIT-IDENTICAL (8 golden shapes + the 2.24 M-triangle sphere, same
  hashes); full decimation 5.83 s → 4.65 s (median of 5); stop latency: stops in the first 100 ms 0.61-0.73 s → ≤ 13 ms,
  worst 0.73 s → 0.53 s (now from the 200 ms row on).
- **Step 3 DONE (2026-10-09):** the per-vertex adjacency (`adjacentTris`, `neighbors`: ~18 M `std::unordered_set`
  nodes on that mesh) → `std::pmr::vector`s in INSERTION order, in one `monotonic_buffer_resource` arena per
  decimation. The output changed (the collapse order follows these lists) and is now the same on the three OS — the
  peers' goldens of `df420dc` showed three different decimations (libstdc++, libc++, MSVC; even triangle counts:
  hollowedCube 82 / 80 / 82). Quality kept or better (source → output distance, mean / max: sphere300 0.0048 / 0.057
  → 0.0039 / 0.032, geodesic 0.0061 / 0.077 → 0.0045 / 0.070, capsule max 0.120 → 0.090, others equal). Full
  decimation 4.65 s → 3.34 s; worst stop latency 0.53 s → 0.20 s (the 700 ms row: the penalty stage's edge maps).
- **Step 4 DONE (2026-10-09): D7 REACHED on Linux — worst stop latency 25 ms** (0.73 s before D7). The penalty
  stage's two edge maps (one `FlatHashMap< uint64_t, EdgeTally >`, walked by `forEachUntil()` with the stop check), the
  collapse queue's processed-edge set and the output vertex map (a `std::map`) are flat tables; the collapse queue has
  a TOTAL order (cost, then a portable hash of (v0, v1, generations) computed once per candidate): ordered by the cost
  alone, std::priority_queue left the ties to each standard library's heap — the peers' goldens of `1adf560` still
  differed (Windows 4 / 8 identical, macOS 0 / 8; macOS also contracts multiply-adds into FMA on arm64: with
  `-ffp-contract=off` its hollowedCube matched).
- ⚠️⚠️ **Found on the way: the QEM was DISABLED on fine meshes** — fixed 2026-10-09: `Vector::normalize()` at any scale
  (base `83878a5`), then the decimator's numerics (double quadrics, a relative singularity test, a point-quadric seam
  anchor — caution-points § VertexFactory). Linux, 2.24 M-triangle sphere: full decimation 5.8 s (13.8 s with float
  quadrics; 5.83 s before D7 — with a QEM that did nothing).
- **The release floor is gone (2026-10-09):** a stop from 200 ms on cost ~20 ms on Linux and 57-71 ms on the Windows
  laptop (fb5a162) — NOT the stop-check interval (256 changed nothing) but the release at return, measured: the 1.1 M
  `VertexData` with their `pmr::vector`s 7.6 ms, the corner UV table's `std::vector` per vertex ~11 ms. Both are now
  `ArenaList`s (in the decimation's arena, TRIVIALLY destructible: their release is the arena's): the floor is ~1.5 ms;
  the worst row is now the start (~13-16 ms: allocating and zeroing the work mesh's 50 MB flat table). Worst 16 ms on
  Linux. macOS peer (M2, AppleClang Release, idle, base `fd9dc5c`, 3 runs): worst **2.5 ms** (2493-2524 us, the start
  row again), every later row 19-190 us (was ~14.5 ms), a full decimation still 4-6 s; goldens identical to `fb5a162`
  in the default and the `-ffp-contract=off` builds; 2499 tests green in Release and ASan/UBSan. To re-measure on
  Windows.
- History: before D7 the worst stop was 0.73 s on Linux, 0.47 s on the M2 and 1.65 s on the old Windows laptop. Two
  things made it long: the copy of the source (`auto workShape = m_source;`, gone since steps 1-2) and the
  deallocation at return (gone with the arena, from the floor step on).

## Done — `Base::Thread` on the `std::jthread` model (2026-10-15, macOS M6)

- Owner decisions 2026-10-15: ONE `start()` with `std::jthread`'s dispatch rule (a body invocable with a
  `std::stop_token` receives the token of this start, any other is called with no argument — the 8 engine owners
  compile unchanged); accessors `requestStop()` (bool, like `request_stop()`) and `stopToken()`, no stop source handed
  out. A fresh stop state per `start()`; `~Thread()` and the move assignment request the stop, then join; `detach()`
  removed (no production caller in base, the engine or projet-alpha; its only test became stop-token tests). The D2
  self-join abort is kept. Base `docs/caution-points.md` § Threads.
- Tests (failed to compile before, pass after): `BaseThread.aStopRequestedBeforeTheBodyChecksIsSeenAtItsFirstCheck`,
  `aStopWakesAConditionVariableWaitWithoutANotify`, `theDestructorRequestsTheStopThenJoins`,
  `aMoveAssignmentOverARunningThreadStopsAndJoinsIt`, `aRestartedThreadGetsAFreshStopState`,
  `aBodyTakingNoTokenStillRunsAndTheDestructorStillJoinsIt`.
- Proof (macOS M6, AppleClang, paranoid warnings: 0 warning; `-Wdocumentation -Werror` clean): 2513/2513 in Release,
  ASan/UBSan and TSan, 0 report; `BaseThread*` 20 rounds under TSan, 0 report; clang-tidy 21.1.6: 1 fixed, 1 on purpose
  (ledger). Linux / Windows validation by the peers pending.

## What remains

- **`setCancellationFlag()` goes away** with the engine migration (`ShapeDecimator.hpp:122`; still called by engine
  `MeshResource.cpp:484` and `MultiLayerMeshResource.cpp:770`): engine item `thread-pool-jobs-owned-through-a-task-handle`, which
  needs nothing more from this item (`TaskHandle`, `submit()` and `setStopToken()` are done) and is open now (owner,
  2026-10-13). The engine's `Base::Thread` owners (engine item `jobs-owned-by-their-starter`) can migrate once the
  `Base::Thread` change above is pushed.
- **Re-measure the stop latency on the NEW Windows machine** (`DISABLED_StopLatencyOnALargeMesh`). The last Windows
  figure, 57-71 ms at fb5a162, came from the dead laptop.
- **Cross-OS bit identity**: owner decision on `-ffp-contract=off` cascade-wide. With it, macOS matches Linux on
  every triangle / vertex count (4 / 8 exact fingerprints). The generators' own outputs change with FMA contraction
  even on one OS (positions of the torus, geodesic, capsule, cylinder).
