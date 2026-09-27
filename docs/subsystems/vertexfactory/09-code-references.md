## Code References

| File | Description |
|------|-------------|
| `Shape.hpp` | Mesh container (vertices, triangles, colors, layers, AABB) |
| `ShapeVertex.hpp` / `ShapeTriangle.hpp` / `ShapeEdge.hpp` | Mesh primitives |
| `ShapeBuilder.hpp` / `ShapeBuilderOptions.hpp` | Primary construction API + options |
| `ShapeGenerator.hpp` / `Grid.hpp` | Procedural primitives and grids |
| `TreeSegment.hpp` / `TreeLeafAttachment.hpp` / `TreeSkeleton.hpp` | Tree botany, no mesh |
| `TreeParameters.hpp` / `TreeParametricGrower.hpp` | Weber & Penn tree growth |
| `TreeColonizationGrower.hpp` | Space-colonization tree growth |
| `TreeSkinningOptions.hpp` / `TreeSkinner.hpp` | Skeleton to mesh, per level of detail |
| `TreeMesh.hpp` | Skeleton + level chain + imposter card |
| `TreeGenerator.hpp` / `.cpp` | The façade, and the module's only compiled unit |
| `ShapeProcessor.hpp` / `ShapeDecimator.hpp` / `ShapeAssembler.hpp` / `ShapeSplitter.hpp` | Processing |
| `Silhouette.hpp` / `XRayAnalyzer.hpp` | Analysis |
| `CapUVMapping.hpp` / `Normal.hpp` / `TextureCoordinates.hpp` | UV / coordinate helpers |
| `Types.hpp` | Grid transform mode + small enums |
| `FileIO.hpp` | File-backed format dispatcher by extension (IO::FileStream) |
| `StreamIO.hpp` | Memory buffer I/O, explicit `FileFormatType` (IO::MemoryStream) |
| `FileFormatInterface.hpp` | Abstract base + `FileFormatType`, `ReadOptions`, `WriteOptions` |
| `FileFormatNative.hpp` | Native `EE3D_V1` binary (read/write) |
| `FileFormatOBJ.hpp` | Wavefront OBJ text (read/write) |
| `FileFormatSTL.hpp` | Stereolithography binary/ASCII (read/write) |
| `FileFormatMDx.hpp` | id Tech MDL/MD2/MD3/MD5 (read-only) |
| `ShapeLoadResult.hpp` | Load destination: `Shape` + optional skeletal animation |
