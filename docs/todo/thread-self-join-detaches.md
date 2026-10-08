---
id: thread-self-join-detaches
title: Base::Thread turns a self-join into a silent detach
status: open
priority: high
scope: src/Thread.hpp, src/Thread.cpp
opened: 2026-10-08
tags: [ave-robustus-ii, concurrency, defect]
---

# Base::Thread turns a self-join into a silent detach

## Why
Destroying (or joining) a `Base::Thread` from its own thread falls back to `detach()` (base `4aa29ae`): the object dies
while its thread still runs — a silent use-after-free instead of a contract fault (Core Guidelines CP.26, Ave Robustus
II rule 5).

## What remains
- Owner decision D2 of the plan: refuse loudly (Debug `assert`, Release trace + `std::abort`).
- Find every path that can self-destroy a thread today (a thread body releasing the last reference of its owner) and
  fix the owner, BEFORE switching the fallback to an abort.
- Test: death test for the self-join.

## References
- projet-alpha `docs/plans/ave-robustus-ii.md` § 3.4, § 4.2, D2.
