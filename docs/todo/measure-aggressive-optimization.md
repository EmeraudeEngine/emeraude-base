---
id: measure-aggressive-optimization
title: Measure EMERAUDE_ENABLE_AGGRESSIVE_OPTIMIZATION before moving its default
status: open
priority: unranked
scope: compile policy (CMakeLists.txt, EMERAUDE_COMPILE_OPTIONS)
opened: 2026-09-08
tags: [performance, build, cpp]
---

# Measure EMERAUDE_ENABLE_AGGRESSIVE_OPTIMIZATION before moving its default

## Why

The option exists but **nothing measured on this codebase justifies either position**. It was
introduced on 2026-09-08 when the Release policy was aligned on the level CEF publishes per
platform (owner decision):

| Platform | Option Off (default) | Option On |
|---|---|---|
| Windows / MSVC | `/Ob2` | `/Ob3` (MSVC has no `/O3`, the inline level is the whole lever) |
| Linux | `-O2` | `-O3` |
| macOS | `-O2` — **but projet-alpha turns the option On here**, so `-O3` | `-O3` |

The owner's words when the default was chosen: *"un test nous dira plus tard s'il vaut mieux en
changer."* This item is that test. The project rule is RUNTIME > READABILITY > COMPILE TIME, so a
proven runtime gain is enough to flip the default — but "aggressive sounds faster" is not.

**Owner intent (2026-09-08):** macOS already runs with the option **On** — that is settled (it is
CEF's own macOS level) and is *not* what this item measures. Windows and Linux stay at CEF's level
until a dedicated test session says otherwise; those sessions are deliberately deferred, not
forgotten. So the work here is: measure Windows (`/Ob2` vs `/Ob3`) and Linux (`-O2` vs `-O3`), then
decide whether their default moves.

## What remains

- [ ] Build the same revision twice per platform (option Off / On) and collect three numbers:
      **binary size**, **build wall time**, and a **frame time** on a fixed bench.
- [ ] Frame time must come from a reproducible pose — projet-alpha's fixed camera
      (`AbstractDemo::enableFixedCamera(position, lookAt)`) exists exactly for that. A hand-held
      camera pose is not a measurement.
- [ ] Run `EmeraudeBaseUnitTests` (Release) in both states: `-O3` and `/Ob3` change what the
      optimiser proves, so a green suite is part of the evidence, not a formality.
- [ ] Decide per platform, and write the numbers into `docs/integration.md` (the option's row)
      before deleting this file. A decision without its numbers is not a decision.

## ⚠️ Traps

- **`/Ob3` is not `-O3`.** MSVC has no `/O3`; `/Ob3` only makes the *same* inliner more aggressive,
  while GCC/Clang's `-O3` adds vectorisation and loop unrolling. The two halves of this option are
  therefore **not the same experiment** and must be judged separately.
- **MSVC option order is load-bearing.** `/O2` *resets* the inline level to `/Ob2` and options are
  processed left to right, so the `/Ob` entry must stay **after** `/O2` in
  `EMERAUDE_COMPILE_OPTIONS`. Reordering the list disables `/Ob3` **in silence** — no diagnostic,
  because MSVC only reports two explicit options in conflict (`D9025`), never an implication of
  `/O2`. A measurement run on a reordered list would compare `/Ob2` against `/Ob2`.
- **`/WX` becomes inliner-dependent.** C4701/C4703 are emitted by the optimiser's dataflow
  analysis, so what it can prove changes with the inline level. C4701 is **not** in the
  suppression list, and warnings are errors by default: `/Ob3` can break the build of a file that
  did not change. That risk is part of the cost side of the measurement.
- **Whoever measures must state which machine.** Two workstations are in play (RTX 3070 Ti and
  RTX 3500 Ada); a CPU-side codegen change is not comparable across them.

## References

- Option declaration and the three flag entries: `CMakeLists.txt` (`EMERAUDE_COMPILE_OPTIONS`).
- Option row: [`../integration.md`](../integration.md) § Options.
- Why the default follows CEF, and the macOS-only authorisation:
  projet-alpha `docs/cef-integration.md` § *Two compile policies on one command line* and
  `docs/caution-points.md` § UI System (CEF).
- Related, narrower idea: [`increase-inlining.md`](increase-inlining.md) — moving small hot
  accessors into headers where a profile says so. That one is targeted; this one is the global knob.
