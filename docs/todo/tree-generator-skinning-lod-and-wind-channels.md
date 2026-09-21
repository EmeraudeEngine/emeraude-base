---
id: tree-generator-skinning-lod-and-wind-channels
title: Tree generator — generalized-cylinder skinning, leaf cards, LOD chain and wind channels
status: open
priority: unranked
scope: src/VertexFactory
tags: [vegetation, procedural, vertexfactory, lod]
opened: 2026-09-21
---

# Tree generator — generalized-cylinder skinning, leaf cards, LOD chain and wind channels

## Why

The skeleton phase is **done** (`TreeSkeleton`, `TreeParametricGrower`, `TreeColonizationGrower`,
Sept 2026 — see `src/VertexFactory/AGENTS.md` § *Vegetation*). What is left is the mesh.

Turning a `TreeSkeleton` into a `Shape` is where the stub's approach failed: merging one capped
cylinder per segment gives interpenetrating caps, no shared vertices between segments, no UV
continuity along a branch, and a triangle budget nobody can afford.

**Owner decisions (2026-09-21)**: the result is a **structured type**, not a bare `Shape`
(`{ Shape, TreeSkeleton, leaf attachments, bounds }`), and the **wind and AO vertex channels are
reserved from the start** rather than retrofitted.

## What remains

1. **Generalized cylinder**: one ring of N vertices per skeleton node, rings chained along a
   branch so consecutive segments **share** their ring, `u` around the branch and `v` = arc
   length / bark tiling scale. N decreases with the radius — that is the radial LOD, free.
   Tips close on an apex vertex, **never** a cap disk.
2. **Branch junctions**: the child's first ring seated on the parent surface with a collar. The
   cheap version overlaps and widens; a proper bifurcation stitch is the quality version. Decide
   which, and say why, in the code.
3. **Leaf cards** in a **second `Shape` group** (`Shape::newGroup()`), so the engine's
   `Interface::buildSubGeometries()` turns them into a second sub-geometry and
   `MultiLayerMeshResource` can give them their own alpha-masked, double-sided material.
4. **Vertex colour channels**, Crytek/SpeedTree convention: **R** = trunk bending weight,
   **G** = branch bending weight, **B** = leaf flutter phase, **A** = baked AO. All four are
   nearly free at skinning time (branch order and arc length are already known) and expensive to
   reconstruct afterwards.
5. **LOD chain**: re-skin the *same* skeleton with fewer rings and clustered leaf cards, rather
   than decimating the mesh. `ShapeDecimator` (QEM) is the wrong tool for foliage — it cannot
   merge leaf cards into bigger ones.
6. **Last LOD geometry**: the crossed-quads / single-card geometry only. Baking the atlas that
   card samples is engine work — `vegetation-octahedral-imposter-atlas` in emeraude-engine.

## Traps

- ⚠️⚠️ **Build with `options.enableDataEconomy(false)` and dedup with
  `ShapeProcessor::deduplicateVertices()` afterwards.** The default per-corner linear scan makes a
  65 536-triangle shape cost 8.7 s against 24.9 ms. Measurements: `docs/caution-points.md`
  § VertexFactory, open item `shape-addvertex-dedup-is-quadratic`.
- ⚠️ Tangents are computed for you by `ShapeBuilder::endConstruction()`; do not hand-roll them.
- ⚠️ `ShapeBuilder` is a **streaming** builder — attributes then `newVertex()`, a triangle every
  three vertices. There is no index API, so a shared ring is expressed by emitting the same
  attributes twice and letting the dedup pass collapse them.
- ⚠️ Normal maps are **Khronos convention (+Y up)**. Bark authored the other way is fixed in the
  texture, never by a flag here.

## References

- Input: `src/VertexFactory/TreeSkeleton.hpp`, grown by either grower. ⚠️ Read the
  parent-before-child contract and the pipe-model taper limit in
  `src/VertexFactory/AGENTS.md` § *Vegetation* before walking a skeleton.
- Engine consumers: `vegetation-renderable-and-lod-chain`, `vegetation-wind-shader`,
  `vegetation-octahedral-imposter-atlas`.
