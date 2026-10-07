---
id: collision-pair-test-defects
title: Collision pair tests — wrong deep cases, approximate closest points, a reversed MTV
status: open
priority: high
scope: Math/Space3D/Collisions, Math/Space3D/SAT.hpp
opened: 2026-10-01
tags: [physics, collisions, physics-overhaul]
---

# Collision pair tests — wrong deep cases, approximate closest points, a reversed MTV

## Why

Phase P1 of the physics overhaul (engine `docs/physics-overhaul.md`). A read-only survey on 2026-10-01 found these.
None is proven by a test yet: each one starts with a failing unit test.

## Owner decision (2026-10-01)

The MTV overloads of `Collisions/` were only used by the engine's `Physics/*CollisionModel` (lights, the editor and the
octree use the boolean overlaps). The physics moved to `Contacts/` (engine P2) and the engine's model wrappers were
removed (P3.a, 2026-10-02). Since nothing in the engine calls the MTV overloads, the plan was to RETIRE them; the owner
REVISED it on 2026-10-02: the base keeps them, they may serve other cases. Every defect below therefore stays to fix
(MTV and boolean alike), each starting with its failing test.

## What remains

- [ ] Tri ↔ tri: `SAT::checkCollision()` builds the MTV "from A to B" (`SAT.hpp:~221-230`) and `SamePrimitive.hpp:96`
  returns it as is, while every other pair pushes A OUT of B. Prove, then align.
- [ ] (A correct manifold version exists: `Contacts/CapsuleTriangle.hpp` pushes towards the capsule centre's side; the
  overlap test below stays wrong for its callers.) Capsule ↔ triangle, axis piercing the triangle (`CapsuleTriangle.hpp:215-219`): MTV = `normal * radius`,
  whatever side the capsule is on and however deep it is. A deep capsule can be pushed THROUGH the triangle — the
  terrain case of a capsule character.
- [ ] Capsule ↔ AABB, deep case (`CapsuleCuboid.hpp:166-215`): only the single closest axis point is pushed out; an
  axis spanning the box leaves the other end inside.
- [ ] (An exact segment ↔ box closest-point routine now exists: `Contacts/CapsuleBox.hpp`
  `CapsuleBoxDetail::closestOfSegmentAndBox()` — reuse it.) `closestPointsCapsuleTriangle` / `closestPointsCapsuleCuboid`: a fixed 4-iteration alternating projection,
  inaccurate for a segment nearly parallel to a face or an edge. Replace by an exact segment ↔ triangle /
  segment ↔ box closest-point routine (Ericson § 5.1.9, § 5.1.10).
- [ ] Degenerate fallbacks answer a hard-coded `negativeY()` MTV (`SamePrimitive.hpp:162/254`, `CapsuleSphere.hpp:103`).
  Owner decision (2026-10-01, for the contact manifolds): a FIXED deterministic axis, not a caller-supplied direction;
  the manifolds use +Y. Align these overlap tests on the same rule (their MTV pushes A out of B, so check which sign
  matches the manifolds' +Y normal from A to B: an MTV of −Y).
- [ ] Tri ↔ sphere / tri ↔ capsule inside tests assume one winding (`c1, c2, c3 >= 0`): check the other winding gives
  the same answer.

## References

- C. Ericson, *Real-Time Collision Detection* (2005), ch. 5.
- `src/Testing/test_MathSpace3D.cpp` (~216 tests today), `src/Testing/test_MathOrientedCuboid.cpp` (1 test).
