## Code References

| File | Description |
|------|-------------|
| `Shape.hpp` | Mesh container (vertices, triangles, colors, layers, AABB; the skinning and the triangles' color and edge indexes as tables) |
| `ShapeVertex.hpp` / `ShapeTriangle.hpp` / `ShapeEdge.hpp` | Mesh primitives |
| `ShapeBuilder.hpp` / `ShapeBuilderOptions.hpp` | Primary construction API + options |
| `ShapeGenerator.hpp` / `Grid.hpp` | Procedural primitives and grids |
| `NestedLattice.hpp` / `.cpp` | Nested square windows as one watertight triangle list (a level of detail around a point) |
| `ShelterMap.hpp` / `.cpp` | Where the land shelters the water of a grid of heights: the open sea, the enclosed basins, the narrow passages |
| `TreeSegment.hpp` / `TreeLeafAttachment.hpp` / `TreeSkeleton.hpp` | Tree botany, no mesh |
| `TreeParameters.hpp` / `TreeParametricGrower.hpp` | Weber & Penn tree growth |
| `TreeColonizationGrower.hpp` | Space-colonization tree growth |
| `TreeSkinningOptions.hpp` / `TreeSkinner.hpp` | Skeleton to mesh, per level of detail |
| `TreeMesh.hpp` | Skeleton + level chain + imposter card |
| `TreeGenerator.hpp` / `.cpp` | The façade, a compiled unit (with `ShapeSimplifier.cpp`, `NestedLattice.cpp` and `ShelterMap.cpp`) |
| `ShapeProcessor.hpp` / `ShapeDecimator.hpp` / `ShapeAssembler.hpp` / `ShapeSplitter.hpp` | Processing |
| `Silhouette.hpp` / `XRayAnalyzer.hpp` | Analysis |
| `CapUVMapping.hpp` / `Normal.hpp` / `TextureCoordinates.hpp` | UV / coordinate helpers |
| `Types.hpp` | Grid transform mode + small enums |
| `FileIO.hpp` | File-backed format dispatcher by extension (IO::FileStream) |
| `StreamIO.hpp` | Memory buffer I/O, explicit `FileFormatType` (IO::MemoryStream) |
| `FileFormatInterface.hpp` | Abstract base + `FileFormatType`, `ReadOptions`, `WriteOptions` |
| `FileFormatNative.hpp` | Native `EE3D_V1` binary (read/write; version 4 written, 2 and 3 read) |
| `FileFormatOBJ.hpp` | Wavefront OBJ text (read/write) |
| `FileFormatSTL.hpp` | Stereolithography binary/ASCII (read/write) |
| `FileFormatMDx.hpp` | id Tech MDL/MD2/MD3/MD5 (read-only) |
| `ShapeLoadResult.hpp` | Load destination: `Shape` + optional skeletal animation |
