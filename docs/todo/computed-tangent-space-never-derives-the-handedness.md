---
id: computed-tangent-space-never-derives-the-handedness
title: The computed tangent space never derives the bitangent handedness — a mirrored UV island lights its normal map backwards
status: in-progress
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

## Owner decision (2026-10-07)

Derive the handedness from the UV winding (Lengyel 2001 / MikkTSpace): per triangle the sign of (Δu1·Δv2 − Δu2·Δv1), per vertex the sign of its triangles.

## What remains

Done (2026-10-07): the frame derived from the UV winding in every computed path, the OBJ loader splitting mirror-seam
vertices, three failing-then-passing tests, Release + ASan / UBSan green (base `docs/caution-points.md` § VertexFactory).

- [ ] **Runtime**: generated shapes change where their UVs are mirrored (torus, capsule, half the hollowed cube, a cap
      of the cylinder and the cone). Look at them under `normal-map-debug` with a directional normal map (and the
      option-4 Khronos control): the bumps must be lit from the same side as on the cuboid / sphere. Then delete this
      item.

## References

- Eric Lengyel, *Computing Tangent Space Basis Vectors for an Arbitrary Mesh* (2001).
- MikkTSpace (Morten S. Mikkelsen), the glTF 2.0 reference for generated tangents.
