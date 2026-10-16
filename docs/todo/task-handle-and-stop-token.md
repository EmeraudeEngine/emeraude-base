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
  in the default and the `-ffp-contract=off` builds; 2499 tests green in Release and ASan/UBSan.
- **Re-measured on the NEW Windows machine (2026-10-15)**: i7-13850HX (8 P + 12 E cores, 28 threads, on AC power,
  balanced power plan, CPU load 1 % before the runs), MSVC 19.44 Release, base `a673299`, 3 runs. **Worst 46.4 /
  29.2 / 44.7 ms: under the 50 ms bound in these runs, by luck (the stage timing below exceeds it).** Unlike Linux and
  the M2, the start row is NOT the worst and the ~1.5 ms floor is NOT reached: every row from 700 ms on costs ~19-29 ms,
  and the two 45 ms rows are isolated outliers (one at 2000 ms in run 1, one at 400 ms in run 3).

  | Stop at | Run 1 | Run 2 | Run 3 |
  |---|---|---|---|
  | 0 ms | 19.3 ms | 18.5 ms | 20.2 ms |
  | 25 ms | 1.0 ms | 0.8 ms | 7.0 ms |
  | 50 ms | 2.6 ms | 2.8 ms | 2.7 ms |
  | 100 ms | 8.3 ms | 5.9 ms | 3.2 ms |
  | 200 ms | 9.6 ms | 9.1 ms | 8.5 ms |
  | 400 ms | 14.8 ms | 13.2 ms | **44.7 ms** |
  | 700 ms | 22.5 ms | 23.3 ms | 19.0 ms |
  | 1000 ms | 21.3 ms | 23.9 ms | 20.0 ms |
  | 1500 ms | 26.4 ms | **29.2 ms** | 19.6 ms |
  | 2000 ms | **46.4 ms** | 25.2 ms | 19.6 ms |
  | 3000 ms | 26.7 ms | 22.6 ms | 19.5 ms |
  | 4000 ms | 20.1 ms | 22.6 ms | 23.2 ms |
  | 6000 ms | 20.5 ms | 21.6 ms | 24.2 ms |
  | 8000 ms | 27.3 ms | 22.1 ms | 21.0 ms |
  | 12000 ms | finished before | finished before | finished before |

  A full decimation finishes between 8 and 12 s (the last two rows), ~11 s by subtraction (2.5 times Linux's 4-6 s;
  the 2.24 M-triangle sphere is generated outside the measured rows). The test that generates the sphere and decimates
  it once (`DISABLED_ProfileALargeDecimation`) takes 15.4 / 16.9 / 15.9 s. Goldens (`DISABLED_PrintGoldenFingerprints`,
  MSVC default `/fp:precise`, no `/arch` = SSE2: no FMA can be emitted): `sphere48 1056 tris 561 verts
  5dd60e3ae66f624b`, `sphere300 21900 / 11112 3f8bb2deb42f0f87`, `torus 1228 / 713 f7b9a411a41dce34`, `geodesic 2048 /
  1083 2e2367ba9aa52a4f`, `hollowedCube 80 / 100 6baf854c88a6e1fe`, `capsule 570 / 327 295c52dae496ea24`, `cylinder
  440 / 379 7c1fa163617fb646`, `sphere48-normalmap 594 / 2112 acc1eb9d8acf852f nm c931022e27154289`; **4 / 8 identical
  to Linux** (GCC 14.2, x86-64 baseline, same base): sphere48, hollowedCube, capsule, cylinder. sphere300 differs by
  one vertex (11112 vs 11113); torus, geodesic and sphere48-normalmap have the same counts but other hashes. This is the
  cross-OS bit identity, an open owner decision (`-ffp-contract` / libm, below); MSVC emits no FMA, so on Windows the
  difference comes from the libm or from how MSVC evaluates the same expressions, not from contraction. The 6 enabled
  `VertexFactoryShapeDecimator.*` tests pass.
- **Windows stage timing of the stop (2026-10-15)**, with an instrumented COPY of `ShapeDecimator.hpp` (a timestamp at
  the first `isCancelled()` that answers true, one at each stage entry, and probe objects between the locals of
  `decimate()` timing each release), same compiler flags. A full uninterrupted decimation takes 10.3 s: work mesh
  122 ms, init 175, corner UV 77, quadrics 83, penalties 408, queue 767, collapses 7 482, output 1 181. Findings:
  - **The ~20 ms floor is the release at return, not an E-core.** Unpinned, the worker ran on a P-core in every row.
    Pinned to the E-cores, the floor is ~30 ms, and pinned to the P-cores it stays ~20 ms. Stopped during the collapses,
    the stop is SEEN within 0.01-2 ms. The rest is unwinding: the collapse queue's vector ~4.3-6.4 ms, the corner UV
    table ~0.7, the triangles + vertices ~6.3-9.4, the arena ~5.4-11, the work mesh ~0.8. A few blocks of hundreds of
    MB cost milliseconds each to free under MSVC; on Linux the same release is ~1.5 ms in total.
  - **⚠️ D7 is EXCEEDED on Windows when the stop lands at the start of the penalty stage: worst 68.9 ms.** Each stage
    builds its `FlatHashMap{(triangles × 3) / 2}`, and that constructor runs with no stop check. It allocates and
    value-initializes the whole table: `bit_ceil(2 × 3.36 M)` = 8.4 M slots, ~200 MB for the penalties' `EdgeTally`
    table and ~134 MB for the queue's `processedEdges`. A stop that lands there is seen only once the table is ready:
    ~48 ms after the penalty stage starts, ~26 ms after the queue stage starts, ~9 ms in the corner UV stage, and
    ~18 ms in the work mesh (the start row). Every row of a 250-900 ms sweep (10 ms steps) that hit the first ~50 ms
    of the penalty stage took 30-52 ms; the stage's own return then frees its table (~5 ms) before the ~20 ms of
    unwinding. Measured totals: 68.9 and 58.5 ms (the 400 ms row of two default runs), 52.1 and 50.7 ms (the 500 and
    460 ms rows of the sweep), 47.6 ms (another 400 ms row). With the delays the unit test uses, the 400 ms row lands
    there on this machine, but only sometimes: the unit test's own runs gave 14.8 / 13.2 / 44.7 ms for that row. Defect
    opened: `decimator-stop-latency-flat-table-init`.
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
  (ledger). Linux (Linux-PA, the change rebuilt on a673299): GCC Release 0 warning, full suite green, TSan
  `BaseThread*` + `ThreadPool*` 20 × 83, 0 report. Windows validation pending.

## Done — `TaskGroup`, the stop-before-start contract, `setCancellationFlag()` removed (2026-10-15, Linux)

- Owner decisions 2026-10-15: `TaskGroup` (N tasks owned by one object, one stop source, `~TaskGroup()` = stop +
  wait; `waitForAll()` for a shutting-down owner on the main thread — aborted from a pool worker); a task stopped
  before it starts NEVER RUNS (`TaskHandle` and `TaskGroup`): a wait after a stop never depends on the queue, so an
  owner destroyed on a pool worker can no longer deadlock a saturated pool. `docs/caution-points.md` § Threads.
- Tests: 10 `ThreadPoolTaskGroup.*` + 2 death tests; `ThreadPoolTaskHandle.TaskStoppedBeforeStartNeverRuns` and
  `DestroyingAQueuedHandleFromTheOnlyWorkerDoesNotDeadlock` (the deadlock tests HUNG past a 20 s timeout on the
  previous contract, pass now). Release 10 × 83 `ThreadPool*`; ASan/UBSan and TSan full suite 2522, 0 report.
- `ShapeDecimator::setCancellationFlag()` is gone (the engine's LOD jobs own a `TaskHandle`, engine item
  `thread-pool-jobs-owned-through-a-task-handle`); test `aRaisedCancellationFlagStopsTheDecimation` removed,
  `aLoweredCancellationFlagChangesNothing` became `anUnrequestedStopTokenChangesNothing`.

## What remains

- The stop latency on the new Windows machine was measured on 2026-10-15: worst 68.9 ms, over the 50 ms bound (the
  large flat tables are built without a stop check) — base item `decimator-stop-latency-flat-table-init`.
- **Cross-OS bit identity**: owner decision on `-ffp-contract=off` cascade-wide. With it, macOS matches Linux on
  every triangle / vertex count (4 / 8 exact fingerprints). The generators' own outputs change with FMA contraction
  even on one OS (positions of the torus, geodesic, capsule, cylinder).
