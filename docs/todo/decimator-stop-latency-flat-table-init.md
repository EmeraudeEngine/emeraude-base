---
id: decimator-stop-latency-flat-table-init
title: The decimator's stop latency exceeds the 50 ms bound on Windows (68.9 ms): its large flat tables are built without a stop check
status: open
priority: high
scope: src/VertexFactory/ShapeDecimator.hpp, src/FlatHashMap (construction)
opened: 2026-10-15
tags: [ave-robustus-ii, concurrency, latency, decimator, windows]
---

# The decimator's stop latency exceeds the 50 ms bound on Windows (68.9 ms): its large flat tables are built without a stop check

## Why

Owner decision D7 (2026-10-08, plan Ave Robustus II § 7): a cancellable computation stops within **50 ms**. Measured by
Window-PA on 2026-10-15 (the new Windows machine: i7-13850HX, 8 P + 12 E cores, MSVC 19.44 Release, base `a673299`),
`DISABLED_StopLatencyOnALargeMesh` (2.24 M triangles):
- the unit test passed 3 / 3 (worst 46.4 / 29.2 / 44.7 ms), by luck: its 400 ms row only sometimes falls in the window
  below;
- a stage-timed copy of `ShapeDecimator.hpp` (repository file untouched) and a 250-900 ms sweep in 10 ms steps: worst
  **68.9 ms**, also 58.5, 52.1, 50.7, 47.6 ms.

Two causes, measured:
1. **The defect — a detection gap.** Each `FlatHashMap{(triangles * 3) / 2}` constructor allocates and value-initializes
   `bit_ceil(6.72 M)` = 8.4 M slots without a stop check: ~200 MB for the penalties' `EdgeTally` table, ~134 MB for
   `buildCollapseQueue()`'s `processedEdges`. A stop landing there is seen only once the table is ready: ~48 ms after
   the penalty stage starts, ~26 ms after the queue stage starts, ~9 ms in the corner-UV stage, ~18 ms in the work mesh
   (the start row). Then the stage's own table free (~5 ms) and the unwinding below follow. The output stage builds the
   same kind of table (`buildOutputShape()`'s `vertexMap{triangles.size()}`, 8.4 M slots, ~200 MB): not measured (no
   sweep row stopped the output stage), very likely the same gap.
2. **The floor — the release at return under MSVC (~20 ms).** Stopped during the collapses, the stop is SEEN within
   0.01-2 ms, then the release costs: the queue vector 4.3-6.4 ms, the corner-UV table 0.7, triangles + vertices
   6.3-9.4, the arena 5.4-11, the work mesh 0.8 ms. Pinned to E-cores ~30 ms, to P-cores ~20 ms (the unpinned worker
   ran on a P-core). Linux pays ~1.5 ms for the same release, the M2 less.

Full uninterrupted decimation there: 10.3 s (work mesh 122 ms, init 175, corner UV 77, quadrics 83, penalties 408,
queue 767, collapses 7482, output 1181) — about 2.5 times Linux.

## What remains

- Close the detection gap (owner to choose, with the A/B of the plan): an interruptible table initialization (fill in
  chunks with a stop check), or smaller / lazily-touched tables (sized to what the stage really inserts). A third form
  of the second: a control-byte layout in the style of SwissTable / `absl::flat_hash_map`, where only a 1-byte-per-slot
  metadata array is cleared (8.4 MB instead of 200 MB) — it bounds the gap only if that clear stays under the bound at
  the worst mesh size (https://abseil.io/about/design/swisstables).
- Then the release floor under MSVC if it still breaks the bound with the gap closed (e.g. one arena for the large
  tables, released once).
- Proof: the 250-900 ms sweep on Windows, worst < 50 ms on P- and E-cores; Linux and the M2 unchanged; the goldens
  unchanged.

## ⚠️ Traps

- The output must stay bit-identical on each OS (`DISABLED_PrintGoldenFingerprints`): the table initialization and the
  release do not change the decimation, so a golden that moves means the change went too far.
- Cross-OS goldens (a separate fact, for the open `-ffp-contract` / libm decision in `task-handle-and-stop-token`): on
  this Windows machine 4 / 8 goldens equal Linux's (sphere48, hollowedCube, capsule, cylinder); sphere300 has 11112 vs
  11113 vertices; torus, geodesic and sphere48-normalmap have the same counts but other hashes. MSVC with `/fp:precise`
  and no `/arch` (SSE2) emits NO FMA, so on Windows the difference is the libm or MSVC's evaluation of the same
  expressions, not contraction.
- Measuring only the unit test's fixed delays hides the defect: time the stages or sweep the delays finely.

## References

- Base item `task-handle-and-stop-token` (the stop token in every stage, the earlier latency history).
- Base `docs/caution-points.md` § VertexFactory (the release floor history); engine
  `docs/subsystems/graphics/20-15b-level-of-detail.md` (the LOD job's stop).
- projet-alpha `docs/plans/ave-robustus-ii.md` § 5.2 (found on the way), § 7 D7.
