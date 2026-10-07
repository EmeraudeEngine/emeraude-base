---
id: physics-p1-leftovers
title: Physics P1 leftovers — wanted only when a consumer needs them
status: parked
priority: unranked
scope: emeraude-base Math (RigidBody, Space3D Contacts / Casts)
opened: 2026-10-07
tags: [physics-overhaul, math]
---

# Physics P1 leftovers — wanted only when a consumer needs them

## Why parked

Owner decision (2026-10-07): the P1 items `contact-manifold-generation`, `rigid-body-math-helpers` and
`shape-casts-with-hit-normal` are closed — what they delivered is documented in `docs/subsystems/source-tree/17-…`,
`18-…`, `19-…`. What they still listed has no consumer in the engine today; it is kept here, parked, so nobody
re-derives it, and is picked up when a consumer appears.

## What remains (each with its tests, Release + ASan / UBSan, on the model of `test_MathSpace3DContacts.cpp`)

- The sum of several inertia tensors for a COMPOUND body, each moved by `parallelAxis()` first (P3 derives an entity's
  tensor from its one collision shape).
- A `Quaternion` from a `Matrix< 3 >` (today only from a `Matrix< 4 >`).
- Casting a moving `OrientedBox` (the character controller casts a capsule).
- A fast AABB path for the contact pairs with the SAME output as the unrotated `OrientedBox` they use today — only if
  a measurement shows the oriented path costs (the overlap tests of `Collisions/` already go through it).

## References

- `src/Math/RigidBody.hpp`, `src/Math/Quaternion.hpp`, `src/Math/Space3D/Contacts/`, `src/Math/Space3D/Casts/`.
- Engine `docs/physics-overhaul.md`.
