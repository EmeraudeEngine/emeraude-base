---
id: task-handle-and-stop-token
title: ThreadPool tasks owned through a TaskHandle with std::stop_token cancellation
status: open
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

## What remains
- `ThreadPool::submit(callable)` → move-only `TaskHandle` owning a `std::stop_source`; the callable receives a
  `std::stop_token`; `~TaskHandle()` = `request_stop()` + wait for THAT task only (owner decision D1).
- `Base::Thread` with the `std::jthread` model: the body takes a `std::stop_token`; the destructor requests stop and
  joins; waits through `std::condition_variable_any::wait(lock, token, pred)`.
- `ShapeDecimator::decimate(std::stop_token)` replacing `setCancellationFlag()`, a checkpoint in EVERY stage
  (deduplication, `initializeData`, corner UV table, quadrics, `buildCollapseQueue`, collapse loop, output build);
  measure the worst-case stop latency on a 2.3 M-triangle input against the bound D7.
- Tests: stop before start, stop during, destructor waits only its task, move semantics; ASan/UBSan **and TSan**.

## ⚠️ Traps
- `std::jthread`'s constructor throws: keep `Base::Thread`, adopt the semantics.
- The engine migration (engine item `jobs-owned-by-their-starter`) removes the ad-hoc flag afterwards.

## References
- projet-alpha `docs/plans/ave-robustus-ii.md` § 3.2, § 4.1, § 4.2, P1.
