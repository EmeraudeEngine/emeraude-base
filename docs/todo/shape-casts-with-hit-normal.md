---
id: shape-casts-with-hit-normal
title: Ray and shape casts that answer the hit distance and the surface normal
status: open
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

## What remains

- [ ] A hit result: `t` in [0, 1] along the cast, point, normal of the surface hit (pointing back towards the caster),
  and whether the cast STARTED inside (t = 0, with a depenetration direction).
- [ ] Ray / segment vs sphere, AABB, oriented box, capsule, triangle with that result. Rays are half-lines: never a
  hit behind the origin (today `LineSphere.hpp:~119` and `LineCuboid.hpp:~159` can answer t < 0 for the infinite
  `Line`).
- [ ] Sphere cast and capsule cast vs sphere, AABB, oriented box, capsule, triangle (time of impact by conservative
  advancement or by Minkowski-sum reduction to a ray, Ericson § 5.5).
- [ ] Unit tests: grazing hits, start-inside, parallel to a face, zero-length cast, a capsule cast onto a 0.29 m step
  edge (the citadel stairs), onto a 30° slope (the normal must be the slope's).

## ⚠️ Traps

- A cast that starts touching (t = 0) is the normal case for a character standing on the ground: it must answer a
  usable normal, not a degenerate one.
- No float division by a possibly-zero length (see `contact-manifold-generation` traps).

## References

- C. Ericson, *Real-Time Collision Detection* (2005), § 5.3 (ray tests), § 5.5 (moving objects).
- K. Fauerby, "Improved Collision Detection and Response" (2003) — the swept sphere vs triangle used by collide and slide.
- `src/Math/Space3D/Intersections/`.
