---
id: process-cpu-time-resolution-contract
title: processCPUTimeNanoseconds() promises a resolution Windows does not have
status: in-progress
priority: high
scope: src/Time/Time.cpp + src/Testing/test_Debug.cpp
opened: 2026-09-15
tags: [time, windows, cross-platform, testing]
---

# processCPUTimeNanoseconds() promises a resolution Windows does not have

## Why

`Time::processCPUTimeNanoseconds()` (`src/Time/Time.cpp:40`) returns nanoseconds on both
platforms, and only one of them can deliver them:

| Platform | Source | Real resolution |
|---|---|---|
| POSIX | `clock_gettime(CLOCK_PROCESS_CPUTIME_ID)` | genuinely nanosecond |
| Windows | `GetProcessTimes()`, kernel + user, converted from 100-ns units | **15.625 ms**, measured |

The unit is honest; the resolution is not. The Windows counter advances **only in exact multiples
of the scheduler quantum** — sampled across 400 ms of busy work: 15 distinct readings, every one a
whole multiple of 156250 ticks (15.625 ms, 31.25 ms, 46.875 ms), with **no intermediate value
anywhere in the sample**. So any interval shorter than one tick reads back as exactly `0`.

`DebugStatistics.timerMeasuresBusyWork` (`src/Testing/test_Debug.cpp:53`) is the visible
consequence: its 50-million-iteration loop lands near one tick, so whether it crosses a boundary
decides the verdict. Measured **4 pass / 2 fail in 6 isolated runs on Windows**, against **30 / 30
pass on Linux**.

The assertion is `EXPECT_GT(elapsed, 0U)` because of a **written claim that is false**:
`test_Debug.cpp:38` says the timer *"now delegates to `Time::processCPUTimeNanoseconds()` —
cross-platform and full-nanosecond"*. True on POSIX, false on Windows. Any fix that leaves that
sentence standing will be re-broken by the next person who reads it.

## Owner decision (2026-10-07)

Options 1 + 2: document the Windows floor and EXPOSE the resolution next to the getter (a `processCPUTimeResolutionNanoseconds()`); the test asserts against it; the false comment of `test_Debug.cpp:38` is fixed. CPU time stays CPU time (no QueryPerformanceCounter).

## What remains

Done on Linux (2026-10-07): `Time::processCPUTimeResolutionNanoseconds()` (POSIX `clock_getres`, Windows the
15.625 ms floor), the warning on `processCPUTimeNanoseconds()`, `timerMeasuresBusyWork` rewritten to work for at
least TWO resolutions of CPU time (no fixed loop length), the false comment fixed, a test pinning the resolution in
(0, 15.625 ms]. Linux: 20 × repeated green.

- [ ] **Windows proof**: `EmeraudeBaseUnitTests --gtest_filter=DebugStatistics.* --gtest_repeat=30` green (it was
      4 pass / 2 fail in 6 before). Then delete this item.

## ⚠️ Traps

- **Do not lengthen the busy loop.** It would only make the failure rarer, which is the one shape of
  fix the Socratus campaign exists to reject — see
  `app_system/docs/plans/socratus.md` § 9.6, decision 6.
- **This test is swept up by app_kernel's CI.** `ctest` over the standalone app_kernel build
  registers `EmeraudeBaseUnitTests` as well as `KernelTests`, so this intermittence makes
  **app_kernel's CI job go red at random on Windows**, for a reason outside the perimeter that job
  claims to measure. That is why it matters beyond emeraude-base.
- The two branches returning the same unit is what hides the problem: a caller reading the
  signature has no way to learn that one of them is quantised.

## References

- `src/Time/Time.cpp:40` — `processCPUTimeNanoseconds()`, both branches
- `src/Testing/test_Debug.cpp:38` (the false claim) and `:53` (the assertion)
- Measurement, method and raw numbers:
  `app_system/docs/plans/socratus/evidence/windows/control-2026-09-15/README.md`,
  § *DebugStatistics.timerMeasuresBusyWork*
- Linux counter-measurement (30 / 30 pass) and the cross-OS reading:
  `app_system/docs/plans/socratus.md` § 9.6, decision 6
