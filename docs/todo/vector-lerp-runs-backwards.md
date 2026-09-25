---
id: vector-lerp-runs-backwards
title: Vector::linearInterpolation runs backwards — every Bezier segment and three actors' smoothing with it
status: open
priority: unranked
scope: src/Math/Vector.hpp (linearInterpolation, quadraticBezierInterpolation, cubicBezierInterpolation); callers in BSpline, BezierCurve, projet-alpha actors
opened: 2026-09-25
tags: [math, animation, correctness, found-by-review, owner-decision]
---

# Vector::linearInterpolation runs backwards — every Bezier segment and three actors' smoothing with it

## Why

`Vector::linearInterpolation(a, b, t)` (`src/Math/Vector.hpp:1915`) returns `a * t + b * (1 - t)`: factor 0 gives
**b**, factor 1 gives **a** — the OPPOSITE of the scalar `Math::linearInterpolation()` (`Math/Base.hpp:374`,
`a + (b - a) * t`) and of every caller's intent. Found by the review of the `BSpline` fix (2026-09-25), confirmed by
algebra and by a scratch build:

- `quadraticBezierInterpolation(A, B, C, t)` = L(L(A, B, t), L(B, C, t), t) returns **C at t = 0** and A at t = 1
  (`Vector.hpp:1933-1935`); `cubicBezierInterpolation` likewise (`:1954-1960`). `BSpline::synthesizeQuadratic/Cubic`
  (`BSpline.hpp` ~483, ~519) and `BezierCurve` (`BezierCurve.hpp` ~136, ~198) feed them t = 0 → 1, so every Bezier
  segment is drawn from the NEXT point back to the current one, then jumps forward: a sawtooth, with time still
  increasing. projet-alpha `basic-scenery`'s White (BezierCurve), Green (BezierQuadratic) and Blue (BezierCubic)
  flying lights fly that path; the Red one (CurveType::None, the explicit lerp) is correct.
- projet-alpha actors call it directly as "move `current` a fraction toward `desired`": `Paladin.cpp:504`
  (`m_turnSpeed` 0.015), `Fox.cpp:220` (0.01), `Drone.cpp:206` (`m_lookSpeed` 0.1). With the reversed lerp they get
  98.5 % / 99 % / 90 % of the target per update — they SNAP to it instead of turning smoothly.

## What remains (owner decision first: it changes what two demos show)

1. Make `Vector::linearInterpolation` return `a + (b - a) * t` (the convention every caller assumes).
2. Re-check the three actors' feel: their speeds were tuned (or never noticed) under the snap; they may need new
   values once the smoothing works.
3. Re-check `basic-scenery`'s flying paths (they become the curves their control points describe).
4. Extend `MathBSpline.lastPointIsTheTerminalSampleForEveryCurveType` (emeraude-base `src/Testing/test_MathBSpline.cpp`)
   with the FIRST sample and every point boundary (`samples[k * Segments].position == point k`), and add a direct
   `Vector::linearInterpolation` test (t = 0 → a). Both fail today for the Bezier types — that is the proof.

## ⚠️ Traps

- ⚠️ `Color::linearInterpolation` and `CartesianFrame::linearInterpolation` are separate functions: check their own
  direction before assuming anything (the engine's `Sequence.cpp:193, 200` uses them).
- Related: `requires-clause-comma-operator` — the scalar lerp accepts vectors only through a comma-expression hole;
  when that hole is closed, a caller swapped to `Vector::linearInterpolation` must use the fixed direction.
