---
id: computed-tangent-space-never-derives-the-handedness
title: The computed tangent space never derives the bitangent handedness — a mirrored UV island lights its normal map backwards
status: open
priority: high
scope: src/VertexFactory/Shape.hpp (computeTriangleTangent, computeVertexTangent, computeVertexTBNSpace) and their callers
opened: 2026-10-06
tags: [vertex-factory, tangent-space, owner-decision]
---

# The computed tangent space never derives the bitangent handedness

## Why

Found while fixing the tangent-frame defects of 2026-10-06 (`docs/caution-points.md` § VertexFactory). When the
engine COMPUTES a tangent frame — every generator through `ShapeBuilder::endConstruction()`, the OBJ loader
(`FileFormatOBJ.hpp`, `computeTriangleTangent()` + `computeVertexTangent()`), the UV unwraps of
`ShapeProcessor` — only the tangent T is derived from the UVs (`Math::Vector::tangent()`, V deltas). The
handedness is never set, so the shader rebuilds B = cross(N, T) · (whatever the vertex held, +1 by default).
On a UV island that is MIRRORED (the UV winding of its triangles is reversed — the usual way a symmetric model
saves texture space), the true bitangent is -cross(N, T): the normal map is lit backwards there.

Side effect inside `ShapeProcessor::generateUVUnwrap()`: the vertices it duplicates start at +1, those it keeps
retain their OLD handedness, although the frame is recomputed from the NEW UVs — two conventions in one shape.

Read from the code, not measured. A file carrying its tangents (glTF `TANGENT` vec4, the native format) is not
affected.

## What remains — owner decision first

- [ ] **Decision**: derive the handedness from the UV winding (the standard, Lengyel 2001 / MikkTSpace: per
      triangle `sign((Δu1·Δv2 − Δu2·Δv1))`, per vertex the sign of its triangles — a vertex shared by triangles of
      both signs is a seam the loader must split), or keep +1 everywhere and document mirrored UVs as unsupported
      without authored tangents. Recommendation: derive it — it changes the rendering ONLY where it is wrong
      today (mirrored islands), and glTF without `TANGENT` is required by the spec to use MikkTSpace.
- [ ] A failing test: a quad whose UVs are mirrored in U gets handedness -1 from the computed path.
- [ ] Runtime: an OBJ with a mirrored island under `normal-map-debug`.

## References

- Eric Lengyel, *Computing Tangent Space Basis Vectors for an Arbitrary Mesh* (2001).
- MikkTSpace (Morten S. Mikkelsen), the glTF 2.0 reference for generated tangents.
