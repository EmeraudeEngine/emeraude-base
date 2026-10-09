---
id: vector-normalized-absolute-epsilon
title: Vector::normalize() / normalized() refuse every vector shorter than ~3.5e-4 (an absolute epsilon)
status: open
priority: high
scope: src/Math/Vector.hpp (and the 243 call sites of the cascade)
opened: 2026-10-09
tags: [math, defect, vertexfactory]
---

# Vector::normalize() / normalized() refuse every vector shorter than ~3.5e-4 (an absolute epsilon)

## Why
`normalized()` returns a ZERO vector, and `normalize()` leaves the vector UNCHANGED (inconsistent), when
`Utility::isZero(lengthSquared())` — `|lengthSquared| <= std::numeric_limits< precision_t >::epsilon()`: an ABSOLUTE
tolerance (1.19e-7 for float), so every valid vector shorter than ~3.45e-4 is refused. Found by the D7 rewrite
(2026-10-09): `ShapeDecimator::computeInitialQuadrics()` normalizes a triangle's cross product (2 × its area): every
triangle under ~1.7e-4 of area got a zero normal and a zero quadric. On a 400 × 200 unit sphere, 0 of 80 487 vertices
had a non-zero quadric and 99.9 % of the collapse costs were exactly 0: the QEM was disabled on every fine mesh (the
engine's automatic LODs of large meshes), and the queue's tie-break chose the collapses.

## What remains (owner decision 2026-10-09: fix at the ROOT, in Vector)
- `normalize()` / `normalized()` refuse only a zero, non-finite or denormal length (state the contract; consistent
  behaviour between the two); tests with tiny valid vectors.
- Audit the 243 call sites (base 190, engine 44, projet-alpha 9): find those that relied on "short = zero" (a
  direction between two nearly identical points…) and give them an explicit, scale-aware test.
- Then re-measure the decimator (quality, timing, the three OS' goldens) — item `task-handle-and-stop-token`.

## References
- `src/Math/Vector.hpp` `normalize()` (floating point), `normalized()`; `src/BaseUtility.hpp` `isZero()`.
