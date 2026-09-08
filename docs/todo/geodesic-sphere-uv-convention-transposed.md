---
id: geodesic-sphere-uv-convention-transposed
title: The geodesic sphere writes U and V transposed against the convention it claims to match
status: in-progress
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

Same radius, same subdivision argument, same material — `Grounds/Pavement002`, which declares a
**normal map at scale 1.0**, a height map (0.02) and `Reflection: Automatic`. Ratio **1.91**. The
owner reads the geodesic one as "the sphere is all black" in the main render.
⚠️ Corrected 2026-09-08: an earlier version of this item said the sphere carried `Walls/Bricks001`,
"the same material as the cube". Wrong — `Bricks001` is the CUBE's material
(`LightAndShadowDebug.cpp:182`); the sphere's is `Pavement002` (`:196`). The geodesic-vs-UV
measurement above is unaffected (same sphere, one argument changed), but "same material as the
cube" was never a valid comparison, and the grey tiled texture seen on the UV sphere was simply its
own `Pavement` albedo — NOT the MDI wrong-texture item.

## What remains — and the decision that comes first

- [x] **Owner decision, 2026-09-08: `generateSphere` is THE convention — align the geodesic one on
      it.** So: `U = 1 - (atan2(-X, Z) + pi) / (2*pi)` (longitude, running backwards),
      `V = acos(Y) / pi` (latitude). The reason it wins: `generateSphere`'s own ⚠️⚠️ comment
      documents *why* U runs backwards (a forward U "would grow it WESTWARD and mirror every
      texture"), and its spheres are the ones already validated on screen. The geodesic generator
      already CLAIMS this convention in its comment — the fix makes the code honest rather than
      inventing anything.
- [x] **UV convention FIXED (2026-09-08).** `subdivide()`'s leaf now writes
      `U = atan2(x, z)/(2π)` lifted when negative and `V = acos(y)/π`; the seam fix and the pole fix
      moved onto U, and the pole is detected by its XZ radius (`< 1e-8`) instead of `|y| > 0.999`,
      which used to flag hundreds of well-defined vertices from depth 5 on. Designed by one agent,
      **refuted by three independent adversarial readers — none refuted**: one compiled the leaf
      under the base's `-Werror` set with g++ 14 and clang 19 for `<float>` and `<double>` and
      measured the mapping on the binary (U(+Z) = {0, 1}, +X 0.25, −Z 0.5, −X 0.75; 0 westward
      edges at every depth). Three tests pin it — `geodesicSphereWritesGenerateSphereConvention`,
      `…UGrowsEastwardNotWestward`, `…SeamSitsOnPositiveZ` — and the geodesic sphere joined both
      winding gates. Suite: **2048/2048** (was 2045). Cascade compiles, zero VUID.
- [x] **Re-measured — and the transposition was NOT the darkening.** Same pose, same material:
      geodesic **32.35 → 33.07** / 255 after the fix; the UV sphere still reads **61.80**. The
      tangent-frame analysis predicted exactly this (a quarter-turn rotation of the frame, not a
      reflection, cannot halve the brightness). The UV defect was real and is closed; the black
      sphere is a **different defect on the same geometry**, still open below.

## What remains — the darkening, still unattributed

Falsified so far, each by a single-variable measurement or a compiled test — do not re-run them:

| Hypothesis | Instrument | Result |
|---|---|---|
| POM | `POMIterations` was at the engine default 0 when the sphere was already dark | dead |
| UV transposition / tangent frame | convention fixed, re-measured at the pinned pose | 32.35 → 33.07, dead |
| baked vertex colour | `generateSphere` writes the same `(n + 1) * 0.5` | dead |
| inverted winding | geodesic sphere added to `loopDrivenGeneratorsWindCCWAroundTheirNormals` and `…FacesOutward` | both pass, dead |

What the capture says: a **flat black disc with no shading gradient at all**, while its shadow on
the ground is present and correct. A wrong normal produces a wrong gradient, not a flat zero. This
reads as albedo ≈ 0 or skipped lighting on that geometry — the UV sphere under the same material
lights up with its texture visible.

- [ ] **Material A/B on the same geometry**: colour-only material (no texture) on the geodesic
      sphere. Lit ⇒ the defect is in texture sampling on this geometry (see the engine's open
      `mdi-wrong-texture-after-first-frame`); still black ⇒ the lighting path.
- [ ] Then RenderDoc on the sphere's draw (validation layers OFF for the capture, X11 — engine
      `docs/todo/renderdoc-layer-present-rejected.md`).

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
- ⚠️⚠️ **The tangent-frame "handedness flip" argument is WRONG for this engine, and it was checked
  independently.** `Math::Vector::tangent()` (`Vector.hpp:1409-1415`) reads ONLY the V component of
  the UV deltas; the bitangent is `cross(N, T) * handedness` with handedness never set on the
  computed path, so `det[T B N] = +1` structurally. Swapping U and V therefore ROTATES the (T, B)
  pair 90° about N — it does not reflect it — and reversing U is invisible to the code. Through the
  transposed frame a normal map's perturbation is applied a quarter turn off in the surface: a
  wrong-direction artefact. Whether that alone explains the ×1.91 is what the re-measure decides.
