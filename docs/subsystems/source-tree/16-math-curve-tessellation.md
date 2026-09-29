## Math: adaptive curve tessellation (`Math/CurveTessellation.hpp`, 2026-09-29)

Header-only, `namespace Math::CurveTessellation`, templated on the floating point type. Written for the engine's
`Scenes::Component::Path` (owner decisions 2026-09-29: every curve kind tessellated on the CPU into a polyline,
adaptively, by a CHORD tolerance in the curve's units — 1 cm by default for metres).

**One mechanism for every kind.** Each curve is converted into CUBIC BÉZIER spans; `appendCubic()` splits a span by
de Casteljau at t = 1/2 until its two inner control points lie within the tolerance of the chord (distance to the
SEGMENT, not the infinite line: a control point beyond an end must still count). The curve lies in the convex hull of
its control points, so the polyline never strays further than the tolerance from the curve. A straight span is ONE
segment. `MaxDepth` (16) only guards a degenerate span.

| Function | Input | Conversion |
|---|---|---|
| `polyline()` | points | as they are |
| `bezierPath()` | the existing `Math::BSpline` (anchors, handles as OFFSETS from the anchor, a `CurveType` per span) | None = straight; BezierQuadratic = degree elevation (`c ± 2/3 (h − c)`); BezierCubic = (P0, P0 + out, P3 + in, P3). Its segment counts are IGNORED |
| `uniformBSpline()` | control points, open or closed | span (Q0..Q3) → ((Q0 + 4Q1 + Q2)/6, (2Q1 + Q2)/3, (Q1 + 2Q2)/3, (Q1 + 4Q2 + Q3)/6); open curves TRIPLE their end points (clamped to them), closed ones wrap |
| `catmullRom()` | points it passes through, `alpha` (0 uniform, **0.5 centripetal**, 1 chordal), open or closed | Barry-Goldman tangents rescaled to the span: m1 = (P2 − P1) + d12 ((P1 − P0)/d01 − (P2 − P0)/(d01 + d12)), m2 likewise, Bézier (P1, P1 + m1/3, P2 − m2/3, P2); open ends get MIRRORED phantom points |

Consecutive coincident points are dropped first (`withoutDuplicates()`): a zero knot interval would divide by zero.

References: P. Barry, R. Goldman, "A recursive evaluation algorithm for a class of Catmull-Rom splines", SIGGRAPH 1988;
C. Yuksel, S. Schaefer, J. Keyser, "Parameterization and applications of Catmull-Rom curves", CAD 43(7), 2011
(centripetal: no cusp nor self-intersection within a span).

**Tests** (`test_MathCurveTessellation.cpp`, 8) compare against INDEPENDENT evaluations — the Bernstein form of the
cubic, the uniform B-spline BASIS functions — never against the conversion under test. Mutation-checked
(2026-09-29): a wrong B-spline coefficient fails `aUniformBSplineMatchesItsBasis`; a flatness test 1000× too lax
fails 6 of the 8.

`Math::BSpline` gained `points()` for this (it had no read access to its points). Its NAME is historical: it is a
piecewise Bézier path, not a B-spline — the uniform B-spline is `CurveTessellation::uniformBSpline()`.
