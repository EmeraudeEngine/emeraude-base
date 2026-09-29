## VertexFactory: Shape Splitting, Boundary Loops and Sealing

### BoundaryLoop (Shape member)

`Shape` stores boundary loops as `std::vector<BoundaryLoop<index_data_t>>` — ordered sequences of vertex indices forming open edges. Populated by:
1. **ShapeSplitter** (automatic, during plane split with `sealCut=false`)
2. **ShapeProcessor::findBoundaryLoops()** (on-demand analysis of any geometry)

Key Shape methods:
- `boundaryLoops()` — const/mutable access
- `isSurfaceOpened()` — true if boundary loops exist
- `boundaryLoopsAnalyzed()` — true if analysis was performed (distinguishes "no holes" from "not checked")
- `surfaceArea()` — sum of triangle areas (squared units)
- `volume()` — divergence theorem (cubic units, returns 0 if surface is open)

### ShapeSplitter

Splits a shape by a plane into front/back parts. Key features:
- **`sealCut` option**: When true, automatically seals cut surfaces via `ShapeProcessor`
- **Boundary edge collection**: Intersection vertex pairs collected during triangle clipping
- **Spatial dedup**: Quantized position keys merge vertices from different code paths (`getOrCopyVertex` vs `getOrCreateIntersectionVertex`)
- **Angle-based loop chaining**: At T-junctions, picks the neighbor with straightest continuation (highest dot product between incoming/outgoing directions)
- **Loop merging**: Adjacent loops with close endpoints are concatenated (handles 4 endpoint combinations with auto-reversal)
- **Mirror strategy**: Back part produces cleaner boundary loops (no on-plane triangle artifacts), mirrored to front part by position matching

Code references:
- `ShapeSplitter.hpp:split()` — Main entry point
- `ShapeSplitter.hpp:chainBoundaryEdges()` — Spatial dedup + angle-based chaining + merge
- `ShapeSplitter.hpp:mirrorBoundaryLoops()` — Position-based loop remapping from back to front
- `ShapeSplitter.hpp:sealCutSurface()` — Delegates to `ShapeProcessor::sealAllBoundaryLoops()`

### ShapeProcessor

Geometry analysis and modification on a `Shape` reference:
- **`deduplicateVertices()`**: Quantization-based vertex merge with index remapping
- **`findBoundaryLoops()`**: Edge counting + canonical position map + angle-based chaining + loop merging
- **`sealBoundaryLoop()`**: Ear-clipping triangulation (O(n·r)) with:
  - 2D projection via orthonormal basis from cap normal
  - CCW winding enforcement
  - Reflex/ear classification with only-reflex containment test
  - New cap vertices with correct normal + planar UV projection (square, normalized [0,1])
  - Cap triangles emitted in NATURAL A/B/C order: `(axisU, axisV, capNormal)` is right-handed, so a CCW ear
    in the 2D basis winds CCW around `capNormal` — the front face. ⚠️ Until Sep 2026 they were emitted C/B/A, a
    Y-down mirror compensation the Y-up migration missed: every sealed cap was culled and showed the inside of
    the model (projet-alpha `geometry-generator`). Gate: `VertexFactoryShapeSplitter.sealedCapsWindCCWAndFaceOutOfTheirPart`
    (computes the winding and checks the cap faces OUT of its part; fails on the C/B/A order).
  - ⚠️ The Newell fallback (no `expectedNormal`) takes the cap normal from the loop's traversal direction, so
    it is only as outward as the loop order; the splitter always passes the plane normal and never uses it.
- **`sealAllBoundaryLoops()`**: Uses pre-computed `Shape::boundaryLoops()` if available, falls back to `findBoundaryLoops()`

### ShapeDecimator (QEM Mesh Decimation)

Reduces polygon density while preserving shape using Quadric Error Metrics (Garland & Heckbert):
- **Internal position-only dedup**: Caller passes original shape; copy + dedup done internally for QEM connectivity
- **Ratio parameter**: 0.0 = max reduction, 1.0 = no change
- **Boundary/UV seam preservation**: Penalty quadrics (configurable weight, default 1000)
- **Topology checks**: Link condition + normal flip rejection for manifold preservation
- **Auto-flip normals**: Compares computed normals with source, flips if majority disagree (handles Y-flip models)
- **Normal map baking** (`normalMapResolution > 0`):
  - Generates lightmap UVs internally via ShapeProcessor
  - Ray-casts each UV texel from low-poly to high-poly (Möller-Trumbore)
  - Spatial grid 128³ for fast triangle lookup, search radius 1
  - UV-space 64² grid for fast texel→triangle mapping
  - Tangent-space encoding ([-1,1] → [0,255] RGBA)
  - 8-pass dilation to eliminate UV seams
  - ThreadPool parallelization (per-row)

Code references:
- `ShapeDecimator.hpp:decimate()` — Main entry, returns decimated Shape
- `ShapeDecimator.hpp:normalMap()` — Access baked normal map Pixmap
- `ShapeDecimator.hpp:bakeNormalMap()` — Internal GPU-to-CPU baking pipeline

### ShapeProcessor: UV Generation

Two UV generation modes on `ShapeProcessor`:

**`generateLightmapUV()`** — Per-triangle UV packing for uniform pixel density:
- Each triangle flattened to 2D independently
- Area-proportional sizing with shelf packing (bin width estimated from total area)
- `smoothVertexAttributesByPosition()`: Averages normals/tangents of co-located vertices for seamless normal mapping across duplicated triangle boundaries
- Ideal for normal map baking (uniform texel density)

**`generateUVUnwrap()`** — LSCM chart-based UV for artistic texturing:
- Chart segmentation by normal discontinuity (configurable angle threshold, default 66°)
- Vertex duplication at chart boundaries for exclusive UV ownership
- Least Squares Conformal Maps parameterization per chart (CGLS sparse solver)
- Shelf packing into [0,1]² UV space
- Tangent space recomputation after UV assignment

**`deduplicateVertices(keepNormals, keepTextureCoordinates)`**:
- Boolean flags control which attributes are compared (default both true)
- Position-only mode (`false, false`) builds mesh connectivity from OBJ-style triangle soup

Code references:
- `ShapeProcessor.hpp:generateLightmapUV()` — Lightmap UV with smooth attributes
- `ShapeProcessor.hpp:generateUVUnwrap()` — LSCM chart-based UV
- `ShapeProcessor.hpp:smoothVertexAttributesByPosition()` — Position-based normal/tangent averaging
- `ShapeProcessor.hpp:solveCGLS()` — Conjugate gradient least-squares sparse solver
- `Shape.hpp:declareTextureCoordinatesAvailable()` — Flag for external UV generation

### Algorithms: DelaunayTriangulation

Bowyer-Watson incremental Delaunay triangulation with constrained boundary:
- Super-triangle enclosure → incremental point insertion → circumcircle invalidation
- Post-filter: remove triangles outside boundary polygon (ray-casting centroid test)
- Template on `data_t` (float/double precision)
- Located in `Algorithms/DelaunayTriangulation.hpp` (standalone, generic)

### XRayAnalyzer (CPU fallback)

CPU-based volumetric cross-section scanner in `VertexFactory/XRayAnalyzer.hpp`:
- Multiple shapes with CartesianFrame positioning
- Viewpoint-relative bounding box → square output images
- `prepare()` precomputes transformed triangles + 2D spatial grid (128²)
- `scan(depth)` per-slice ray-cast with ThreadPool parallelization
- `scanAll()` single-pass ray-cast + per-slice extraction (1000× faster than per-slice scan)
- See `Graphics/Compute/XRayAnalyzer` (emeraude-engine repository) for the GPU-accelerated version
