---
id: bspline-zero-segments
title: BSpline constructors accept 0 segments, which divides by zero
status: open
priority: high
scope: src/Math/BSpline.hpp
opened: 2026-10-08
tags: [ave-robustus-ii, math, defect]
---

# BSpline constructors accept 0 segments, which divides by zero

## Why
`BSpline` / `BSplinePoint` constructors accept 0 segments while `setSegments()` / `setDefaultSegments()` refuse it; with 0
segments `synthesize()` divides by zero (infinite times).

## What remains
- The constructors apply the setters' rule (owner: refuse → which default? or clamp to 1); test.

## References
- Found by the Ave Robustus II warning pass (2026-10-08, projet-alpha `docs/plans/ave-robustus-ii.md`); not raised by a warning, so left for its own fix.
