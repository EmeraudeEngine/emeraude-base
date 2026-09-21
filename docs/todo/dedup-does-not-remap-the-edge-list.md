---
id: dedup-does-not-remap-the-edge-list
title: ShapeProcessor::deduplicateVertices() leaves the edge list pointing at dead vertex indices
status: open
priority: unranked
scope: src/VertexFactory/ShapeProcessor.hpp
tags: [vertexfactory, correctness]
opened: 2026-09-21
---

# ShapeProcessor::deduplicateVertices() leaves the edge list pointing at dead vertex indices

## Why

`deduplicateVertices()` (`src/VertexFactory/ShapeProcessor.hpp:106`) rebuilds
`m_shape.vertices()` and remaps every triangle's three vertex indices through its `remapping`
table. It does not touch `m_shape.edges()`:

- each `ShapeEdge` still holds the **pre-merge** vertex indices, which may no longer exist;
- each `ShapeTriangle::edgeIndex()` still points into the **old** edge numbering;
- the shared-edge cross-links are stale for the same reason.

So `shape.edges()` is garbage after a dedup. It is latent rather than live: nothing in the cascade
consumes the edge list today (`Silhouette` has no caller anywhere), and the generators that dedup
do not need edges. It will stop being latent the first time someone runs silhouette extraction, a
hole analysis or an adjacency pass on a deduplicated shape — and it will look like a defect in
THAT pass, not here.

Found while writing the tree skinner, which deduplicates every mesh it builds.

## What remains

Pick one, it is an owner decision because the three have different costs:

1. **Remap the edges too.** The `remapping` table is already there; the difficulty is that two
   distinct edges can collapse onto the same vertex pair, so the edge list has to be rebuilt and
   the cross-links recomputed, not just renumbered.
2. **Drop the edges and say so.** `m_shape.edges().clear()` plus a note: a dedup invalidates the
   adjacency, rebuild it if you need it. One line, honest, and it turns a silent wrong answer into
   an obvious empty one.
3. **Rebuild them.** Re-run the `addEdge()` pass over the remapped triangles. Now cheap, since
   `addEdge()` became O(1) on 2026-09-21 — it would have been unthinkable before.

## Traps

- ⚠️ A closed-looking shape is not watertight in the edge sense: `generateSphere()` leaves its UV
  seam unmerged, so "every edge is paired" is false even before any dedup. See
  `docs/caution-points.md` § VertexFactory.
- ⚠️ Whatever is chosen, cover it: the two edge-pairing tests in
  `src/Testing/test_VertexFactoryShapeBuilder.cpp` run on a shape that was never deduplicated, so
  they would not have caught this.

## References

- `docs/caution-points.md` § VertexFactory — the finding and why it is latent.
- Sibling item: `shape-addvertex-dedup-is-quadratic`.
