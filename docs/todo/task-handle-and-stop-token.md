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
- **Step 3 (next):** the per-vertex `std::unordered_set` adjacency (`adjacentTris`, `neighbors`: ~18 M nodes on that
  mesh) → flat sorted arrays. Their iteration order drives the collapse order, so the output CHANGES (and becomes the
  same on the three OS, which the unordered_set order does not guarantee): validate by a geometric error metric, not
  by hashes. **Step 4:** the edge maps (`edgeCounts`, `edgeVerts`, `processedEdges`, `vertexMap`) → `FlatHashMap`.
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
