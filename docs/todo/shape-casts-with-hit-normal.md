---
id: shape-casts-with-hit-normal
title: Ray and shape casts that answer the hit distance and the surface normal
status: in-progress
priority: unranked
scope: Math/Space3D/Intersections, Math/Space3D/Collisions
opened: 2026-10-01
tags: [physics, collisions, character-controller, physics-overhaul]
---

# Ray and shape casts that answer the hit distance and the surface normal

## Why

Phase P1 of the physics overhaul (engine `docs/physics-overhaul.md`). The engine's kinematic character controller
(engine item `kinematic-character-controller`) moves a capsule by collide and slide: it SWEEPS the capsule along the
wanted motion, stops at the first hit, slides along the hit normal. It also probes the ground below the feet. Today the
line / segment tests answer a bool and a hit point only — no distance (t), no normal — and there is no sphere or
capsule sweep at all.

## Done (2026-10-01)

- `Casts/ShapeCast.hpp`: `castRay` / `castSphere` / `castCapsule` against an `OrientedBox`, a `Triangle`, a `Sphere`, a
  `Capsule`, with a `CastHit` (fraction, point on the target, normal back towards the caster, started inside) —
  conservative advancement on the exact closest points. 13 tests (2 randomised), Release and ASan/UBSan green. Doc:
  `docs/subsystems/source-tree/18-math-space3d-casts.md`.

## What remains

- [ ] Casting a box (a moving `OrientedBox`), when a consumer needs it (the character controller casts a capsule).
- [ ] The old `Space3D/Intersections/` line tests: `Line` is infinite, `LineSphere.hpp:~119` / `LineCuboid.hpp:~159` can
  answer a hit behind the origin. Keep them as overlap queries or route their callers to `castRay`; decide when the
  engine's callers are reviewed (P2).

## ⚠️ Traps

- A cast that starts touching (t = 0) is the normal case for a character standing on the ground: it must answer a
  usable normal, not a degenerate one.
- No float division by a possibly-zero length (see `contact-manifold-generation` traps).

## References

- C. Ericson, *Real-Time Collision Detection* (2005), § 5.3 (ray tests), § 5.5 (moving objects).
- K. Fauerby, "Improved Collision Detection and Response" (2003) — the swept sphere vs triangle used by collide and slide.
- `src/Math/Space3D/Intersections/`.
