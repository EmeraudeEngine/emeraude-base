---
id: contact-manifold-generation
title: Contact manifolds — 1 to 4 contact points with feature ids for every solid pair
status: open
priority: unranked
scope: Math/Space3D/Collisions, Math/OrientedCuboid
opened: 2026-10-01
tags: [physics, collisions, physics-overhaul]
---

# Contact manifolds — 1 to 4 contact points with feature ids for every solid pair

## Why

Phase P1 of the physics overhaul (engine `docs/physics-overhaul.md`). Every pair test here answers one MTV (a normal
and a depth), never a contact point. The engine solver therefore gets ONE contact per pair, and a box resting on one
point rocks and tips at random. Stable resting and stacking need a manifold: up to 4 points on the contact face, each
with its own depth and a stable feature id so the solver can warm-start from one tick to the next.

## What remains

- [ ] A manifold result type: shared normal (convention stated once, A → B or "push A out of B"), up to 4 points
  (`StaticVector`), per-point depth, per-point feature id (which face / edge / vertex of each shape made it).
- [ ] Oriented box ↔ oriented box: SAT on the 15 axes (exists in `OrientedCuboid::isIntersecting()`), then
  reference / incident face selection and Sutherland-Hodgman clipping, then reduction to 4 points; edge ↔ edge case
  gives one point from the closest points of the two edges. A tolerance that prefers face axes over edge axes, so the
  chosen axis does not flicker (Gregorius, GDC 2015).
- [ ] Sphere ↔ oriented box, capsule ↔ oriented box (2 points when the capsule lies on a face), sphere / capsule ↔
  triangle with a contact point. The AABB variants become the special case of the oriented ones, or stay as fast
  paths with the same output.
- [ ] Unit tests (Release + ASan/UBSan): box resting flat (4 points, depth exact), box on an edge (2), on a corner (1),
  rotated 45° about Y on a face, deep penetration, coplanar and degenerate inputs; the feature ids stay identical
  when the box moves by a tiny amount.

## ⚠️ Traps

- Never `Vector / s` on a depth or a length: `operator/` is NaN for `|s| <= epsilon`. Guard `> FLT_MIN`, multiply by
  `1 / s` (triad 11).
- The engine negates the base normal before the solver (engine `docs/subsystems/physics/05-…`). Choose the manifold's
  convention once and document it; do not add a third convention.
- Correctness must not depend on bit-exact floats: the owner may enable `-ffast-math` one day.

## References

- D. Gregorius, "The Separating Axis Test between Convex Polyhedra" (GDC 2013), "Robust Contact Creation for Physics
  Simulations" (GDC 2015). C. Ericson, *Real-Time Collision Detection* (2005), § 4.4, § 5.1.
- Box2D v3 `manifold.c` (MIT) for the clipping and the feature ids — cite at the point of use if code is followed.
- `src/Math/OrientedCuboid.hpp:158`, `src/Math/Space3D/Collisions/`.
