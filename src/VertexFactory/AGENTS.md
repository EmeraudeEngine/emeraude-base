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

**ShapeVertex<T>** - Position + normal + tangent (+ its handedness) + texture coordinates,
influences and weights for one vertex.
- ⚠️⚠️ **`biNormal()` is DERIVED, not stored**: `cross(normal, tangent) * tangentHandedness`.
  The handedness is a **signed scalar member**, neutral at `+1`, and it is the only thing that
  distinguishes a **mirrored UV island** from a plain one — mirroring flips the authored bitangent
  relative to the cross product. It arrived 2026-08-28; before that the sign did not exist and
  `setTangent(Vector<4>)` silently DROPPED its W, so every mirrored island lit its normal map
  backwards (measured on the Khronos `NormalTangentMirrorTest`: two of its four columns scattered
  over 107.7° and 102.9° of highlight angle, against 0.3° on the reference column).
  glTF's `TANGENT` accessor is a vec4 for exactly this reason. A caller that only knows a direction
  uses the `Vector<3>` overload, which deliberately leaves the handedness untouched.
- ⚠️⚠️ **`sizeof(ShapeVertex<float>)` is 84 bytes and that is part of a PERSISTED FORMAT**:
  `FileFormatNative` writes vertices as a raw blob of that size. Any change to this structure MUST
  bump the native format version in the same commit — a size change with an unchanged version
  number is silent data corruption, not a compatibility question. The size is pinned by
  `test_VertexFactoryShapeVertex.cpp` so the failure is a red test, not a corrupt file.
- See: `ShapeVertex.hpp`, `test_VertexFactoryShapeVertex.cpp`

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

### Vegetation — the tree skeleton and its growers (Sept 2026)

The tree work is split in two phases on purpose: **growing the botany**, which is done, and
**turning it into a mesh**, which is not (`docs/todo/tree-generator-skinning-lod-and-wind-channels.md`).
Keeping the skeleton is what lets one tree be re-skinned at several LOD levels, baked into an
imposter, turned into physics capsules or given a wind hierarchy without growing it again.

**TreeSegment / TreeLeafAttachment / TreeSkeleton** - the botany, no mesh
- A segment is a straight tapered internode. Its frame sits at the START and its **local +Y is the
  growth axis** — read it with `CartesianFrame::localYAxis()`, never `upwardVector()`: that
  accessor carries no up/down meaning and cannot invert if the world convention moves again.
- ⚠️⚠️ **Contract: a grower MUST add a segment only after its parent**, so a parent index is
  always smaller than its child's. The reverse passes (`computeRadiiFromPipeModel()`) and the
  future skinning phase both rely on it, and **nothing checks it for you**. The unit test
  `VertexFactoryTreeSkeleton.growersAddAParentBeforeItsChild` is the only guard.
- `TreeChildTable` is the children of every segment in compressed row form, built on demand
  (`buildChildTable()`) rather than stored — a vector of vectors would allocate once per segment.
- `computeRadiiFromPipeModel(tipRadius, exponent)` is the discrete pipe model (da Vinci / Murray):
  `radius = tipRadius * tipCount^(1/exponent)`, and a segment ends on the radius of its THICKEST
  child so the surface stays continuous. ⚠️ **On an unbranched chain the tip count never changes,
  so this model yields NO taper there** — that is the model, not a defect. The parametric grower
  sets its own radii from the Weber & Penn formulas and must not call it.
- `makeTreeFrame(position, growthAxis)` (in `TreeSegment.hpp`) is the single way to turn a
  direction into a segment frame. The spin around the axis is picked from the world axis least
  aligned with it, so a nearly vertical branch does not flip its frame between two segments.
- See: `TreeSegment.hpp`, `TreeLeafAttachment.hpp`, `TreeSkeleton.hpp`

**TreeParameters / TreeParametricGrower** - Weber & Penn, SIGGRAPH '95
- The parameter set IS the paper's, so a table published for Arbaro or Blender's Sapling transfers
  unchanged; `TreeCrownShape`'s enumeration order is the paper's numbering. `quakingAspen()` is
  the paper's set; `broadleaf()` and `conifer()` are **designed here** and say so.
- Not implemented, deliberately: the periodic taper range ]2, 3] (cacti) and pruning.
- Measured on this workstation: aspen 3 178 segments / 24 375 leaves / ~15 m in **3 ms**,
  broadleaf 7 276 / 49 410 / ~9 m in 4 ms, conifer 1 952 / 34 080 / ~18 m in 3 ms.
- See: `TreeParameters.hpp`, `TreeParametricGrower.hpp`

**TreeColonizationGrower** - Runions, Lane & Prusinkiewicz, EGWNP 2007
- Space colonization: a cloud of attraction points fills the crown, every tip steers toward the
  points closest to it, a point is consumed when a tip reaches it. The branching pattern is not
  prescribed — it emerges from the competition. Driven by the SHAPE of the crown, not by species
  parameters: `growFromAttractors()` takes any cloud, so the crown can be any volume.
- Radii come from the pipe model, which is why that lives on the skeleton and not in this class.
- A uniform grid (`NodeGrid`) answers "which tip is nearest this point". ⚠️ **Nothing ever
  iterates that `unordered_map`** — it is read by key only and each cell keeps insertion order.
  Iterating it would make a seeded result depend on the hash table layout.
- Measured: ellipsoid crown ~875 segments / 9 m in 4 ms; a 2 000-attractor wide crown 2 061
  segments / 10.7 m x 9.6 m in 11 ms.
- See: `TreeColonizationGrower.hpp`

**TreeSkinningOptions / TreeSkinner / TreeMesh** - the mesh
- `TreeSkinner` walks the skeleton branch by branch and emits ONE continuous tube per branch:
  consecutive segments share their ring, `u` runs around, `v` is the arc length over
  `barkTextureLength`, and a branch closes on a single **apex vertex**, never a cap disk. The old
  stub's pile of capped cylinders is what this replaces.
- ⚠️⚠️ **The skinner builds its OWN rotation-minimizing frame** (double reflection, Wang, Jüttler,
  Zheng & Liu, ACM TOG 27(1), 2008) and must never use the segment frames: `makeTreeFrame()` picks
  its spin from the world axis least aligned with the direction, so it FLIPS when a branch crosses
  that threshold and the tube corkscrews. Guarded by
  `VertexFactoryTreeSkinner.theTubeDoesNotCorkscrewOnIndependentlyFramedSegments`.
- **One radial count per BRANCH**, from its base radius — not per ring. Two rings of different
  counts cannot be chained into a quad strip, so "N follows the radius" is applied at branch
  granularity. The axial half of the resolution is `axialStride`, which skips ring stations.
- **Junctions are the cheap version**, deliberately: the child's first rings sit on the parent
  AXIS, are swallowed by the parent tube, and a `collarScale` fakes the swelling. A real
  bifurcation stitch would have to be redone for every level of detail, and is invisible under
  bark at the distance a tree is seen from.
- Two groups, always: `TreeMesh::BarkGroup` then `TreeMesh::LeafGroup`, so
  `Interface::buildSubGeometries()` gives the engine two sub-geometries and therefore two
  materials. The leaf group is declared even when empty.
- Vertex channels (Crytek/SpeedTree convention): **R** trunk bending weight (normalized height,
  raised to `trunkBendExponent`), **G** branch bending weight (arc along the branch; 0 on the
  trunk, which would otherwise sway twice), **B** leaf flutter phase (constant per card), **A**
  baked occlusion.
- ⚠️ The **A channel is a DENSITY estimate, not ray-traced occlusion**: it counts leaves and
  branch nodes in the cell around a point, normalized on the densest cell. It captures the one
  thing that reads — the inside of a canopy is darker than its rim — for a hash lookup. Real
  occlusion needs rays and belongs to a bake.
- `TreeMesh` is the structured result: the skeleton, the level chain, and the crossed-quads card.
  ⚠️ The card is kept APART from the chain, not as its last rung: it carries ONE group because it
  samples a single baked atlas, and that atlas is engine work
  (`vegetation-octahedral-imposter-atlas`).
- The atlas **parametrisation** is here, though, in `Math/OctahedralMapping.hpp`: the baker asks
  `octahedralCellDirection()` which direction to render a cell from, the shader asks
  `octahedralBlend()` which three cells to blend. One header on purpose — a baker and a shader
  that disagree by one cell show a neighbouring view. ⚠⚠ **The map is 2-to-1 on the border**, so two
  border cells legitimately hold the same view; never try to make them distinct
  (`docs/caution-points.md` § Math).
- Measured, quaking aspen seed 1 (3 178 segments, 24 375 leaves), levels 0 to 3:
  **119 709 / 29 457 / 7 403 / 1 736** triangles, 114 ms for the whole chain plus the card.
  Conifer: 150 924 / 35 160 / 8 787. A colonized crown: 12 591 / 6 286 / 4 476 — it plateaus,
  because its triangle count is set by its topology (874 short segments in 402 branches) rather
  than by its radii.
- See: `TreeSkinningOptions.hpp`, `TreeSkinner.hpp`, `TreeMesh.hpp`

**TreeGenerator** - the façade
- Picks a grower, skins the chain, optionally builds the card, and returns a `TreeMesh`. Use it
  unless you want a skeleton with no mesh, or a mesh from a skeleton you built yourself.
- ⚠️ Deliberately **not a template**, unlike the rest of the module: it is the only compiled unit
  here and the **only source** of the `emeraude_base_vertex` object library
  (`CMakeLists.txt:426`). Making it header-only would leave that target with no source at all —
  remove the target in the same move, or keep a `.cpp`. A tree is generated at load time, so
  `float` costs nothing and the template instantiation is spent once.

**Grid / GridQuad** - 2D grids with height displacement; `Types.hpp` holds the grid transform mode.
- See: `Grid.hpp`, `GridQuad.hpp`

**Cost of building a shape** - ⚠️ read this before writing any dense generator
- `ShapeBuilder` → `Shape::addTriangle()` → `addEdge()` x3, plus `addVertex()` /
  `addVertexColor()` when `dataEconomy` is on (**the default**).
- All three went from a linear scan to a hashed lookup (`addEdge()` 2026-09-21, the two others
  2026-09-22). A 65 536 triangle sphere went from **14 s** to **21.6 ms** with the default
  options, and the path is linear.
- ⚠️⚠️ **Which of the two merge paths is faster depends on the MERGE RATIO, not on the size.**
  On that sphere 83 % of the corners merge and the in-build hash wins (21.6 ms against 25.5 ms
  for `enableDataEconomy(false)` + a batch `ShapeProcessor::deduplicateVertices()`). On a tree
  canopy almost every leaf-card vertex is unique, so the in-build hash pays an insertion per
  corner and merges nothing: **122 ms batch against 208 ms in-build** for the aspen chain,
  124 against 263 for the conifer. `TreeSkinner` keeps the batch path for that measured reason.
  Measure your own generator before choosing; do not assume the default.
- ⚠️⚠️ **The merge semantic changed with it** (owner decision): a hash cannot reproduce an epsilon
  equality, which is not transitive, so `addVertex()` now merges by a **grid** of
  `mergeTolerance` (1e-4, the value `ShapeProcessor` uses, so the two paths agree). It merges
  MORE than the old epsilon did — 33 169 vertices against 36 405 — and merging progressively is
  order-dependent at a cell boundary where the batch pass is not, so the two differ by a handful
  of vertices. Never pin an exact count across them.
- ⚠️ It does NOT close a UV seam and must not: the two sides carry u = 0 and u = 1. A sphere keeps
  48 unpaired edges and is not watertight in the edge sense — that was never the epsilon's doing.
- ⚠️⚠️ **Any pass that renumbers or splits vertices must end on `Shape::rebuildEdges()`.** An edge
  holds vertex indices and a triangle holds edge indices, so both go stale. `deduplicateVertices()`,
  `generateLightmapUV()` and `generateUVUnwrap()` all did it silently; see
  `docs/caution-points.md` § VertexFactory for the measured damage. Rebuilding is only affordable
  because `addEdge()` is hashed now.
- Measurements, the epsilon-vs-grid explanation and the seam consequence:
  `docs/caution-points.md` § VertexFactory. Open item:
  `docs/todo/shape-addvertex-dedup-is-quadratic.md`.

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

### Sphere longitude: direction and seam

> [!IMPORTANT]
> **U grows EASTWARD, and the seam sits on `+Z`.** With north at `+Y`, east is the POSITIVE rotation
> about `+Y` (the right-hand rule — which is why the Earth turns counter-clockwise seen from above
> the north pole). Measured: `U = 0` and `U = 1` both fall on `+Z`, `U = 0.5` on `-Z`. On an
> equirectangular map the U edges are the ANTIMERIDIAN and `U = 0.5` is the prime meridian, so
> Greenwich faces `-Z`, the engine's FORWARD, and the seam falls mid-Pacific where cartographers
> already put it so it cuts no land.
> Pinned by `sphereUGrowsEastwardNotWestward` and `sphereSeamSitsOnPositiveZ`.

> [!CAUTION]
> **`generateSphere` parameterises theta the OTHER WAY, on purpose-looking but load-bearing detail.**
> It uses `sTheta = -sin(theta)`, so a growing theta walks `+Z -> -X -> -Z -> +X`: the NEGATIVE
> rotation about `+Y`. Pairing U with a growing theta therefore grows it WESTWARD and MIRRORS every
> texture. U is deliberately run backwards against theta to compensate. Do not "tidy" that away.
> ⚠️⚠️ **A polar screenshot cannot catch this.** A mirrored globe still converges cleanly at the pole
> and still shows a plausible Arctic; only the east-west handedness gives it away. This defect was
> declared fixed on the strength of exactly such a capture, and the owner caught it on screen
> afterwards. Judge handedness, never convergence.

> [!IMPORTANT]
> **Seam vertices are DUPLICATED** at the same position, one carrying `U = 0` and one `U = 1`. That
> is what stops U interpolating from 1 back to 0 across the last quad and squeezing the whole map
> into one slice. A generator sharing a single vertex there is broken even though its UVs look fine
> in isolation.

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

> [!CAUTION]
> **`Shape::flipYAxis()` does NOT mirror the vertex colours.** It walks `m_vertices` and
> `m_triangles`; the colours live in the separate `m_vertexColors`. The gem cuts paint a VOLUMETRIC
> colour — position mapped to RGB — so while they were authored Y-down and mirrored afterwards,
> their green channel described the PRE-mirror frame, inverted against their own geometry. It was
> visible: the vertex-colour row of `parametric-geometries` iterates every shape, gems included.
> Fixed, then made moot by the re-authoring below. **Keep the rule**: anything derived from a
> position before a mirror is stale after it, and `flipYAxis()` will not tell you.

### The gem cuts are authored Y-UP (since Aug 2026)

All eleven gem generators author their facet math directly in the Y-up world: table above, culet
below. `convertYDownAuthoring()` and its eleven call sites are **deleted**.

> [!IMPORTANT]
> **Removing a mirror reverses handedness, and that has FOUR consequences, not one.** Each was
> needed at every converted site; missing any single one corrupts the output:
> 1. the authored `Y` literals negate;
> 2. every `emitTriangle()` swaps its first two vertices **and their UVs together** — that is what
>    `reverseWinding()` used to do;
> 3. the operands of each cross product feeding a per-face normal swap, since
>    `cross(M·e₁, M·e₂) = −M·cross(e₁, e₂)`;
> 4. **the bitangent** of the per-face tangent frame: `cross(normal, T)` becomes `cross(T, normal)`.
>    Forget it and `v` silently becomes `1 − v`.
>
> The stored normals need no attention at all: `emitTriangle()` derives the flat normal from the
> triangle it emits, so swapping the emission order corrects them by construction.

Guarded by `gemCutsKeepTheirGeometry`, which pins every cut through invariants taken over TRIANGLE
CORNERS. Two lessons are built into it, both learned the hard way during the conversion:

> [!CAUTION]
> **An invariant symmetric under the transformation it is meant to detect is not an invariant.**
> The plain sum of `V` cannot see the bitangent flip, because `Σ(1 − v) = n − Σv`, which equals `Σv`
> on every symmetric facet. Three cuts were converted with `V` flipped and the sums saw nothing;
> only the princess, whose chevrons are asymmetric, gave it away. The SQUARED sums do not cancel.
>
> **Never measure over `shape.vertices()`.** That array is the result of DEDUPLICATION, which
> depends on the emission order — exactly what re-authoring changes. The trillion came out with six
> fewer shared vertices, identical triangles and identical geometry, and every vertex-array sum moved
> with the count, accusing a correct conversion. The vertex count is a packing detail, not a shape.

## Critical Attention Points

- **MDx is read-only by design** — do not add a write path; document the boundary instead.
- **StreamIO needs an explicit `FileFormatType`** — a memory buffer has no extension; never try to
  content-sniff text formats (OBJ/ASCII-STL have no reliable magic).
- **Untrusted counts**: any new parser MUST validate header-derived counts against the actual
  stream size before allocating (the Tier-1 vuln above). Add a fuzz target under `src/Fuzzing/`.
- **Never change `ShapeVertex` or `ShapeTriangle` without bumping `FileFormatNative`'s version.**
  The payload is a raw blob of `sizeof(...)`; the count validation can PASS on a wrong stride, so
  the corruption is silent. Version 2 (2026-08-28) is the tangent-handedness layout; there is
  deliberately no version-1 read path — the format had no users yet (owner decision), and a loud
  refusal beats a plausible misparse.
- **`setTangent(Vector<4>)`'s W is the handedness, not a homogeneous coordinate.** Dropping it
  compiles, renders, and is wrong only on mirrored UVs — the worst kind of silent defect.

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
