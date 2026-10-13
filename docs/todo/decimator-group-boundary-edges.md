---
id: decimator-group-boundary-edges
title: ShapeDecimator does not protect the edges between two groups (materials)
status: open
priority: unranked
scope: src/VertexFactory/ShapeDecimator
opened: 2026-10-13
tags: [vertexfactory, lod, decimation]
---

# ShapeDecimator does not protect the edges between two groups (materials)

## Why
A multi-material mesh keeps its groups through decimation: each work triangle carries its source group
(`src/VertexFactory/ShapeDecimator.hpp:953-1012`, `triGroupMap`), and the output emits the survivors in group order
and rebuilds the group ranges (`:1771-1836`). The work mesh is built from the source WITHOUT copying it (`WorkMesh`,
`:630`; `buildWorkMesh()`, `:857`), so the former "save the groups before `deduplicateVertices()`" concern is gone.

What is still missing: the edge where two groups meet is not penalised. `applyBoundaryAndSeamPenalties()` (`:1144`)
anchors only open-boundary edges (used by one triangle) and UV-seam vertices. A group boundary on a position-welded
surface whose UVs are continuous is an ordinary interior edge for the QEM: a collapse can move it, so the border
between two materials drifts or zig-zags at the coarser levels.

Not a defect of what exists (the groups stay valid): an improvement, waiting for the owner's ranking.

## What remains
- Choose (owner): a penalty on the vertices / edges shared by triangles of different `groupIndex` (the same weighted
  point quadric as the UV seams, `m_boundaryPenaltyWeight`), or a per-group independent decimation (the most reliable
  for multi-material meshes, but every group then keeps its own open border).
- A unit test: a two-group welded plane, decimated, its group border still on the source line.

## References
- Engine item `geometry-lod-storage-architecture` (the consumer: automatic LODs, `MeshResource::generateLODLevel()`).
