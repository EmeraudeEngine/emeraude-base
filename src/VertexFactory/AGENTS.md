# VertexFactory - Geometry Construction, Loading and Processing

Context for developing 3D geometry (mesh) manipulation in Emeraude Engine.

## Module Overview

**Geometry foundation** - Provides mesh data containers, procedural generation, file loading
(OBJ/STL/MDx/native), processing (decimation, hole-filling, splitting) and analysis. All
CPU-side geometry in the engine flows through VertexFactory `Shape`. glTF/FBX are **not** here
— those are engine-level `Loaders` (fastgltf/ufbx); VertexFactory is the format-agnostic
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

**TreeGenerator / Grid / GridQuad** - Specialized generators
- `TreeGenerator` is the only compiled unit (`TreeGenerator.cpp`); the rest are header-only.
- `Grid` generates 2D grids with height displacement; `Types.hpp` holds the grid transform mode.
- See: `TreeGenerator.hpp/.cpp`, `Grid.hpp`, `GridQuad.hpp`

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

> [!CRITICAL]
> **id Tech → engine axes (Y-up, Aug 2026): `(md5.y, md5.z, md5.x)`, a ROTATION (det +1).**
> It used to be `(y, -z, x)`, a REFLECTION (det -1), for the old Y-down world. That single sign
> had **three** compensations hanging off it, and they only make sense together:
>
> | What | Coupled to the determinant? | State |
> |---|---|---|
> | Position + vertex-normal transform (MDL/MD2/MD3/MD5) | yes — the sign IS the transform | fixed |
> | MD5 vertex-normal negation after `computeVertexTBNSpace()` | **yes** — a reflection inverts the cross product | **deleted** |
> | Triangle winding reversal (2,1,0), all four formats | **NO** — a FORMAT convention | **KEPT** |
>
> ⚠️⚠️ **The winding reversal is NOT a mirror compensation.** id Tech stores triangles in the
> opposite winding to the engine's front-face convention, full stop. Measured: removing it renders
> every ID model inside out (`boss1.md2` shows its inner limb faces and a hollow head). It survived
> the Y-up flip untouched. Do not "simplify" it away along with the determinant.
>
> ⚠️⚠️ **The MD5 conversion lived in FIVE places and only two were named.** `md5ToEnginePosition()`,
> `md5ToEngineRotation()` (the joint orientations — `M` must stay identical to the position one),
> the normal negation above, a **fourth copy inlined in the skinning loop** — the one that actually
> builds the visible mesh — and a **fifth in a different module entirely**,
> `Animation/MD5AnimParser.hpp` (`.md5anim` clips). Updating the named helpers and missing the
> inline copy is what left the skinned CyberDemon upside down while MDL/MD2/MD3 were already
> upright. The inline copy now calls `md5ToEnginePosition()`; keep it that way.
>
> ⚠️⚠️ **The fifth site was missed by the migration and by the inventory that recorded four.** The
> ANIMATION parser kept `(y, -z, x)` while the MESH path moved to `(y, z, x)`, so clips lived in a
> mirrored frame relative to the mesh they animated. Fixed Aug 2026, and now pinned by
> `MD5AnimParser.conversionIsARotationNotAReflection` — **the search key for "where is this
> conversion?" is the module list above, not a grep for `FileFormatMDx`.**
>
> ⚠️ That test needs DIFFERENT discriminating axes for its two halves, which is a trap in itself:
> for POSITIONS only md5 Z separates the two transforms; for ROTATIONS md5 Z is precisely the axis
> that CANNOT, because conjugating a rotation by a reflection that negates the rotation's own axis
> leaves it unchanged (`S·R(Y,θ)·Sᵀ == R(Y,θ)`). The rotation half turns about md5 X instead.
>
> **Beyond that pin, verification is visual, per format** — no unit test sees the mesh path:
> `geometry-loader --demo-options 7` = QuakePlayer (MDL), `8` = boss1 (MD2), `6` = cyberdemon (MD5).
> Upright, solid (no inner faces), correctly lit.

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

## Texture coordinate convention (Y-up world)

`V = 0` is the **top row of the image** (Vulkan image origin is top-left). Pairing that with a
Y-up world gives the two rules every hand-authored generator must follow:

- **Vertical faces** (normal in the XZ plane): `V = 0` pairs with the **`+Y`** edge, `V = 1` with
  `-Y`. Reference: `generateQuad`.
- **Horizontal faces**: follow `generatePlane` — on a `+Y`-facing surface `U` grows with `+X` and
  `V` grows with `+Z`. The `-Y`-facing face of a closed shape is the same mapping with `V` negated.

> [!IMPORTANT]
> **`generateScreenQuad()` is the one deliberate exception, and it must stay that way.** It is a
> fullscreen NDC quad (`-1..1`, no options, no scale) for the post-processor and the overlay manager,
> whose source images are already in **screen space** — so its `V` pairs with **`+Y`**, the exact
> opposite of every world-space generator. It is not a `generateQuad` that someone forgot to migrate.
> Locked by `screenQuadPairsVWithPositiveYOnPurpose`, so a "harmonising" sweep over the `V` axis
> fails loudly instead of silently flipping the whole post-process chain and the entire overlay.

> [!CAUTION]
> **The V pairing is a defect class of its own, distinct from winding, from vertex coordinates and
> from declared normals.** The Y-up switch reversed the emission order of `generateCuboid`'s faces
> (winding) but left every `setTextureCoordinates` paired with the position it had in the Y-down
> era, so all six faces rendered **V-flipped** while compiling clean and passing the whole unit suite.
> No assertion on the **geometry** can see it — the shape, its normals and its winding are all
> correct, only the image is upside down. But an assertion on the **pairing** catches it outright,
> and there is now one: `test_VertexFactoryShapeGenerator.cpp` walks every vertex of a shape, keeps
> the vertical faces (`|normal.Y| < 0.5`) and requires `V = 0` above mid-height, `V = 1` below.
> Fixed and measured for both `generateCuboid` overloads (Aug 2026) — negating `V` on all six faces
> restores the pre-migration relationship, in which the up-facing horizontal face agreed with
> `generatePlane`. `generateHollowedCube` is deliberately excluded: its UVs are parameterised
> per beam (`U` = beam width, `V` = length/width ratio) and assembled through `ShapeAssembler`
> rotations, so its `V` is not tied to world `Y`.

> [!CAUTION]
> **A sphere is the one shape where a `V`-versus-`Y` probe cannot see a UV defect.** `generateSphere`
> carried TRANSPOSED coordinates until Aug 2026 — the accumulator named `U` advanced per STACK
> (latitude), the one named `V` per SLICE (longitude) — so every texture came out rotated a quarter
> turn. Comparing `V` against `Y` reads a flat 0.5 above and below the equator when `V` is really
> longitude: it looks like a symmetric shape, not a defect, which is how this survived a full audit.
> **The discriminator is a LATITUDE RING**: along one, longitude must sweep and latitude must hold.
> Transposed, the ring gives span `U` = 0 and span `V` = 1 — the perfect signature.
> Pinned by `sphereMapsUToLongitudeAndVToLatitude`. ⚠️ Not a Y-up residue: it predates the flip and
> depends on no axis sign.

## Winding convention (front faces are CCW)

A front face winds **counter-clockwise around its own outward normal** —
`VK_FRONT_FACE_COUNTER_CLOCKWISE`, the pipeline default. The `emitTriangle(A, B, C)` helpers emit in
**natural A/B/C order**: the historical B/C swap was a mirror compensation and is gone.

> [!IMPORTANT]
> **Verify a winding by COMPUTING it, never by eye.** Take `cross(B-A, C-A)` and check it against the
> generator's own declared outward normal. Gated by
> `loopDrivenGeneratorsWindCCWAroundTheirNormals`, which covers every loop-driven generator
> (sphere, cylinder, cone, disk, torus, capsule, hemisphere, tube, arrow) with `cuboid`, `plane` and
> `triangle` as **CONTROLS** — a probe that fails a control is a broken probe, not a discovery.
> All nine measured 100% correct in Aug 2026: the migration plan had listed them as "still mirrored"
> for weeks, and they were not.

> [!CAUTION]
> **Two triangle families carry NO evidence about winding. Excluding them is not optional.**
> 1. **Degenerate** triangles (zero area) — the collapsed quads at a sphere's poles.
> 2. Triangles **straddling a normal discontinuity** — a cap fan sharing a rim vertex with the side.
>    Averaging a radial normal with a cap normal yields a direction that is NOT the face's outward
>    normal, so the comparison is meaningless. **Omitting this filter reported 13 false positives on
>    `generateArrow` alone**, indistinguishable at a glance from a genuine mirror defect. With it,
>    the arrow is 15/15 clean.

> [!CAUTION]
> **The check is only valid while the normals are INDEPENDENT of the winding.** `ShapeGenerator`
> calls neither `computeVertexNormal()` nor `computeTriangleNormal()` — every normal is authored from
> the parametric surface — which is what makes it evidence rather than a tautology. That property is
> itself pinned by `theWindingCheckRejectsAMirroredShape`: it applies `Shape::reverseWinding()`
> (indices swapped, normals untouched) and requires **every** triangle carrying evidence to flip its
> verdict. If someone ever derives the normals from the winding, that test fails instead of the gate
> silently passing forever while measuring nothing. **Never delete it to "simplify" the suite.**

### The gem cuts are the exception: judge them against the SOLID, never their own normals

The twelve gem-cut generators pass a **separately computed** normal to their `emitTriangle()`, built
from a different vertex pair than the triangle it is attached to. The two drifted: measured Aug 2026,
**11 of the 12 cuts carried facet normals pointing INTO the solid** (princess: every single facet),
while their **winding was correct everywhere**.

> [!CAUTION]
> **Judging a gem with `countWindingAgreement()` reports the exact opposite of the truth** — it
> compares against precisely the normals that are wrong, so correct winding reads as "mirror-wound".
> That is why `gemCutsWindAndFaceOutward` references `faceCentre - centroid` instead, which owes
> nothing to the authored normals. ⚠️ That reference is valid for **convex** shapes only; every cut
> here is convex, but do not reuse it on a shape with a concavity.

**The fix, applied to all eleven generators:** a flat facet's normal IS its own geometric normal, so
`emitTriangle()` now derives it from the triangle being emitted and keeps the authored one only as a
degenerate fallback and for the per-face UV tangent frame (deliberately untouched, so UVs did not
move). Deriving it makes the drift **structurally impossible** rather than merely fixed.

> [!CAUTION]
> **Guard on the normalization RESULT, never on the input length.** `Vector::normalized()` gives up
> when `lengthSquared()` trips `Utility::isZero()` and returns the **ZERO vector**. A sliver can
> therefore clear a `length() > 1e-7` test and still come back with no normal at all — worse than an
> inward one, since it kills lighting outright. Two princess-cut culet slivers did exactly that
> (`length` 1.25e-4, `lengthSquared` 1.56e-8). A successful normalization has `lengthSquared() == 1`,
> so testing the **result** against `0.5` is unambiguous and needs no epsilon of its own. The gate
> uses the same criterion to decide which triangles carry evidence: one the library cannot normalize
> has no normal to judge.

⚠️ Still open, and untouched by the above: the gem facet math is authored **Y-down** and converted by
`convertYDownAuthoring()`. That is a readability debt with **no visual effect** — the produced
geometry is correct. Do not confuse it with the normals defect, which was real.

## Critical Attention Points

- **MDx is read-only by design** — do not add a write path; document the boundary instead.
- **StreamIO needs an explicit `FileFormatType`** — a memory buffer has no extension; never try to
  content-sniff text formats (OBJ/ASCII-STL have no reliable magic).
- **Untrusted counts**: any new parser MUST validate header-derived counts against the actual
  stream size before allocating (the Tier-1 vuln above). Add a fuzz target under `src/Fuzzing/`.

## Open Axis-B markers (not yet done)

Tracked in `docs/plans/ave-robustus.md` (§6, "Real correctness gaps"): all resolved.
- `ShapeBuilder.hpp:636` — `FIXME: Check this` verified correct (TriangleFan vertex shift) + test.
- `TriangleGenerator` — the unused `generateEnvelope` ("bad algorithm") generator was **removed**
  as dead code (no caller anywhere); the whole `TriangleGenerator.hpp` is gone.
- `ShapeDecimator` arithmetic was audited clean in A.4 (`width*height` already 64-bit).
(The inventory's `OrientedCuboid` marker lives in `Math`, not VertexFactory — also resolved.)

## Code References

| File | Description |
|------|-------------|
| `Shape.hpp` | Mesh container (vertices, triangles, colors, layers, AABB) |
| `ShapeVertex.hpp` / `ShapeTriangle.hpp` / `ShapeEdge.hpp` | Mesh primitives |
| `ShapeBuilder.hpp` / `ShapeBuilderOptions.hpp` | Primary construction API + options |
| `ShapeGenerator.hpp` / `TreeGenerator.hpp` / `Grid.hpp` | Procedural generators |
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
