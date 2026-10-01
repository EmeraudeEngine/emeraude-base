## Important Files

### Animation (skeletal data types)
- `Animation/Joint.hpp` - Joint struct (name, parentIndex, T/R/S, inverseBindMatrix)
- `Animation/Skeleton.hpp` - Ordered joint collection, name lookup, hierarchy validation
- `Animation/AnimationChannel.hpp` - Keyframes for one `targetIndex` (joint OR node), 3 interpolation modes, ChannelTarget enum, `sampleVector()`/`sampleQuaternion()`
- `Animation/AnimationClip.hpp` - Named channel collection, auto-computed duration
- `Animation/Skin.hpp` - Mesh-to-skeleton binding, joint remapping, inverse bind matrices, and the root transform above the root joints (`setRootTransform()`, the glTF mesh-node correction, 2026-10-01)

### Math (critical)
- `Vector.hpp` - 2D/3D/4D vectors
- `Matrix.hpp` - Transformation matrices
- `Quaternion.hpp` - 3D rotations (`toRotationMatrix4()` for standard column-major 4x4)
- `CartesianFrame.hpp` - Coordinate system with orthonormal basis (`fromQuaternion()`, `toQuaternion()`)
- `TransformUtils.hpp` - TRS decomposition/composition (`decomposeTRS()`, `composeTRS()`)
- `Bezier.hpp` - Bezier curves

### Math/Space3D Primitives
- `Point.hpp` - 3D point
- `Line.hpp` - Infinite line (origin + direction)
- `Segment.hpp` - Finite line segment (start + end)
- `Sphere.hpp` - Sphere (center + radius)
- `Capsule.hpp` - Capsule/Stadium solid (axis segment + radius)
- `Triangle.hpp` - Triangle (3 vertices)
- `AACuboid.hpp` - Axis-aligned bounding box

### Math/Space3D Collisions
- `Collisions/SamePrimitive.hpp` - Same-type collisions (Sphere-Sphere, Capsule-Capsule, etc.)
- `Collisions/CapsulePoint.hpp` - Capsule vs Point
- `Collisions/CapsuleSphere.hpp` - Capsule vs Sphere
- `Collisions/CapsuleCuboid.hpp` - Capsule vs AABB
- `Collisions/CapsuleTriangle.hpp` - Capsule vs Triangle (terrain mesh)

### Math/Space3D Intersections
- `Intersections/LineCapsule.hpp` - Line-Capsule raycasting
- `Intersections/SegmentCapsule.hpp` - Segment-Capsule intersection

### Math/Space2D Primitives
- `Point.hpp` - 2D point (alias of `Vector< 2, precision_t >`)
- `Line.hpp` / `Segment.hpp` / `Circle.hpp` / `Triangle.hpp` - 2D primitives
- `AARectangle.hpp` - Axis-aligned 2D bounding box. **Stored as min/max points** and behaves like
  `Space3D/AACuboid`: the default ctor and `reset()` both yield the **empty/inverted state**
  (`isValid()` false), so point-cloud accumulation is `reset()` (or default) then `merge(Point)` /
  `mergeX` / `mergeY`. Static constexpr factories: `AARectangle::Unit()` (0,0,1,1 unit box) and
  `AARectangle::Zero()` (concrete 0,0,0,0 origin seed for the position+size setters, since they
  can't build on the empty default).
  Parity helpers: `centroid()`, `farthestPoint()`, `highestLength()`, `size()`, `contains(Point)`,
  corner-pair `set()`, non-mutating `merged()` / `intersection()`. `farthestPoint()` is float-only
  (`requires` clause). `centroid()` is `requires`-split per precision: floating returns the exact
  `(min+max)*0.5`, **integer returns the truncated integer midpoint `(min+max)/2`** (never the silent
  `(0,0)` collapse a float `*0.5` cast back to `int` would produce). `isValid()` rejects non-finite
  dimensions for floating types.
  **One intentional deviation from AACuboid**: `width()`/`height()` clamp to 0 when `max ≤ min`
  (AACuboid returns raw `max-min`) — because `AARectangle` is also instantiated for integers, where
  the inverted-sentinel `max-min` would be UB (signed) / wrong (unsigned). Invisible for valid rects.

### General Concepts
- `Observer.hpp` / `Observable.hpp` - Event pattern
- `ThreadPool.hpp` - High-performance thread pool
- `JSON.hpp` - JSON manipulation
- `FlagTrait.hpp` - Flag management
- `NamableTrait.hpp` - Naming trait
- `TokenFormatter.hpp` - Case detection/conversion (zero-allocation design)
- `INIParser.hpp` - INI-style parser (INIVariable, INISection, INIParser)
- `SourceCodeParser.hpp` - Source code parser with annotations

### I/O Foundation
- `IO/ByteStream.hpp` - Abstract polymorphic stream interface (read/write/seek/tell)
- `IO/FileStream.hpp` - File-backed ByteStream (ifstream/ofstream)
- `IO/MemoryStream.hpp` - Memory-backed ByteStream (random-access)
- `IO/IO.hpp` - File utility functions (exists, extension, etc.)

### Factories (all use unified ByteStream I/O)
- `PixelFactory/` - Image manipulation (`FileIO.hpp`, `StreamIO.hpp`)
- `VertexFactory/` - Geometry generation/manipulation (`FileIO.hpp`, `StreamIO.hpp`)
- `WaveFactory/` - Audio manipulation (`FileIO.hpp`, `StreamIO.hpp`)
