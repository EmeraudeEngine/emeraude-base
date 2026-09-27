## Math/Space3D: Capsule Primitive

### Definition
A **Capsule** (also called Stadium solid) is a swept sphere: a line segment with a radius. It consists of a cylindrical body capped by two hemispheres.

```
    ___
   /   \      ← hemisphere (radius r)
  |     |
  |     |     ← cylinder (height h, radius r)
  |     |
   \___/      ← hemisphere (radius r)
```

### Internal Representation
- `Segment<precision_t> m_axis` - Central axis (start and end points)
- `precision_t m_radius` - Radius of the capsule

### Key Design Decisions

**Degenerate capsules (zero-length axis) are VALID:**
- A capsule with `startPoint == endPoint` behaves as a sphere
- `isValid()` only checks `radius > 0`
- Use `isDegenerate()` to detect zero-length axis

**Code references:**
- `Math/Space3D/Capsule.hpp:isValid()` - Returns true if radius > 0
- `Math/Space3D/Capsule.hpp:isDegenerate()` - Returns true if axis length ≈ 0
- `Math/Space3D/Capsule.hpp:closestPointOnAxis()` - Critical for collision detection

### Collision Convention (MTV)
**MTV (Minimum Translation Vector) pushes the FIRST argument out of the SECOND.**

```cpp
// 4 overloads per collision pair:
isColliding(Capsule, X)           // without MTV
isColliding(Capsule, X, mtv)      // MTV pushes Capsule out of X
isColliding(X, Capsule)           // without MTV
isColliding(X, Capsule, mtv)      // MTV pushes X out of Capsule
```

### Usage Example
```cpp
// Create a capsule (vertical, height 10, radius 2)
Capsule<float> capsule{{0, 0, 0}, {0, 10, 0}, 2.0f};

// Check collision with sphere
Sphere<float> sphere{1.5f, {3, 5, 0}};
Vector<3, float> mtv;
if (isColliding(capsule, sphere, mtv)) {
    // mtv contains vector to push capsule out of sphere
}

// Raycast against capsule
Line<float> ray{{-10, 5, 0}, {1, 0, 0}};
Point<float> hit;
if (isIntersecting(ray, capsule, hit)) {
    // hit contains first intersection point
}
```

### Available Collision Tests
| Pair | File | Notes |
|------|------|-------|
| Capsule-Capsule | `SamePrimitive.hpp` | Segment-segment distance |
| Capsule-Point | `CapsulePoint.hpp` | Point-to-axis distance |
| Capsule-Sphere | `CapsuleSphere.hpp` | Closest point on axis |
| Capsule-Cuboid | `CapsuleCuboid.hpp` | Iterative refinement |
| Capsule-Triangle | `CapsuleTriangle.hpp` | Terrain mesh collision |
| Line-Capsule | `LineCapsule.hpp` | Raycasting |
| Segment-Capsule | `SegmentCapsule.hpp` | Finite ray intersection |

### Unit Tests
All capsule functionality is tested in `Testing/test_MathSpace3D.cpp`:
- Primitive tests (constructor, isValid, isDegenerate, centroid, volume)
- Collision tests (all pairs, with/without MTV)
- Intersection tests (Line, Segment)
- Edge cases (degenerate capsules, touching/not touching)
