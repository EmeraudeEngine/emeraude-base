## Critical Attention Points

- **MDx is read-only by design** — do not add a write path; document the boundary instead.
- **StreamIO needs an explicit `FileFormatType`** — a memory buffer has no extension; never try to
  content-sniff text formats (OBJ/ASCII-STL have no reliable magic).
- **Untrusted counts**: any new parser MUST validate header-derived counts against the actual
  stream size before allocating (the Tier-1 vuln above). Add a fuzz target under `src/Fuzzing/`.
- **Never change `ShapeVertex` or `ShapeTriangle` without bumping `FileFormatNative`'s version.**
  The payload is a raw blob of `sizeof(...)`; the count validation can PASS on a wrong stride, so
  the corruption is silent. Version 2 (2026-08-28) is the tangent-handedness layout (84 bytes); there
  is deliberately no version-1 read path — the format had no users yet (owner decision), and a loud
  refusal beats a plausible misparse. **Version 3 (2026-10-03)** appends the secondary texture
  coordinates LAST (92 bytes, `Vector< 2 >`): a version-2 record is then the exact prefix of a
  version-3 one, so version 2 is still read (the secondary set at (0, 0)). `FileFormatNative`'s
  `static_assert` pins that prefix. ⚠️ A future member must also go LAST to keep that property.
- **The secondary texture coordinates** (`ShapeVertex::secondaryTextureCoordinates()`, glTF's
  `TEXCOORD_1`, owner 2026-10-03: a field, not a side array). Every vertex pays 8 bytes; every
  algorithm that copies a vertex carries them. `flipTextureV()` negates both sets (a file format's V
  convention applies to every set). `ShapeProcessor::deduplicateVertices()` keys on them: without
  that, it merged vertices differing only in their second unwrap (test
  `deduplicatingVerticesKeepsDistinctSecondaryTextureCoordinates`: 615 merged on a 16 × 8 sphere).
  `Shape::create(Indexed)VertexBuffer()` write them after the primary set when asked (the engine's
  vertex format order). `Shape::addVertex()` (manual building) does not take them: (0, 0).
- **`setTangent(Vector<4>)`'s W is the handedness, not a homogeneous coordinate.** Dropping it
  compiles, renders, and is wrong only on mirrored UVs — the worst kind of silent defect.
- **`sin(π)` and `cos(π/2)` are not 0 in float** (-8.74e-8 and -4.37e-8). A generator that computes
  its pole or equator ring by trigonometry puts the pole vertices on a ~1e-7 ring: a hole invisible in
  a render and to any exact-position weld. Set the extreme rings exactly (`generateSphere` -Y pole,
  `generateHemisphere` pole and equator, `generateCapsule` both poles, fixed 2026-09-28). Pinned by
  `VertexFactoryShapeGenerator.polesLieExactlyOnTheAxis`.
- **`Shape::memoryOccupied()` / `Grid::memoryOccupied()` count CAPACITIES, and `clear()` keeps them.**
  `clear()` empties the vectors and hash maps without giving their memory back: freeing a shape's
  memory takes a swap with empty containers (the engine's CPU-copy release, phase 1). Their bytes
  are an estimate for the hash indexes (one pointer per bucket, one node per element; libc++ and
  libstdc++ size them differently: macOS counted citadel's geometry 48 MiB lower). Pinned by
  `VertexFactoryGrid.MemoryOccupiedCountsTheHeights` and
  `VertexFactoryShapeGenerator.memoryOccupiedCountsTheStorages`.
- **The construction-time indexes are released once the shape is final:
  `Shape::releaseConstructionIndexes()`** (the engine calls it after a geometry's GPU upload,
  2026-10-03). `m_unpairedEdges`, `m_vertexIndex` and `m_vertexColorIndex` only serve
  `addVertex()` / `addVertexColor()` / `addTriangle()` (its `addEdge()`) and `rebuildEdges()`, yet
  they lived as long as the shape: in the engine's citadel the unpaired-edge index alone weighed
  360 MiB of the 1359 MiB of its 339 geometries. Any of those calls after the release first
  rebuilds all three from the stored data (`restoreConstructionIndexes()`: edges replayed in
  insertion order, so each slot keeps its first half-edge and its paired flag). ⚠️ The rebuilt
  vertex index holds EVERY stored vertex, those `saveVertex()` stored without merging included, and
  uses the merge tolerance current at the rebuild: a later `addVertex()` may merge with a vertex the
  never-released shape would not have. `clear()` / `resizeData()` reset the state. Pinned by
  `VertexFactoryShapeBuilder.releasingTheConstructionIndexesFreesTheirMemory` and
  `.anEditAfterTheReleaseRebuildsTheConstructionIndexes` (fails with the rebuild disabled: 28
  vertices instead of 26).
