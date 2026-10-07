## Math/Space3D: linear shape casts (physics overhaul P1, 2026-10-01)

`src/Math/Space3D/Casts/ShapeCast.hpp` — the first contact of a ray, a sphere or a capsule moved by a displacement
against an `OrientedBox`, a `Triangle`, a `Sphere` or a `Capsule`. What the engine's kinematic character controller
sweeps its capsule with (collide and slide) and what a ground probe casts (engine item `kinematic-character-controller`).

### API
- `castRay(origin, motion, target, hit)`, `castSphere(sphere, motion, target, hit)`, `castCapsule(capsule, motion,
  target, hit)` → `bool` (a contact within the motion) and a `CastHit`:
  - `fraction()` in [0, 1] of the motion travelled (the ray covers origin → origin + motion);
  - `point()` on the TARGET's surface;
  - `normal()` the target's surface normal there, pointing BACK TOWARDS THE CASTER (unit);
  - `startedInside()` the caster already penetrated (or, for a ray, started in a box / on a triangle) — fraction 0,
    the normal then separates it (back along the motion when the cores overlap).
- A capsule is cast without rotation.

### Method — conservative advancement on the exact closest points
The caster is a point (ray, sphere) or a segment (capsule) with a radius; each step computes the EXACT core distance to
the target (`CapsuleBoxDetail::closestOfSegmentAndBox()`, `CapsuleTriangleDetail::closestOfSegmentAndTriangle()`,
segment ↔ point, segment ↔ segment), the unit direction n towards the target, and advances by d / (motion · n). For a
translating convex shape this NEVER overshoots — the target lies beyond the separating plane of normal n — and converges
on the first contact (G. van den Bergen 2004, "Ray Casting against General Convex Objects…"; E. Catto, "Continuous
Collision", GDC 2013). A contact is declared within 0.1 mm; 64 steps at most (a grazing pass that does not converge is
a miss). No third-party code.

### Rules worth knowing
- Touching at the start and moving AWAY (a character leaving the ground) is NOT a contact; touching and moving into the
  surface is a contact at fraction 0; penetrating at the start is always reported (`startedInside()`).
- A degenerate (collinear) triangle is never hit. Triangles are two-sided (as the contacts).
- Not covered yet: casting a box (a moving box), rotating casts.

### Tests (`src/Testing/test_MathSpace3DCasts.cpp`, 13)
Ray down onto the ground (fraction 0.5, point, normal), misses (parallel, too short, away), started inside; sphere
down onto the ground; touching-and-leaving vs touching-and-pushing; a standing capsule down; a walking capsule into a
citadel step (0.29 m) hitting the step's TOP EDGE at the analytic fraction with the edge-to-centre normal; a sphere onto a
30° triangle (the normal points back up although the winding normal points down); sphere vs sphere / capsule; capsule
vs capsule; a degenerate triangle. Randomised (local LCG, 1000 draws each): sphere casts vs boxes and capsule casts vs
triangles are CONSERVATIVE (no contact sampled before the reported fraction) and EXACT (the gap at the fraction under
1 mm). Release and ASan/UBSan green.

### Box casts (physics overhaul P5, 2026-10-02, decision 14)
- `castBox(box, motion, target, hit)`: an `OrientedBox` TRANSLATED (not turned) against a box, a triangle, a sphere or a
  capsule — the same conservative advancement on the closest points `closestPoints()` gives (GJK,
  `21-math-space3d-convex-distance.md`) between the moved box and the target's core; exact for its faces, edges and
  corners. The same `CastHit` contract.
- Within 1 mm of contact, approached to 0.5 mm: the direction between two closest points a few hundredths of a
  millimetre apart is noise in float. GJK runs about the box's start (coordinates of the order of the shapes).
- A normal within 0.01 rad of a face normal — the target's, else the box's own facing it — becomes that normal EXACTLY:
  face against face has no unique pair of closest points, the GJK normal leaned 3e-4 rad (0.0024 before the recentring)
  and a box bounced off a wall drifted over it.
- In exact contact at the start GJK has no direction: a box that 1 mm along its motion is apart is leaving (no contact).
- Tests in `test_MathSpace3DConvexDistance.cpp`: onto the ground (fraction 0.5), a box turned 45° meets with its edge
  (fraction (2 − √2 / 2) / 3), onto a triangle, a sphere and a capsule, started inside, leaving, the face normal EXACT 89 m
  from the origin, and 120 random turned boxes against a 1/2000 march (2e-3).
- A cast that STARTS touching (fraction 0) is the normal case of a character standing on the ground: it must answer a
  usable normal, never a degenerate one; and no float division by a possibly-zero length.
- `Space3D/Intersections/` `Line*` tests take an INFINITE line: they may answer a hit behind its origin, which is right
  for a line. A caller that means a RAY (forward only) uses `castRay` (owner, 2026-10-07).
- Left for when a consumer needs it: casting a moving `OrientedBox` (item `physics-p1-leftovers`).
