---
id: geodesic-sphere-uv-convention-transposed
title: The geodesic sphere writes U and V transposed against the convention it claims to match
status: open
priority: high
scope: src/VertexFactory/ShapeGenerator.hpp (generateGeodesicSphere / subdivide)
opened: 2026-09-08
tags: [geometry, uv, tangent-space, measured]
---

# The geodesic sphere writes U and V transposed

## Why

`subdivide()` (the depth-0 leaf of `generateGeodesicSphere`) opens with:

> *"Compute spherical UV matching generateSphere() convention: tex.X = latitude […] tex.Y =
> longitude"*

**It does not match.** Read side by side:

| | U | V |
|---|---|---|
| `generateSphere` (UV sphere) | **longitude**, running BACKWARDS — `texCoordU` starts at 1 and decreases as theta grows, with its own ⚠️⚠️ comment saying a forward U would "grow it WESTWARD and mirror every texture" | **latitude** — `texCoordV` steps by `deltaV` per stack |
| `generateGeodesicSphere` | **latitude** — `1 - acos(Y)/pi` | **longitude** — `(atan2(-X, Z) + pi) / (2*pi)`, increasing |

So U and V are **transposed**, and the longitude additionally runs the opposite way.

## Why it matters beyond texture orientation

The tangent basis is derived from the UV gradients across a triangle. Transposing U and V exchanges
tangent and bitangent; reversing the longitude negates one of them. Together that is a **handedness
flip of the tangent frame**, and a normal map sampled through a flipped frame perturbs the shading
normal the wrong way. On a curved surface a large share of it then faces away from the light.

## Measured, 2026-09-08

`light-and-shadow-debug`, pinned pose `setPosition(-4, 2, 8)` / `lookAt(-4, 2, 1)`, pinned exposure
(f/16, 1/125, ISO 100), one variable changed — `generateSphereInstance`'s `useGeodesic`:

| Sphere | Region mean | max |
|---|---|---|
| geodesic (`useGeodesic = true`) | **32.35** / 255 | 122 |
| UV (`useGeodesic = false`) | **61.80** / 255 | 217 |

Same radius, same subdivision argument, same material (`Walls/Bricks001`, which declares a
**normal map** at scale 0.05 and a **height map**). Ratio **1.91**. The owner reads the geodesic one
as "the sphere is all black" in the main render; a cube carrying the same material renders correctly.

## What remains — and the decision that comes first

- [x] **Owner decision, 2026-09-08: `generateSphere` is THE convention — align the geodesic one on
      it.** So: `U = 1 - (atan2(-X, Z) + pi) / (2*pi)` (longitude, running backwards),
      `V = acos(Y) / pi` (latitude). The reason it wins: `generateSphere`'s own ⚠️⚠️ comment
      documents *why* U runs backwards (a forward U "would grow it WESTWARD and mirror every
      texture"), and its spheres are the ones already validated on screen. The geodesic generator
      already CLAIMS this convention in its comment — the fix makes the code honest rather than
      inventing anything.
- [ ] Move the seam fix and the pole fix onto whichever component ends up carrying the longitude —
      they currently operate on `Math::Y` of the UV, i.e. on the longitude under the present
      (transposed) layout. Fixing the axes without moving them re-breaks the seam.
- [ ] Re-measure the two spheres afterwards. **The transposition is proven; that it is the whole
      cause of the darkening is NOT.** Only the fix plus a re-measure closes that.

## ⚠️ Traps

- ⚠️⚠️ **Two hypotheses were already falsified on this defect — do not re-run them.**
  (1) *POM*: `Walls/Bricks001` does carry a height map, but the sphere was already dark with
  `POMIterations` at the engine default 0, so no POM code was emitted. (2) *Vertex colour*: the
  geodesic sphere bakes a volumetric `(position + 1) * 0.5` vertex colour, which multiplies the
  albedo since the material merge and averages 0.5 — a tempting match for the measured ×1.91, but
  `generateSphere` writes **the same** colour from `(normal + 1) * 0.5`, and on a unit sphere normal
  equals position. Both spheres carry it; it cannot explain a difference between them.
- ⚠️ Normals are NOT the problem: the geodesic leaf sets `setNormal(vectorA)`, the unit vector, which
  is exact for a sphere.
- ⚠️ Both generators receive the identical `getShapeBuilderOptions(false, false, false)`, so the
  difference is not in the builder options either.
- ⚠️ The geodesic sphere is **non-indexed** (one vertex triple per triangle, which is what lets the
  seam fix shift a V past 1.0). Any fix must keep that property or the seam fix stops working.
- ⚠️ A separate, unmeasured observation from the same capture: the UV sphere appeared to carry a grey
  tiled texture rather than the orange brick albedo the cube shows. That may be
  `mdi-wrong-texture-after-first-frame` (engine item) and must not be folded into this one.
