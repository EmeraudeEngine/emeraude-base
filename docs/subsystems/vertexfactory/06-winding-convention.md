## Winding convention (front faces are CCW)

A front face winds **counter-clockwise around its own outward normal** —
`VK_FRONT_FACE_COUNTER_CLOCKWISE`, the pipeline default. The `emitTriangle(A, B, C)` helpers emit in
**natural A/B/C order**: the historical B/C swap was a mirror compensation and is gone.

> [!IMPORTANT]
> **Verify a winding by COMPUTING it, never by eye.** Take `cross(B-A, C-A)` and check it against the
> generator's own declared outward normal. Gated by
> `loopDrivenGeneratorsWindCCWAroundTheirNormals`, which covers every loop-driven generator
> (sphere, cylinder, cone, disk, torus, capsule, hemisphere, tube, arrow) with `cuboid`, `plane` and
> `triangle` as **CONTROLS** — a probe that fails a control is a broken probe, not a discovery.
> All nine measured 100% correct in Aug 2026: the migration plan had listed them as "still mirrored"
> for weeks, and they were not.

> [!CAUTION]
> **Two triangle families carry NO evidence about winding. Excluding them is not optional.**
> 1. **Degenerate** triangles (zero area) — the collapsed quads at a sphere's poles.
> 2. Triangles **straddling a normal discontinuity** — a cap fan sharing a rim vertex with the side.
>    Averaging a radial normal with a cap normal yields a direction that is NOT the face's outward
>    normal, so the comparison is meaningless. **Omitting this filter reported 13 false positives on
>    `generateArrow` alone**, indistinguishable at a glance from a genuine mirror defect. With it,
>    the arrow is 15/15 clean.

> [!CAUTION]
> **The check is only valid while the normals are INDEPENDENT of the winding.** `ShapeGenerator`
> calls neither `computeVertexNormal()` nor `computeTriangleNormal()` — every normal is authored from
> the parametric surface — which is what makes it evidence rather than a tautology. That property is
> itself pinned by `theWindingCheckRejectsAMirroredShape`: it applies `Shape::reverseWinding()`
> (indices swapped, normals untouched) and requires **every** triangle carrying evidence to flip its
> verdict. If someone ever derives the normals from the winding, that test fails instead of the gate
> silently passing forever while measuring nothing. **Never delete it to "simplify" the suite.**

### The gem cuts are the exception: judge them against the SOLID, never their own normals

The twelve gem-cut generators pass a **separately computed** normal to their `emitTriangle()`, built
from a different vertex pair than the triangle it is attached to. The two drifted: measured Aug 2026,
**11 of the 12 cuts carried facet normals pointing INTO the solid** (princess: every single facet),
while their **winding was correct everywhere**.

> [!CAUTION]
> **Judging a gem with `countWindingAgreement()` reports the exact opposite of the truth** — it
> compares against precisely the normals that are wrong, so correct winding reads as "mirror-wound".
> That is why `gemCutsWindAndFaceOutward` references `faceCentre - centroid` instead, which owes
> nothing to the authored normals. ⚠️ That reference is valid for **convex** shapes only; every cut
> here is convex, but do not reuse it on a shape with a concavity.

**The fix, applied to all eleven generators:** a flat facet's normal IS its own geometric normal, so
`emitTriangle()` now derives it from the triangle being emitted and keeps the authored one only as a
degenerate fallback and for the per-face UV tangent frame (deliberately untouched, so UVs did not
move). Deriving it makes the drift **structurally impossible** rather than merely fixed.

> [!CAUTION]
> **Guard on the normalization RESULT, never on the input length.** `Vector::normalized()` gives up
> when `lengthSquared()` trips `Utility::isZero()` and returns the **ZERO vector**. A sliver can
> therefore clear a `length() > 1e-7` test and still come back with no normal at all — worse than an
> inward one, since it kills lighting outright. Two princess-cut culet slivers did exactly that
> (`length` 1.25e-4, `lengthSquared` 1.56e-8). A successful normalization has `lengthSquared() == 1`,
> so testing the **result** against `0.5` is unambiguous and needs no epsilon of its own. The gate
> uses the same criterion to decide which triangles carry evidence: one the library cannot normalize
> has no normal to judge.

> [!CAUTION]
> **`Shape::flipYAxis()` does NOT mirror the vertex colours.** It walks `m_vertices` and
> `m_triangles`; the colours live in the separate `m_vertexColors`. The gem cuts paint a VOLUMETRIC
> colour — position mapped to RGB — so while they were authored Y-down and mirrored afterwards,
> their green channel described the PRE-mirror frame, inverted against their own geometry. It was
> visible: the vertex-colour row of `parametric-geometries` iterates every shape, gems included.
> Fixed, then made moot by the re-authoring below. **Keep the rule**: anything derived from a
> position before a mirror is stale after it, and `flipYAxis()` will not tell you.

### The gem cuts are authored Y-UP (since Aug 2026)

All eleven gem generators author their facet math directly in the Y-up world: table above, culet
below. `convertYDownAuthoring()` and its eleven call sites are **deleted**.

> [!IMPORTANT]
> **Removing a mirror reverses handedness, and that has FOUR consequences, not one.** Each was
> needed at every converted site; missing any single one corrupts the output:
> 1. the authored `Y` literals negate;
> 2. every `emitTriangle()` swaps its first two vertices **and their UVs together** — that is what
>    `reverseWinding()` used to do;
> 3. the operands of each cross product feeding a per-face normal swap, since
>    `cross(M·e₁, M·e₂) = −M·cross(e₁, e₂)`;
> 4. **the bitangent** of the per-face tangent frame: `cross(normal, T)` becomes `cross(T, normal)`.
>    Forget it and `v` silently becomes `1 − v`.
>
> The stored normals need no attention at all: `emitTriangle()` derives the flat normal from the
> triangle it emits, so swapping the emission order corrects them by construction.

Guarded by `gemCutsKeepTheirGeometry`, which pins every cut through invariants taken over TRIANGLE
CORNERS. Two lessons are built into it, both learned the hard way during the conversion:

> [!CAUTION]
> **An invariant symmetric under the transformation it is meant to detect is not an invariant.**
> The plain sum of `V` cannot see the bitangent flip, because `Σ(1 − v) = n − Σv`, which equals `Σv`
> on every symmetric facet. Three cuts were converted with `V` flipped and the sums saw nothing;
> only the princess, whose chevrons are asymmetric, gave it away. The SQUARED sums do not cancel.
>
> **Never measure over `shape.vertices()`.** That array is the result of DEDUPLICATION, which
> depends on the emission order — exactly what re-authoring changes. The trillion came out with six
> fewer shared vertices, identical triangles and identical geometry, and every vertex-array sum moved
> with the count, accusing a correct conversion. The vertex count is a packing detail, not a shape.
