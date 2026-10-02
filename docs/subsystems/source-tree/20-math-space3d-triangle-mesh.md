## Math/Space3D: TriangleMesh — static triangle meshes for collision (physics overhaul P5, 2026-10-02)

`src/Math/Space3D/TriangleMesh.hpp` (header-only, `TriangleMesh< T >`): the triangles of a static mesh, their unit
winding normals, the ACTIVE EDGES of each one, and a bounding-volume hierarchy built once. The engine's
`Physics::TriangleMeshCollisionModel` wraps it (engine `docs/subsystems/physics/18-triangle-mesh-statics.md`).

### `build(vertices, indices, activeEdgeCosine)`
- Refuses (false, the mesh left empty): no index, a count not a multiple of 3, an index out of the vertices, a
  non-finite vertex, a cosine outside [-1, 1] or not finite, or no triangle left once the degenerate ones are skipped.
- **Welds** the vertices of the same position first (exact components — `Vector::operator==` has a tolerance): an
  exported mesh splits a vertex by normal or by texture coordinate, and its shared edges would otherwise look like
  borders.
- The front face is the winding normal `(B - A) × (C - A)` (counter-clockwise).

### The hierarchy
- A binary AABB tree, the surface area heuristic over 12 bins of the centroids on the longest centroid axis (I. Wald,
  "On fast Construction of SAH-based Bounding Volume Hierarchies", 2007); the median when no bin boundary separates
  the range (all centroids in one point) or when no split is cheaper; at most `MaxLeafTriangles` = 4 per leaf, a leaf
  forced at `MaxDepth` = 48.
- Built iteratively (a right range waits on a stack while the left one is built next), stored depth first: a node's
  left child FOLLOWS it, `right` names the other. The triangles are stored in the leaves' order (`first`, `count`).
- `visit(minimum, maximum, function)`: every triangle whose bounds overlap the box (borders included), from a fixed
  stack of `MaxDepth + 2` (no allocation). Its own bounds are min / max vectors, not `AACuboid`: a flat mesh has a box of
  zero thickness, which `AACuboid::isValid()` refuses.

### Active edges (the idea of Jolt's `MeshShape`, J. Rouwé, MIT — no code taken)
- Bits `EdgeABActive` (1), `EdgeBCActive` (2), `EdgeCAActive` (4). An edge shared by EXACTLY two triangles is INACTIVE
  when flat (the normals' cosine ≥ the threshold — cos 5° is Jolt's default) or CONCAVE (the far vertex of the other
  triangle stands in front of this one's plane); ACTIVE when convex beyond the threshold. A border, a non-manifold edge
  (3 triangles or more) and a knife edge (opposite normals, cosine < −0.999) are active.
- `correctInternalEdgeNormal(...)` (three overloads: by index, for a placed copy of a triangle, for one sweep normal at
  a point): when every contact point lies on an inactive feature — an inactive edge, a vertex whose two edges are
  inactive, or the face — and the normal leans away from the face's, the normal becomes the face normal on the body's
  side and each depth is projected on it (× the cosine). A body sliding across two coplanar triangles then never meets
  the edge they share.

### Tests (`src/Testing/test_MathSpace3DTriangleMesh.cpp`, 13, Release and ASan/UBSan green)
Refusals; degenerate triangles skipped; `visit()` = brute force on 600 random triangles × 300 random boxes; hierarchy
invariants on 1000 triangles (every triangle in exactly one leaf, leaves ≤ 4, children inside their parent); 200
identical triangles stay bounded; a flat quad's diagonal inactive and its borders active; split vertices welded;
convex edge active, concave inactive; knife edge active; the three correction cases.
