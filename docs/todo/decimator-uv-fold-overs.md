---
id: decimator-uv-fold-overs
title: ShapeDecimator folds UVs over — a few triangles of every LOD map their texture mirrored
status: open
priority: high
scope: emeraude-base VertexFactory (ShapeDecimator)
opened: 2026-10-07
tags: [vertex-factory, lod, uv, tangent-space]
---

# ShapeDecimator folds UVs over — a few triangles of every LOD map their texture mirrored

## Why

Measured while fixing the computed tangent frame (2026-10-07, base `docs/caution-points.md` § VertexFactory): a
32 × 16 UV sphere decimated to 50 % by `ShapeDecimator` has **27 of 448 triangles** whose UV winding is reversed
relative to their neighbours (their UV determinant changed sign), and 5 vertices of an unmirrored sphere end up with a
mirrored frame. The edge collapses keep a triangle from flipping in 3D, not in UV space: the texture is mirrored on
those triangles and their tangent frame sides with the fold.

## What remains

- [ ] A failing test: decimate a sphere (and a UV-seamed cuboid) and count the triangles whose UV determinant has the
      opposite sign of the source triangle they come from — 0 expected.
- [ ] Refuse a collapse that reverses a surrounding triangle's UV winding (the same test the decimator does for the 3D
      normal), or penalise it in the quadric cost (attribute-aware QEM: Garland & Heckbert 1998, Hoppe 1999).
- [ ] Then `decimatedVerticesKeepTheTangentHandedness` can assert on every vertex again, not only away from the folds.

## References

- `src/VertexFactory/ShapeDecimator.hpp`; test `VertexFactoryShapeDecimator.decimatedVerticesKeepTheTangentHandedness`.
- M. Garland, P. Heckbert, *Simplifying Surfaces with Color and Texture using Quadric Error Metrics* (1998).
- H. Hoppe, *New Quadric Metric for Simplifying Meshes with Appearance Attributes* (1999).
