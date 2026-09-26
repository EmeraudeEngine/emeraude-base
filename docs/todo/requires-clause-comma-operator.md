---
id: requires-clause-comma-operator
title: Seventeen requires-clauses are COMMA expressions — the first half of each constraint is ignored
status: open
priority: unranked
scope: src/FlagTrait.hpp, src/Math/Base.hpp, src/PixelFactory/{Color,Gradient,Margin,Pixmap}.hpp, src/WaveFactory/Wave.hpp
opened: 2026-09-25
tags: [concepts, templates, correctness, found-by-audit]
---

# Seventeen requires-clauses are COMMA expressions — the first half of each constraint is ignored

## Why

`requires (std::is_arithmetic_v< number_t >, std::is_floating_point_v< scale_number_t >)` is a parenthesised COMMA
expression: its value is the second operand alone, so `is_arithmetic_v< number_t >` is evaluated and DISCARDED. The
intended conjunction is `&&`. Found on 2026-09-25 while fixing `BSpline::synthesize()`: its unqualified
`linearInterpolation(Vector, Vector, float)` compiled only because `Math/Base.hpp:374`'s SCALAR lerp accepts a
`Vector` through this hole.

The sites (grep `requires (std::is_[a-z_]*< [a-z_]* >, std::` in `src/`): `FlagTrait.hpp:41`; `Math/Base.hpp:375, 394,
417, 478, 511, 577`; `PixelFactory/Color.hpp:1691, 1720, 1740, 1761, 1781, 1802`; `PixelFactory/Gradient.hpp:44`;
`PixelFactory/Margin.hpp:50`; `WaveFactory/Wave.hpp:435`; `PixelFactory/Pixmap.hpp:3094`.

## What remains

1. Replace each comma by `&&` (or split the constraints), one header at a time, building the whole cascade and the
   unit suite after each: every compile error names a caller that relied on the hole.
2. Known callers that rely on it today — they pass VECTORS and MATRICES to the scalar `Math::linearInterpolation()`:
   the engine's `src/Animations/Sequence.cpp:151-186` (the Vector2/3/4 and Matrix2/3/4 `Variant` lerps — the animated
   `WorldPosition` of `basic-scenery`'s flying lights goes through it; lines 118-143 are legitimate scalars), and this
   repository's own `src/VertexFactory/Grid.hpp:991-995` (normals) and `src/Math/CartesianFrame.hpp:1301-1304`, which
   will break its build first; probably others. Give them an explicit lerp; keep the direction `A + (B - A) * t`.
3. Add a compile-time test (`static_assert(!requires { Math::linearInterpolation(Vector< 3, float >{}, …); })` style)
   for each fixed constraint, so a comma can never come back unnoticed.

## ⚠️ Traps

- `Vector::linearInterpolation(a, b, t)` ran BACKWARDS until 2026-09-26 (factor 0 gave b; `docs/caution-points.md`
  § *Fixed: `Vector::linearInterpolation()` ran BACKWARDS*). It now follows the scalar convention, so it is the safe
  replacement for the VECTOR callers of the hole; `Matrix` has no member lerp.
- A constraint that is too wide never fails to compile, it silently accepts types it was written to refuse: the
  build is green before AND after the bug. Only a negative test proves a constraint.

## References

- `src/Math/BSpline.hpp` (the explicit lerp that replaced the hole-relying call, 2026-09-25).
- `docs/caution-points.md` § *A `requires (A, B)` is a comma expression*.
