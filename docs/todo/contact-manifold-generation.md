---
id: contact-manifold-generation
title: Contact manifolds — 1 to 4 contact points with feature ids for every solid pair
status: in-progress
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

## Owner decisions (2026-10-01)

Header-only `src/Math/Space3D/Contacts/`; a new `Space3D::OrientedBox` primitive (centre, axes, half extents); the
manifold normal points FROM A TO B; order box ↔ box, sphere ↔ box, capsule ↔ box, sphere / capsule ↔ triangle, the
sphere / capsule pairs (engine `docs/physics-overhaul.md` § 1.5).

## Done (2026-10-01)

- `Space3D::OrientedBox`, `ContactPoint` / `ContactManifold` and box ↔ box (`Contacts/BoxBox.hpp`), 15 tests, Release and
  ASan/UBSan green, clang-tidy 21.1.6: 0 new finding except one `cppcoreguidelines-pro-bounds-constant-array-index` on
  `OrientedBox::axis(index)` (`@pre index < 3`), kept on purpose (owner, ledger). Doc:
  `docs/subsystems/source-tree/17-math-space3d-contacts.md`.
- Sphere ↔ box (`Contacts/SphereBox.hpp`) and capsule ↔ box (`Contacts/CapsuleBox.hpp`, exact segment-box closest
  points, 2 points along a face), both orders; 32 tests in all including 3 randomised property tests.
- Sphere ↔ triangle, capsule ↔ triangle (two-sided; a piercing capsule leaves towards its centre's side), sphere ↔
  sphere, sphere ↔ capsule, capsule ↔ capsule (2 points when parallel): 50 tests, 6 of them randomised.

## What remains

- [ ] Decide at the engine integration (P2, `physics-unified-contact-pipeline`): the AABB pairs become the special case
  of the oriented ones (an unrotated `OrientedBox`), or keep a fast path with the same output — measure first.
- [ ] Owner decision pending: one-sided triangles for the meshes of P5. (The +Y fallback for coincident centres is
  decided: kept, determinism first — owner 2026-10-01.)
- [ ] Unit tests for each new pair (Release + ASan/UBSan), on the model of `test_MathSpace3DContacts.cpp`.

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
