---
id: cpu-time-unknown-clock-rate
title: CPUTime records nothing when CLOCKS_PER_SEC is not 1e3, 1e6 or 1e9
status: open
priority: high
scope: src/Time/Statistics/CPUTime.cpp
opened: 2026-10-08
tags: [ave-robustus-ii, time, defect]
---

# CPUTime records nothing when CLOCKS_PER_SEC is not 1e3, 1e6 or 1e9

## Why
`CPUTime::stop()` converts with `if constexpr` branches for `CLOCKS_PER_SEC` 1000, 1e6 and 1e9 only: any other rate
records no duration at all, silently. (Negative / failed `std::clock()` samples are dropped since 2026-10-08 — an owner
check: drop, clamp or count?)

## What remains
- A generic conversion (`duration * 1'000'000 / CLOCKS_PER_SEC` in 64 bits) for any rate; test.
- Owner: confirm "drop" for a failed / negative sample (also applied to `Statistics::RealTime`).

## References
- Found by the Ave Robustus II warning pass (2026-10-08, projet-alpha `docs/plans/ave-robustus-ii.md`); not raised by a warning, so left for its own fix.
