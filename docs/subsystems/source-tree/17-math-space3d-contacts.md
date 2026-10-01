## Math/Space3D: OrientedBox and contact manifolds (physics overhaul P1, 2026-10-01)

Phase P1 of the engine's physics overhaul (engine `docs/physics-overhaul.md`; owner decisions § 1.5). The overlap tests
of `Space3D/Collisions/` answer ONE minimum translation vector; a rigid-body solver needs up to 4 contact points to keep
a box resting on a face. Open work: `docs/todo/contact-manifold-generation.md`.

### `Space3D::OrientedBox< T >` (`src/Math/Space3D/OrientedBox.hpp`)
- Centre, three orthonormal axes (`std::array` of `Vector< 3 >`), half extents. `fromCuboid(localBox, frame)` places a
  local axis-aligned box with a `CartesianFrame` (its scaling multiplies the half extents, its axes become the box axes).
- `isValid()`: finite values, non-negative half extents, unit axes (1e-3). Zero half extents are VALID (a flat box).
- `projectedRadius(direction)` = Σ e_i |a_i · d|, `corner(index)` (bit 0 = +X, bit 1 = +Y, bit 2 = +Z).
- `Math::OrientedCuboid` (8 corners, 6 normals, built from a model matrix) stays for its current uses.

### `ContactPoint< T >` / `ContactManifold< T >` (`src/Math/Space3D/Contacts/ContactManifold.hpp`)
- ⚠️ **The normal points FROM A TO B** — the opposite of the overlap tests' MTV, which pushes A OUT of B. Moving A by
  `-normal * depth` (or B by `+normal * depth`) separates a point. `flip()` swaps the roles (feature ids unchanged).
- Up to `MaxPoints` = 4 points (`StaticVector`), each with its position HALFWAY between the two surfaces, its own
  depth (positive = penetrating) and a feature id that stays the same while the same features touch — what a solver
  matches to warm-start its accumulated impulses. `addPoint()` refuses a fifth point (`false`).

### Box ↔ box (`src/Math/Space3D/Contacts/BoxBox.hpp`, `computeContactManifold(boxA, boxB, manifold)`)
- SAT on the 15 axes (3 + 3 faces, 9 edge cross products; a cross product under 1e-6² is skipped as parallel).
- The axis choice is BIASED towards faces, then towards A's faces (Gregorius GDC 2013): an edge wins only when its
  separation exceeds `0.95 × best face + 1e-4`, a face of B only when it exceeds `0.98 × face of A + 1e-4`. Without the
  bias the chosen axis flickers between two nearly equal ones from one step to the next.
- Face contact: the incident face (the other box's face most anti-parallel to the reference normal) is clipped by
  the 4 side planes of the reference face (Sutherland-Hodgman; a quad gains at most one vertex per plane, so ≤ 8), the
  points above the reference face are dropped, the rest reduced to 4: the deepest, the farthest from it, and the
  largest triangles on each side of that segment (ties break on the feature id → independent of the clipping order).
- Edge contact: one point between the closest points of the two support edges (Ericson § 5.1.8).
- Feature ids: bit 31 = edge ↔ edge (edge of A << 8 | edge of B | sign bits << 16); otherwise bit 30 = B holds the
  reference face, bits 24-27 the reference face (axis × 2 + side), 16-19 the incident face, 0-15 the clipped point
  (an incident corner 0-3, or `0x100 | clip plane << 4 | edge tag`).
- Returns `false` and an empty manifold when separated; a grazing contact whose clipping keeps no point returns
  `false` too.
- No third-party code: the algorithms follow the cited papers / book.

### Sphere ↔ box (`Contacts/SphereBox.hpp`) — one point
- The centre clamped in the box frame (Ericson § 5.1.4); normal from the sphere to the box; centre inside (or within
  1e-6 of the surface) → the face of least penetration, depth = radius + distance to that face.
- Feature id: the box region of the centre in base 3 per axis (`ContactsDetail::regionDigit()`: 0 inside the slab, 1
  under, 2 above — a face, an edge or a corner); `0x100 | face` when the centre is inside.

### Capsule ↔ box (`Contacts/CapsuleBox.hpp`) — one or two points
- The EXACT closest points of the capsule's segment and the box: along the segment the squared distance is
  Σ max(|α_i + β_i t| - e_i, 0)², a convex piecewise quadratic with at most 6 breakpoints (where |α_i + β_i t| = e_i);
  each piece is minimised in closed form. It replaces, for this pair, the 4-iteration alternating projection of
  `Collisions/CapsuleCuboid.hpp` (item `collision-pair-test-defects`).
- Shallow (segment outside the box, nearer than the radius): one point; TWO when the closest feature is a face and the
  segment lies along it (within sin = 0.05, ~3°): the segment clipped to the face's prism — a capsule lying on a floor
  does not rock on one point.
- Deep (the segment touches or crosses the box): SAT on the 3 face axes and the 3 cross products segment × box axis,
  biased towards faces (0.95 × + 1e-4); a face clips the segment (≤ 2 points), an edge gives the point between the
  segment and that box edge.
- A zero-length capsule is a sphere (same depth and point as sphere ↔ box: tested).
- Feature ids: `0x1000 | face << 4 | end` (shallow along a face), `0x2000 | region` (shallow single point),
  `0x4000 | face << 4 | end` (deep face), `0x8000 | box axis` (deep edge).
- Each pair has its reversed overload (box first): the same manifold, `flip()`ped.

### Triangles (`Contacts/SphereTriangle.hpp`, `Contacts/CapsuleTriangle.hpp`) — TWO-SIDED
- `TriangleDetail::closestPointOnTriangle()`: the exact closest point by Voronoi regions (Ericson § 5.1.5), with the
  region (`TriangleDetail::Region`: vertices 1-3, edges AB 4 / AC 5 / BC 6, face 7) used in the feature ids. A
  collinear triangle (`unitNormal()` false) gives no contact — `Triangle::isValid()` accepts one.
- Sphere: one point; a centre ON the triangle leaves along the winding normal (B - A) × (C - A).
- Capsule: the exact segment ↔ triangle closest points (the segment piercing it, else its ends against the triangle
  and the segment against the 3 edges); shallow → one point, or two when the segment lies along the face (normal
  within ~2.6° of the face normal — a TIE between the face and an edge at the same distance must not hide it);
  deep (touching / piercing) → pushed along the face normal TOWARDS THE SIDE OF THE CAPSULE'S CENTRE, the segment
  clipped to the triangle's prism, each end's depth from the plane. The overlap test `Collisions/CapsuleTriangle.hpp`
  pushed a piercing capsule by its radius whatever its side and depth (item `collision-pair-test-defects`).
- A one-sided mode (back faces ignored, Jolt's `EBackFaceMode`) is for the triangle meshes of P5: owner decision then.

### Round shapes (`Contacts/RoundShapes.hpp`) — sphere ↔ sphere, sphere ↔ capsule, capsule ↔ capsule
- Each reduces to the closest points of a point or a segment, then two spheres. Two parallel capsules (within
  ~2.6°) side by side get two points, the ends of their overlap.
- ⚠️ Coincident centres / crossing axes leave the direction undefined: crossing axes take their common perpendicular
  (from A's centre towards B's), coincident centres or coincident parallel axes take +Y — owner decision 2026-10-01:
  a fixed, deterministic answer ("I prefer determinism"), not a caller-supplied direction.
- Feature ids: sphere ↔ capsule = the capsule region (1 start cap, 2 end cap, 3 cylinder); capsule ↔ capsule =
  region A << 4 | region B, or 0x100 | end for the parallel pair.

### Tests (`src/Testing/test_MathSpace3DContacts.cpp`, 50)
Flat (4 points, depth and positions exact), swapped A/B (normal negated), on an edge (2), on a corner (1), yawed 45°
(the 4 corners), the 45° stacked cubes (octagon → 4 spanning points), feature ids stable under a 1 mm / 0.1° move,
crossed edges (1 edge point), deep penetration, a zero-thickness box, determinism, the manifold's capacity.
Sphere ↔ box: on the ground (and swapped), a corner, separated, centre inside, a rotated edge. Capsule ↔ box: lying (2
points), overhanging (clipped to x 4 … 5), standing, tilted, separated, crossing a cube (deep face, 2 points), across a
rotated edge (deep edge), degenerate = sphere, a rotated box vs a 20 000-sample exact distance.
Triangles: above / under the face (two-sided), a vertex and an edge region, special cases (collinear, centre on it),
a capsule lying (2), overhanging (clipped to the prism, x ±0.6), standing, piercing (pushed towards its centre, both
sides), across an edge, degenerate = sphere. Round shapes: two spheres (and coincident), sphere vs capsule cylinder and
cap, crossed capsules, parallel capsules (2 points, x 0 … 1), crossing axes.
Randomised properties (2000 draws each, a local LCG so the draws are identical on every platform): box pairs — moving B
along the normal by the deepest depth separates them; spheres and capsules — the depth equals radius minus the
densely sampled exact distance (1e-4 / 1e-3), no contact beyond the radius; spheres vs triangles against a 300 × 300
barycentric brute force (500 draws); capsules vs triangles; capsule pairs against a 300 × 300 brute force (500 draws).
Release and ASan/UBSan green.

### ⚠️ Traps
- Never `Vector / s` on a length that can be tiny (NaN under epsilon): multiply by `1 / s` after a `> threshold` test.
- A constexpr local used in a lambda is declared INSIDE the lambda (MSVC C3493 on an uncaptured one).
