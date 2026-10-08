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
- **The remaining latency is DEALLOCATION, not computation**: on a stop, the work stops within milliseconds, but the
  return frees the dedup hash map (1.1 M nodes) and two `std::unordered_set` per vertex (1.1 M vertices). Reaching the
  D7 bound (≤ 50 ms) needs flat data structures (CSR adjacency, a sorted / open-addressing dedup) — also a large
  allocation and speed gain (Allocatus Reduxus). Owner decision: do it, or accept a size-proportional bound.
- `Base::Thread` with the `std::jthread` model (stop token to the body, request_stop + join in the destructor).
- `setCancellationFlag()` goes with the engine migration (P2, engine item `jobs-owned-by-their-starter`).
