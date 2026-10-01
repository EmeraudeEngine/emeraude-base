---
id: rigid-body-math-helpers
title: Rigid-body math helpers — inertia tensors of the primitives, orientation integration
status: in-progress
priority: unranked
scope: Math (Matrix, Quaternion, CartesianFrame)
opened: 2026-10-01
tags: [physics, math, physics-overhaul]
---

# Rigid-body math helpers — inertia tensors of the primitives, orientation integration

## Why

Phase P1 of the physics overhaul (engine `docs/physics-overhaul.md`), needed by phase P3 (engine `rotational-physics`, closed 2026-10-02).
The engine's inertia tensor defaults to the identity whatever the mass and the shape, and nothing computes it from a
shape. The orientation is integrated as one angle-axis rotation per tick, through `CartesianFrame` (two stored
vectors), with every quaternion round trip going through a 4×4 matrix.

## Done (2026-10-01)

- `src/Math/RigidBody.hpp`: solid box / sphere / cylinder / capsule inertia (refusing invalid inputs), the parallel-axis
  theorem, the skew matrix, `integrateOrientation()` (world ω, exp-map, renormalised); `Quaternion::setFromScaledAxis()`
  no longer narrows through double. 8 tests, Release and ASan/UBSan green. Doc:
  `docs/subsystems/source-tree/19-math-rigid-body.md`.

## What remains

- [ ] The sum of several tensors for a compound body (each moved by `parallelAxis()` first). ⚠️ P3 (2026-10-02) did NOT
  need it: an entity with several massive components derives its tensor from its ONE collision shape and its total
  mass (engine `docs/subsystems/physics/16-rigid-body-rotation.md`). Open only for a future compound-shape body.
- [ ] A `Quaternion` from a `Matrix< 3 >` (today only from a `Matrix< 4 >`), when the engine integration needs it — P3
  did not (the scene step reads `CartesianFrame::toQuaternion()`).

## References

- Any rigid-body text, e.g. D. Baraff, "An Introduction to Physically Based Modeling: Rigid Body Simulation" (SIGGRAPH
  1997 course notes).
- `src/Math/Quaternion.hpp:1187`, `src/Math/Matrix.hpp`, `src/Math/CartesianFrame.hpp`.
