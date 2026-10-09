---
id: task-handle-and-stop-token
title: ThreadPool tasks owned through a TaskHandle with std::stop_token cancellation
status: in-progress
priority: high
scope: src/ThreadPool.hpp, src/Thread.hpp, src/VertexFactory/ShapeDecimator.hpp
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
  its captures are destroyed; a task waiting for its own handle is refused and traced (D2 open: abort?). 8 tests
  (`ThreadPoolTaskHandle.*`), 20 rounds under TSan, 0 report.
- `ShapeDecimator::setStopToken()`: every stage loop checks the stop every 4096 iterations (initialisation, corner UV
  table, quadrics, penalties, collapse queue, collapses, output, orientation check, normal-map bake);
  `ShapeProcessor::deduplicateVertices()` takes an optional `std::stop_token` for its read phase (the shape stays
  untouched when stopped). Test `aRequestedStopTokenStopsTheDecimation`.
- Measured stop latency (`DISABLED_StopLatencyOnALargeMesh`, 2.24 M-triangle sphere, Linux i9-14900K Release): worst
  **1.52 s → 0.74 s**. Stage timing of a full decimation: copy + dedup 1 288 ms, init 405, corner UV 33, quadrics 22,
  penalties 672, queue 442, collapses 2 148, output 643.

## What remains
- **Owner decision (2026-10-08), D7: REWRITE** — the decimator's work data goes flat (lean work copy: positions + triangles only;
  CSR adjacency; a sorted / open-addressing dedup) to reach the ≤ 50 ms stop bound; step by step, each step A/B-measured.
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
  quadrics; 5.83 s before D7 — with a QEM that did nothing), worst stop latency 21 ms. The remaining floor (~15-20 ms
  on Linux / macOS, ~60 ms measured on the former Windows laptop) is NOT the stop-check interval (256 changes nothing):
  it is the release of the large work structures — revisit on the new Windows machine.
- **Cross-OS identity:** with `-ffp-contract=off`, macOS matches Linux on every triangle / vertex count (4 / 8 exact
  fingerprints); the generators' own outputs change with FMA contraction, even on one OS (positions of the torus,
  geodesic, capsule, cylinder). Bit identity across OS needs an owner decision on `-ffp-contract=off` cascade-wide.
- **The remaining latency is DEALLOCATION, not computation**: on a stop, the work stops within milliseconds, but the
  return frees the dedup hash map (1.1 M nodes) and two `std::unordered_set` per vertex (1.1 M vertices). Reaching the
  D7 bound (≤ 50 ms) needs flat data structures (CSR adjacency, a sorted / open-addressing dedup) — also a large
  allocation and speed gain (Allocatus Reduxus). Owner decision: do it, or accept a size-proportional bound.
- **The first uninterruptible block is the COPY of the source** (`auto workShape = m_source;` at the top of
  `decimate()`): a stop requested at 0 ms still costs 0.47 s on the macOS peer (M2, 2026-10-08, base `73a319a`; worst
  0.47 s idle, 0.79 s under a -j8 build). Measured on Linux (i9-14900K, Release, probe outside the suite): copying the
  2.24 M-triangle sphere (1 122 226 vertices × 92 B, 2 240 000 triangles × 64 B, 6 720 000 edges) takes 0.57-0.61 s,
  destroying it 0.07 s. The work shape only needs positions + triangles for the connectivity: building it lean (no
  edges, no attributes) would cut both the latency and ~hundreds of MB of allocation — same owner decision as above
  (flat data), plus a check before the copy for a stop requested before the start.
- Windows peer (laptop, MSVC Release, 2026-10-08, base `73a319a`): worst **1.65 s**; stop at 0 ms 1.50 s, shrinking
  steadily to 0.33 s at 1500 ms (a stop landing inside the copy waits for its end), then 0.68-1.65 s in the later
  stages; a full decimation takes more than 12 s there. Same probe there: copy 1.28-1.30 s, destroy 0.20 s
  (Linux ×2.2 and ×2.9) — copy + destroy = 1.48-1.50 s, the whole 0 ms row: the source copy IS that latency.
- `Base::Thread` with the `std::jthread` model (stop token to the body, request_stop + join in the destructor).
- `setCancellationFlag()` goes with the engine migration (P2, engine item `jobs-owned-by-their-starter`).
