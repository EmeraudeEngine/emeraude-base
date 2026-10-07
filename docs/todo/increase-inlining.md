---
id: increase-inlining
title: Increase inlining where it pays
status: in-progress
priority: low
scope: cascade-wide (emeraude-base first)
opened: unknown
tags: [performance, cpp]
---

# Increase inlining where it pays

## What remains

- [ ] Continue moving small, hot accessors into the headers, and **measure**. The project rule is
  RUNTIME > READABILITY > COMPILE TIME, so this is legitimate work — but it is worth doing only
  where a profile says so, never as a blanket "inline everything".
- [ ] Then the engine, then projet-alpha.

⚠️ The historical entry was marked WIP with no record of what was already covered.

## First profile-driven pass (2026-10-07, Linux, RTX 3070 Ti)

`perf` on `citadel` (fixed pose) and `balls-of-steel`, 20 s each, Release. Where the CPU goes: OpenAL Soft's mixer
(its own thread) 12–16 %, shared-count atomics 3.5 %, mutexes 3–5 %, `dynamic_cast` 1.2–1.6 %. The only small hot
out-of-line accessor: `Vulkan::AbstractDeviceDependentObject::device()` (by-value `shared_ptr`, ~280 call sites).
A/B, inline + `const &`: **295.0 → 295.5 FPS** (median of 5 alternated runs, runs 283–298) — noise, under the 5 % gate
of Ave Performus: NOT adopted, reverted. ⚠️ Both benches are GPU-bound (93–99 % GPU): an inlining gain cannot show
in the frame time there — a further pass needs a CPU-bound bench (or CPU-ms per frame, `perf stat`), else this item
measures nothing. The `MeshResource::geometry()` mutex (from the same profile) is an engine change: no gain either,
kept as a race fix (engine `docs/subsystems/graphics/20-15b-level-of-detail.md`).

## Where it starts, and why the item lives here

**Owner decision (2026-08-26): emeraude-base owns this sweep.** It is cascade-wide work, and the
foundation is where the convention is set — the doctrine of [`docs/plans/ave-robustus.md`](../plans/ave-robustus.md)
applies verbatim: *"emeraude-base first. The engine and projet-alpha inherit the hardening later."*
The item was inherited from the engine's historical root `TODO.md`, written before `EmEn::Libs`
was extracted into this repository.

It is consolidation, not a feature. (It was opened under the "Ave robustus!" feature freeze,
which the owner lifted on 2026-08-27 with the plan's closure — no constraint remains.)
