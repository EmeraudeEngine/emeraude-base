# VertexFactory - Geometry Construction, Loading and Processing

Context for developing 3D geometry (mesh) manipulation in Emeraude Engine.

## Module Overview

**Geometry foundation** - Provides mesh data containers, procedural generation, file loading
(OBJ/STL/MDx/native), processing (decimation, hole-filling, splitting) and analysis. All
CPU-side geometry in the engine flows through VertexFactory `Shape`. glTF/FBX are **not** here
— those are engine-level `AssetLoaders` (fastgltf/ufbx); VertexFactory is the format-agnostic
in-memory representation they feed into.

## Architecture (Separation of Concerns)

### Core Data Containers

**Shape<vertex_data_t, index_data_t>** - The mesh container (template, float/uint32_t default)
- Holds vertices, triangles, optional per-vertex colors, sub-geometry layers and an AABB.
- The canonical in-memory geometry type; every loader/generator produces a `Shape`.
- See: `Shape.hpp`

**ShapeVertex<T>** - Position + normal + tangent + texture coordinates for one vertex.
- See: `ShapeVertex.hpp`

**ShapeTriangle<V,I>** / **ShapeEdge** - Triangle (3 vertex indices + attributes) / edge primitives.
- See: `ShapeTriangle.hpp`, `ShapeEdge.hpp`

### Construction & Generation

**ShapeBuilder<V,I>** - The primary geometry-construction API
- `ConstructionMode` (Triangles, TriangleStrip, …) + `beginConstruction()` / `setPosition()` etc.
- Configured by **ShapeBuilderOptions** (global vertex color, normal/texcoord generation, …).
- Every file loader builds its `Shape` through a `ShapeBuilder`.
- See: `ShapeBuilder.hpp`, `ShapeBuilderOptions.hpp`

**ShapeGenerator** - Procedural primitives (triangle, quad, cube, sphere, cylinder, cone, …).
- See: `ShapeGenerator.hpp`

**TriangleGenerator / TreeGenerator / Grid / GridQuad** - Specialized generators
- `TreeGenerator` is the only compiled unit (`TreeGenerator.cpp`); the rest are header-only.
- `Grid` generates 2D grids with height displacement; `Types.hpp` holds the grid transform mode.
- See: `TriangleGenerator.hpp`, `TreeGenerator.hpp/.cpp`, `Grid.hpp`, `GridQuad.hpp`

### Processing & Analysis

**ShapeProcessor** - Hole detection / hole filling and other in-place geometry operations.
**ShapeDecimator** - Mesh decimation via Quadric Error Metrics (Garland & Heckbert); also the
normal-map dilation helper.
**ShapeAssembler** - Groups several shapes into one.
**ShapeSplitter** - Splits a shape (returns a split result).
**Silhouette** - Silhouette-edge detection.
**XRayAnalyzer** - X-ray cross-section analysis.
**CapUVMapping / Normal / TextureCoordinates** - UV-mapping helpers for caps and spherical/cubic
coordinate generation.
- See the matching `*.hpp`.

### File I/O Classes (Unified ByteStream Architecture)

All format handlers operate on `IO::ByteStream &` (polymorphic: file or memory), exactly like
WaveFactory and PixelFactory.

**FileIO** - File-backed dispatcher **by extension**
- Creates `IO::FileStream`, delegates to the format handler's `readStream`/`writeStream`.
- Dispatch: `ee3d`→Native, `obj`→OBJ, `stl`→STL, `mdl`/`md2`/`md3`/`md5mesh`→MDx.
- `read(path, ShapeLoadResult &, ReadOptions)` / `write(shape, path, WriteOptions)`.
- See: `FileIO.hpp`

**StreamIO** - Memory buffer I/O, **explicit format** (a buffer has no extension)
- `read(vector<byte>, FileFormatType, ShapeLoadResult &, ReadOptions)` — dispatches to all four
  handlers, mirroring the `PixelFactory::StreamIO::read(data, format, …)` precedent.
- `write(shape, FileFormatType, vector<byte>, WriteOptions)` — Native/OBJ/STL write; MDx is
  read-only (its `writeStream` fails cleanly).
- See: `StreamIO.hpp`

**FileFormatInterface** - Abstract base + the shared option/format vocabulary
- `readStream(IO::ByteStream &, ShapeLoadResult &, ReadOptions)` / `writeStream(IO::ByteStream &, Shape &, WriteOptions)`.
- `enum class FileFormatType { Native, OBJ, STL, MDx }` (the StreamIO selector).
- `ReadOptions`: `scaleFactor`, `flip{X,Y,Z}Axis`, `request{Normal,TangentSpace,TextureCoordinates,VertexColor}`.
- `WriteOptions`: currently empty.
- See: `FileFormatInterface.hpp`

**FileFormatNative** - Emeraude native binary (`EE3D_V1`)
- 32-byte header + three `uint64` counts (vertices/triangles/colors) + payload blob.
- Read **and** write. The only format with full StreamIO write fidelity.
- See: `FileFormatNative.hpp`

**FileFormatOBJ** - Wavefront OBJ (ASCII), read + write
- Negative (relative) face indices supported via `resolveIndex()` (1-based, end-relative).
- See: `FileFormatOBJ.hpp`

**FileFormatSTL** - Stereolithography (binary + ASCII), read + write
- See: `FileFormatSTL.hpp`

**FileFormatMDx** - id Tech loader (MDL / MD2 / MD3 / MD5), **read-only by design**
- Magic dispatch: `IDPO`→MDL, `IDP2`→MD2, `IDP3`→MD3, text→MD5. `writeStream` always fails
  (these are third-party *import* formats — a deliberate boundary, not a missing feature).
- `IDTechUnitScale` (0.01) converts the ~100× idTech unit system to engine units.
- See: `FileFormatMDx.hpp`

**ShapeLoadResult** - The read destination: a `Shape` plus optional skeletal-animation data.
- See: `ShapeLoadResult.hpp`

## Usage Patterns

### Load a mesh from a file (automatic dispatch)

```cpp
VertexFactory::ShapeLoadResult<float, uint32_t> result;
VertexFactory::ReadOptions options;
options.requestNormal = true;
options.requestTextureCoordinates = true;
VertexFactory::FileIO::read("model.obj", result, options);
const auto & shape = result.shape;
```

### Memory buffer I/O (StreamIO) — explicit format

```cpp
// A memory buffer carries no extension, so the format is selected explicitly.
std::vector<std::byte> bytes = /* ... from an archive / network / asset pack ... */;
VertexFactory::ShapeLoadResult<float, uint32_t> result;
VertexFactory::StreamIO::read(bytes, VertexFactory::FileFormatType::OBJ, result);

// Write (Native/OBJ/STL writable; MDx read-only).
std::vector<std::byte> output;
VertexFactory::StreamIO::write(result.shape, VertexFactory::FileFormatType::Native, output);
```

## Input Robustness (Ave robustus! — A.2 / A.3)

The four file-format parsers consume **untrusted input**. Owner directive (shared with
WaveFactory): *malformed input must never crash the engine — cancel the load (`return false`),
nothing fancier.* Hardening landed in the A.2 characterization pass and the A.3 fuzzing pass
(`src/Fuzzing/fuzz_{native,stl,mdx,obj}`), each fix covered by
`Testing/test_VertexFactoryFileFormats.cpp`, green in Release **and** under ASan/UBSan via `ctest`.

- **The shared Tier-1 vuln**: an untrusted count read from a header fed to `resize`/`reserve`/
  `vector(n)` without validation → `std::length_error`/`std::terminate`/OOM under `-fno-exceptions`.
  Native (ee3d) and STL now validate every count against the **remaining stream bytes**
  (overflow-safe, division-first) before allocating.
- **MDx** (read-only legacy MDL/MD2/MD3/MD5): a uniform `exceedsStream()` guard at every alloc
  site, plus the fuzzing fixes — MD2/MDL empty-frame null-deref + unchecked triangle vertex/st/
  normal indices, MD3 OOB index + 64 GB `reserveData` (triangle total bounded vs stream) + offset
  signed-overflow (`int64_t`), MD5 null-deref (derive `jointCount` from `joints.size()`, validate
  weight→joint / vertex→weight / triangle→vertex cross-refs before building).
- **OBJ**: a face index that references a non-existent vertex is bounds-checked **before** the
  access (was `std::vector::at` → `out_of_range` → terminate). `resolveIndex()` widens to `int64_t`
  so a list larger than `INT_MAX` cannot wrap (the former `int32_t` cast was UB).
- **Diagnostics**: all `std::cerr` in the parsers, FileIO dispatch and StreamIO migrated to the
  `EmEn::Base::Logging` hook (no raw `cerr` in this module).

## Critical Attention Points

- **MDx is read-only by design** — do not add a write path; document the boundary instead.
- **StreamIO needs an explicit `FileFormatType`** — a memory buffer has no extension; never try to
  content-sniff text formats (OBJ/ASCII-STL have no reliable magic).
- **Untrusted counts**: any new parser MUST validate header-derived counts against the actual
  stream size before allocating (the Tier-1 vuln above). Add a fuzz target under `src/Fuzzing/`.

## Open Axis-B markers (not yet done)

Tracked in `docs/plans/ave-robustus.md` (§6, "Real correctness gaps"):
- `TriangleGenerator.hpp:87` — `@FIXME Remove internal triangles`.
- `ShapeBuilder.hpp:636` — `FIXME: Check this` review.
- `ShapeDecimator` arithmetic was audited clean in A.4 (`width*height` already 64-bit).
(The inventory's `OrientedCuboid` marker lives in `Math`, not VertexFactory.)

## Code References

| File | Description |
|------|-------------|
| `Shape.hpp` | Mesh container (vertices, triangles, colors, layers, AABB) |
| `ShapeVertex.hpp` / `ShapeTriangle.hpp` / `ShapeEdge.hpp` | Mesh primitives |
| `ShapeBuilder.hpp` / `ShapeBuilderOptions.hpp` | Primary construction API + options |
| `ShapeGenerator.hpp` / `TriangleGenerator.hpp` / `TreeGenerator.hpp` / `Grid.hpp` | Procedural generators |
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
