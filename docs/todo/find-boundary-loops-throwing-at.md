---
id: find-boundary-loops-throwing-at
title: ShapeProcessor::findBoundaryLoops() calls the throwing .at()
status: open
priority: high
scope: src/VertexFactory/ShapeProcessor.hpp
opened: 2026-10-08
tags: [ave-robustus, vertexfactory, defect]
---

# ShapeProcessor::findBoundaryLoops() calls the throwing .at()

## Why
Seen during the P1 work on `deduplicateVertices()` (2026-10-08): `findBoundaryLoops()` reads `canonicalMap.at(index)`.
`.at()` throws on a missing key — an abort under `-fno-exceptions`, forbidden by the cascade rules (Ave Robustus). The
map is built from the same vertices, so a miss means an inconsistent shape (an index past the vertex array).

## What remains
- Bounds checked explicitly (`find()` + refusal with a log), a test with a triangle indexing past the vertex array.
- Grep the base for the other `.at(` calls on containers (std::map / unordered_map / vector) at the same time.

## References
- projet-alpha `docs/plans/ave-robustus-ii.md`.
