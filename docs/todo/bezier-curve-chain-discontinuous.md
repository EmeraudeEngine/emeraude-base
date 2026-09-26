---
id: bezier-curve-chain-discontinuous
title: BezierCurve chains overlapping quadratic segments — the curve jumps at every segment boundary and never closes
status: open
priority: unranked
scope: src/Math/BezierCurve.hpp (synthesizePoint, segment selection); callers in projet-alpha (basic-scenery, game-logic, particles)
opened: 2026-09-26
tags: [math, animation, correctness, found-by-audit]
---

# BezierCurve chains overlapping quadratic segments — the curve jumps at every segment boundary and never closes

## Why

`BezierCurve::synthesizePoint()` (`src/Math/BezierCurve.hpp` ~153-198) evaluates segment `i` as the quadratic
`(P[i], P[i+1], P[i+2])`, with `i = floor(t * numSegments)`, `numSegments = N - 2` open / `N` closed — the segments
overlap with a step of ONE point (the comment at `BezierCurve.hpp:180-182` says "for simplicity", and describes a
midpoint scheme the code does not use). Segment `i` ends at `P[i+2]` while
segment `i + 1` starts at `P[i+1]`: the curve is not C0. Measured by the lerp audit (2026-09-26, with the corrected
`Vector::linearInterpolation`): basic-scenery's 5-point open curve jumps |P2 - P1| ≈ 2 646 units at t = 1/3 and
|P3 - P2| ≈ 2 236 at t = 2/3 (3 780 / 3 937 before the lerp fix);
a closed curve does not close (5-point wrap gap 141 units). No test covers `BezierCurve` (`ave-robustus-inventory` notes
"0 tests").

Consumers: projet-alpha `basic-scenery`'s White flying light (5 points, open), `game-logic`'s moving smoke (5 points,
closed — the duplicated closing point plus `close()` also close it twice), `particles`' spiral blower (6 points, closed).

## What remains

1. Pick the chaining scheme (owner/engine decision): midpoint-chained quadratics (segment i = (mid(P[i], P[i+1]),
   P[i+1], mid(P[i+1], P[i+2])), C1, the usual "smooth polyline through a control polygon"), or segments stepping by
   TWO points (passes through every other point), or a Catmull-Rom / cubic path through all the points.
2. Add tests: C0 at every boundary, `t = 0` / `t = 1` at the curve's ends, a closed curve closes.
3. Re-check the three consumers' look (their paths change).

## References

- emeraude-base `docs/caution-points.md` § *Fixed: `Vector::linearInterpolation()` ran BACKWARDS*.
