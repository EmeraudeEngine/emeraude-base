---
id: rigid-body-math-helpers
title: Rigid-body math helpers — inertia tensors of the primitives, orientation integration
status: open
priority: unranked
scope: Math (Matrix, Quaternion, CartesianFrame)
opened: 2026-10-01
tags: [physics, math, physics-overhaul]
---

# Rigid-body math helpers — inertia tensors of the primitives, orientation integration

## Why

Phase P1 of the physics overhaul (engine `docs/physics-overhaul.md`), needed by phase P3 (engine `rotational-physics`).
The engine's inertia tensor defaults to the identity whatever the mass and the shape, and nothing computes it from a
shape. The orientation is integrated as one angle-axis rotation per tick, through `CartesianFrame` (two stored
vectors), with every quaternion round trip going through a 4×4 matrix.

## What remains

- [ ] Inertia tensors (about the centre of mass, local frame) of a solid box, sphere, capsule (cylinder + two
  hemispheres), cylinder, from the mass and the dimensions; the parallel-axis theorem to move a tensor; the sum of
  several (a compound shape).
- [ ] The cross-product (skew-symmetric) matrix of a vector.
- [ ] Orientation integration from a world angular velocity over dt (exact exp-map, `Quaternion::setFromScaledAxis()`
  already exists) with renormalisation; a `Quaternion` from a `Matrix< 3 >`.
- [ ] Unit tests against the closed forms (box `m(h² + d²)/12`…), and an integration test: a torque-free body
  spinning about a principal axis keeps its axis and its rate over 10 000 steps.

## References

- Any rigid-body text, e.g. D. Baraff, "An Introduction to Physically Based Modeling: Rigid Body Simulation" (SIGGRAPH
  1997 course notes).
- `src/Math/Quaternion.hpp:1187`, `src/Math/Matrix.hpp`, `src/Math/CartesianFrame.hpp`.
