## Math/Space3D: convex distance (GJK) — physics overhaul P5, 2026-10-02

`src/Math/Space3D/Casts/ConvexDistance.hpp` (header-only): `ConvexPolytope< T >` (a point, a segment, a triangle or a
box — at most 8 vertices, a `StaticVector`) and `closestPoints(A, B)` → `ClosestPoints< T >` {`onA`, `onB`, `distance`,
`overlapping`}: the Gilbert-Johnson-Keerthi distance (1988), the simplex's closest points by its Voronoi regions
(C. Ericson, "Real-Time Collision Detection", § 9.5 and § 5.1.5-5.1.6). Rounded shapes are a core polytope plus a radius:
the caller subtracts the radii. What `castBox()` advances on (`18-math-space3d-casts.md`). No third-party code.

### Robustness (each one a defect found by the tests, 2026-10-02)
- The convergence is relative to a HUNDRED ULPS of the precision (`epsilon() × 100`): 1e-6 was out of a float's reach,
  GJK went on, added a nearly coplanar support point and left the minimum (0.569 instead of 0.113 on a segment ↔ box).
- An iteration that does not bring the simplex closer to the origin is undone and ends the search (Ericson § 9.5).
- A tetrahedron whose height over a face is under 1e-4 of its size is FLAT: every face is a candidate instead of
  trusting sign tests that are noise — a flat one answered "inside" (overlapping) 0.1 m from the origin.
- Deterministic: the supports break ties on the first vertex; at most 64 iterations.

### Tests (`src/Testing/test_MathSpace3DConvexDistance.cpp`, 15, Release and ASan/UBSan green)
Point ↔ point; separated boxes; a turned box's edge over the ground; overlap; an empty polytope refused; segment ↔ box
and segment ↔ triangle against the exact closest points (400 random cases each); box ↔ box overlap against the
separating axis test (3000 random pairs); and the box casts (below).
