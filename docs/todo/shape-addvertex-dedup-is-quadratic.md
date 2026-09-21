---
id: shape-addvertex-dedup-is-quadratic
title: Shape::addVertex() deduplicates with a linear scan, so the data-economy path is quadratic
status: open
priority: unranked
scope: src/VertexFactory/Shape.hpp
tags: [performance, vertexfactory]
opened: 2026-09-21
---

# Shape::addVertex() deduplicates with a linear scan, so the data-economy path is quadratic

## Why

`ShapeBuilderOptions::dataEconomyEnabled()` defaults to **true**, so every generator routes each
of its triangle corners through `Shape::addVertex()` (`src/VertexFactory/Shape.hpp:1591`) and
`Shape::addVertexColor()` (`:1615`). Both walk the whole existing list looking for a match, so
building a shape costs O(corners x vertices).

Measured on this workstation (GCC, `-O2`, `generateSphere`), after the `addEdge()` fix of
2026-09-21 removed the *other* quadratic term:

| Sphere | Triangles | economy ON | economy OFF + `ShapeProcessor::deduplicateVertices()` |
|---|---|---|---|
| 16x8 | 256 | 0.4 ms | 0.2 ms |
| 32x16 | 1 024 | 4.3 ms | 0.4 ms |
| 64x32 | 4 096 | 48.5 ms | 1.7 ms |
| 128x64 | 16 384 | 567.9 ms | 6.3 ms |
| 256x128 | 65 536 | **8 742.8 ms** | **24.9 ms** |

The economy path still scales as n^2 (4x triangles -> ~15x time); the streamed path is linear
(4x -> 4.0x). At 65 536 triangles the gap is **351x**.

The hashed path is also *better* at merging, not worse: it returns 33 169 vertices where the
economy path returns 36 405. `Vector::operator==` compares with `Utility::equal()` and an
**absolute** `std::numeric_limits<float>::epsilon()` (1.19e-7), which is finer than the rounding
the generators actually produce, so the linear scan misses merges that the 1e-4 quantised hash
finds.

No procedural generator that produces a dense mesh can use the default today. The tree generator
work (`tree-generator-skinning-lod-and-wind-channels`) will disable data economy and dedup
afterwards, but
every other caller silently pays the cost.

## What remains

The fix is not mechanical, because it changes a merge semantic:

1. `ShapeProcessor::deduplicateVertices()` already owns a correct hashed dedup (quantised key ->
   compact index, `src/VertexFactory/ShapeProcessor.hpp:116`). Making `addVertex()` hash-based
   means giving it the **same quantised** equality, i.e. replacing an epsilon comparison by a grid
   comparison for every existing caller.
2. **Owner decision needed** — three candidate shapes:
   - keep `addVertex()` as is, flip `m_dataEconomyEnabled` to **false** by default, and let callers
     that want compaction call `ShapeProcessor` (smallest code, changes what every generator
     returns);
   - give `Shape` a construction-time hash index for vertices, mirroring `m_unpairedEdges`
     (keeps the API, changes the merge tolerance);
   - leave both and only document the trap.
3. Whatever is chosen, `addVertexColor()` has the same defect and must move with it.

## Traps

- Do **not** hash the raw float bits to "preserve" the current semantics: epsilon equality is not
  transitive, so no hash can reproduce it. Any hashed dedup *is* a semantic change; say so.
- Measure with `economy OFF` too. The two paths do not return the same vertex count, so a
  regression test that pins a vertex count will fire on this change.

## References

- Fixed sibling defect (same file, 2026-09-21): `Shape::addEdge()` pairing, see `docs/caution-points.md`.
- `src/VertexFactory/ShapeProcessor.hpp:106` — `deduplicateVertices()`, the blessed hashed path.
