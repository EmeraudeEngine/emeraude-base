## Architecture (Separation of Concerns)

### Core Data Containers

**Shape<vertex_data_t, index_data_t>** - The mesh container (template, float/uint32_t default)
- Holds vertices, triangles, optional per-vertex colors, sub-geometry layers and an AABB.
- The canonical in-memory geometry type; every loader/generator produces a `Shape`.
- **The rare attributes are tables of the Shape, not members of every vertex and triangle** (2026-11-26, owner
  decision): the skinning (`isSkinningAvailable()`, `vertexInfluences(v)`, `vertexWeights(v)`, `setVertexSkinning()`),
  the triangles' color indexes (`triangleVertexColorIndex(t, corner)`, `setTriangleVertexColorIndexes(t, {a, b, c})`)
  and edge indexes (`triangleEdgeIndex(t, corner)`, set by `addTriangle()` / `rebuildEdges()`); the two triangle tables
  are flat, three `index_data_t` a triangle in corner order (`cornerSlot()`). A table is allocated by the first
  value that is not the default and then follows the vertex or triangle count; a shape that never had one answers the
  defaults (`(-1, -1, -1, -1)`, zero weights, color index 0, no edge), past its end too. ⚠️ A pass that replaces the
  vertex list carries the skinning itself: `remapVertexAttributes(remapping)` after it (the first old vertex of a new
  index wins, as the whole-vertex copy did; `ShapeProcessor::deduplicateVertices()`). Measured: the 312 MB OBJ
  (3.9 M vertices, 7.8 M triangles) 818 → **520 MiB** of `Shape`, its peak 822 → **524 MiB**. Tests
  `VertexFactoryShapeTables.*`.
- See: `Shape.hpp`

**ShapeVertex<T>** - Position + normal + tangent (+ its handedness) + texture coordinates (two sets) for one vertex;
the skinning is a table of the Shape since 2026-11-26.
- ⚠️⚠️ **`biNormal()` is DERIVED, not stored**: `cross(normal, tangent) * tangentHandedness`.
  The handedness is a **signed scalar member**, neutral at `+1`, and it is the only thing that
  distinguishes a **mirrored UV island** from a plain one — mirroring flips the authored bitangent
  relative to the cross product. It arrived 2026-08-28; before that the sign did not exist and
  `setTangent(Vector<4>)` silently DROPPED its W, so every mirrored island lit its normal map
  backwards (measured on the Khronos `NormalTangentMirrorTest`: two of its four columns scattered
  over 107.7° and 102.9° of highlight angle, against 0.3° on the reference column).
  glTF's `TANGENT` accessor is a vec4 for exactly this reason. A caller that only knows a direction
  uses the `Vector<3>` overload, which deliberately leaves the handedness untouched.
- **The secondary texture coordinates** (2026-10-03): `setSecondaryTextureCoordinates()` /
  `secondaryTextureCoordinates()`, a `Vector<2>` (glTF's `TEXCOORD_1`), (0, 0) when the source has
  none. Details and traps: `07-critical-attention-points.md`.
- ⚠️⚠️ **`sizeof(ShapeVertex<float>)` is 60 bytes (84, then 92 with the secondary set, until the skinning left it on
  2026-11-26) and that is part of a PERSISTED FORMAT**:
  `FileFormatNative` writes vertices as a raw blob of that size. Any change to this structure MUST
  bump the native format version in the same commit — a size change with an unchanged version
  number is silent data corruption, not a compatibility question. The size is pinned by
  `test_VertexFactoryShapeVertex.cpp` so the failure is a red test, not a corrupt file.
- See: `ShapeVertex.hpp`, `test_VertexFactoryShapeVertex.cpp`

**ShapeTriangle<V,I>** / **ShapeEdge** - Triangle (3 vertex indices + surface tangent, normal, handedness: 40 bytes in
float, 64 before the color and edge indexes became tables of the Shape) / edge primitives.
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
- Vertex channels (Crytek/SpeedTree convention, made CONTINUOUS 2026-09-23 — owner decision
  "hiérarchie continue"): **R** trunk bending weight (normalized height, raised to
  `trunkBendExponent`), **G** branch bending weight = the distance along the skeleton FROM THE
  TRUNK over the longest such distance (0 on the trunk, which would otherwise sway twice; a child
  starts where its parent is), **B** the phase of the FIRST-ORDER limb, shared by everything it
  carries (twigs and leaves), **A** baked occlusion. A leaf card takes its twig's G at the petiole
  (a little more at the tip) and its twig's B: it sways WITH the twig; its flutter is the engine's,
  weighted by the card V. `TreeSkinner::buildWindHierarchy()` resolves the path and phase per
  segment.
- ⚠️⚠️ **Until 2026-09-23 G was `arc / branch length`, restarting at 0 at every branch base**, and B was
  a per-LEAF phase that also drove the branch wave: every child branch stayed put where its parent
  swayed (~0.5 G) and every leaf swayed out of phase with its twig — the wind tore the trees apart
  (owner-spotted). Regression test `theBranchSwayIsContinuousAcrossTheJunctions` (a hand-built
  skeleton: the old formula reads a 0.51 phasor gap at the junction, the new one < 0.05).
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
- **A level keeps the tree's VOLUME** (owner, 2026-09-23: "le LOD est censé décomplexifier l'arbre
  sans changer son volume général"). Each level keeps HALF the leaves of the previous one
  (`coarsened()`), the survivors are enlarged by sqrt(2) per level to keep the coverage, and each
  enlarged card is pulled toward the crown centre by the half-diagonals it grew (`emitLeafCard()`),
  so its farthest point stays the finest level's. ⚠️ Before: a QUARTER of the leaves per level,
  enlarged from the petiole OUTWARD — the broadleaf canopy was **+62 % wider and +28 % taller** at
  level 3, and trees shrank as the camera came closer. Now, worst of the four presets at level 3:
  **−8 % radius, +2 % height**, leaf area 1.0 (test `theCanopyEnvelopeHoldsAcrossTheLevels`). A cap
  on the enlargement was measured and rejected: ×3 left 14 % of the leaf area at level 3.
- Measured 2026-09-23, seed 1, levels 0 to 3 (triangles): quaking aspen **119 709 / 53 833 /
  25 683 / 12 400**, broadleaf 287 622 / 117 516 / 58 048 / 28 968 (after the base-split fix below), conifer 150 924 / 69 240 /
  34 347 / 17 175, colonized crown 27 634 / 15 855 / 11 178 / 9 162 (it plateaus: its count is set
  by its topology rather than its radii). The finest level of the broadleaf takes ~0.5 s.
- ⚠️⚠️ **`nBaseSplits` forks the ORIGINAL trunk once** (`StemRequest::baseSplitAllowed`): a fork clone is a
  level-0 stem restarting at segment 0, and until 2026-09-23 every clone forked again at its own base —
  364 "trunks" on the broadleaf, the 243 last ones BARE, 1.5 cm thick, poking 2 m out of the crown and
  popping in when the finest level took over (owner-spotted). Test
  `theTrunkForksOnlyAtItsBaseAndNoStemIsBare`. Diagnose a stem that sticks out by counting the order-0
  branches and the branches that carry neither a child nor a leaf, before touching the skinner or the LOD.
- See: `TreeSkinningOptions.hpp`, `TreeSkinner.hpp`, `TreeMesh.hpp`

**TreeGrowthCurve** - the AGE of a tree (Sept 2026, owner: "improve the generator to take the age into account")
- `TreeGenerator::setAge(years)`: 0 (the default) grows the preset exactly as it always was
  (guard: `VertexFactoryTreeGrowthCurve.theReferenceAgeLeavesThePresetUntouched`, bit-exact).
- The model is allometric, applied to a COPY of the parameters at `generate()`; the owner chose it
  over a year-by-year growth simulation (Palubicki et al. 2009).
- Height: Chapman-Richards, used as a RATIO to the preset's reference age, so Hmax cancels.
  `heightFactor(a) = ((1 - e^(-k·a)) / (1 - e^(-k·ref)))^p`: 1 at `ref`, monotonic, saturating.
- From the height ratio h:
  - the trunk radius/length as h^0.5 (elastic similarity, McMahon 1973);
  - the bare foot of the trunk as `1 - (1 - b)·h^(0.6 - 1)` — the crown lifts, capped at 0.8;
  - the flare as h^0.5, capped at 2.5×;
  - the upward attraction divided by h^0.5 (old limbs droop);
  - the leaf count × h and the leaf card × h^0.5, which keeps the foliage cover for h times the leaf
    triangles.
  - Colonization: the crown and clear trunk × h, the spacing × h^0.5, the attractors × h² (the crown
    surface), the iterations × h.
- Preset curves `{referenceAge, k, p}`:
  - aspen `{25, 0.05, 1.3}`: 13 m, ×1.5 at 100 y;
  - broadleaf `{25, 0.02, 1.3}`: 9 m, ×3.3 at 200 y, ~30 m;
  - conifer `{35, 0.025, 1.3}`: 18 m, ×2.0 at 200 y;
  - colonized `{25, 0.03, 1.3}`: ×2.3 at 150 y.

  These are the model's own choices, not one species' measurements.
- ⚠️ **The curve drives the TRUNK, not the bounding box.** An old broadleaf's skeleton came out 3.87×
  taller for a factor of 3.28: its lifted crown starts higher and the branch tips overtop the trunk.
  The test reads the order-0 top.
- ⚠️ **Cost**: the old broadleaf is 724 638 triangles at LOD 0 against 287 622 (×2.5), the old conifer
  ×1.8, the old colonized crown ×4.2 (the attractors). On `terrain`, ~10 % of the woods as old stands cost
  +10-11 ms next to one (projet-alpha `src/Builtin/AGENTS.md` § 6d).
- See: `TreeGrowthCurve.hpp`, tests `VertexFactoryTreeGrowthCurve.*`.

**TreeGenerator** - the façade
- Picks a grower, skins the chain, optionally builds the card, and returns a `TreeMesh`. Use it
  unless you want a skeleton with no mesh, or a mesh from a skeleton you built yourself.
- ⚠️ Deliberately **not a template**, unlike the rest of the module: it is the only compiled unit
  here and the **only source** of the `emeraude_base_vertex` object library
  (`CMakeLists.txt:426`). Making it header-only would leave that target with no source at all —
  remove the target in the same move, or keep a `.cpp`. A tree is generated at load time, so
  `float` costs nothing and the template instantiation is spent once.
- **The species names its materials** (owner decision 2026-09-23): `setBarkMaterial("...")` /
  `setLeafMaterial("...")` — NAMES only, this library knows no material — copied into the result
  (`TreeMesh::barkMaterial()` / `leafMaterial()`); the engine resolves them
  (`Scenes::Toolkit::vegetationMaterial()`). Presets: `TreeGenerator::quakingAspen()` (leaf001),
  `broadleaf()` (leaf003), `conifer()` (leaf007), `colonizedCrown()` (leaf002), all on
  `Vegetals/palm_bark`.
- ⚠️ **The leaf card's width/length must be the leaf IMAGE's** (`skinningOptions().setLeafAspectRatio()`):
  the texture covers the whole card. 1 for a square image, 0.5 for the 1024 × 2048 pine twig; the
  presets set it (the former default 0.7 squashed every square leaf).
- ⚠️ **A leaf image's OPACITY sets the leaf size a preset needs.** `conifer()`'s leaf007 is a whole twig of
  needles, 23 % opaque (the old `fullfoliage` card was 96 %): at the preset's 10 cm the crown fell from 99 %
  to 54 % opaque, so the preset now sets `setLeafScale(0.2)` (77 % of the foliage footprint back, no
  triangle added). `colonizedCrown()`'s leaf002 is 36 % opaque: `colonizationGrower().setLeafScale(0.225)`.
  Measure the mask's coverage before choosing a size. ⚠️ Density was NOT why the far pines were bare: that
  was the fixed alpha-test threshold on the mips (engine `src/Graphics/AGENTS.md` § Alpha COVERAGE).
- ⚠️ **V = 1 at the petiole, V = 0 at the tip** (`TreeSkinner::emitCard()`): V = 0 is the image's first
  row, its top, and a leaf image stands on its stem. It was the other way round until 2026-09-23 —
  every leaf upside down (owner-spotted).

**Grid / GridQuad** - 2D grids with height displacement; `Types.hpp` holds the grid transform mode.
- See: `Grid.hpp`, `GridQuad.hpp`

**NestedLattice** (2026-11-23) - `buildNestedLattice()` / `countNestedLattice()`: square windows nested in one
another, window k drawn with cells of 2^k finest cells wherever window k - 1 does not cover it, as ONE triangle list.
A level of detail around a point for a surface that cannot afford its finest cells everywhere (the engine's terrain
ray-tracing proxy, under a triangle budget).
- **Pure topology**: the vertices are lattice nodes (level, x, y); the caller gives each one its position. A `Grid`
  and its `coarsened()` copies do it without a crack: point samples, the same node at the same place at every level.
- **Watertight by construction**: a level's cell lying along the finer window is cut into a fan of three triangles
  around the finer lattice's node in the middle of the shared side — never two triangles, which would leave that
  node ending two finer sides in the middle of a coarse one (a T-junction: a crack once the nodes take a height).
  Every other cell is cut like a plain grid: along (x, y)-(x + 1, y + 1), turning (x, y) -> (x, y + 1) ->
  (x + 1, y + 1). A single window IS the plain grid, nodes in row-major order.
- ⚠️ **A node the windows share is ONE vertex**, the finest level's: never give a level its own copy of a border
  node — two positions an ulp apart are a crack for a ray.
- ⚠️ **What the windows must respect** (refused otherwise, one error on `VertexFactory::NestedLattice`, nothing
  built): corner and side of window k on multiples of 2^(k + 1) (whole cells of the level around it); window k
  inside window k + 1 and smaller; a side shared with the next window only on the OUTER border (where window k
  touches a side of window k + 1, window k + 1 touches the same side of window k + 2 — otherwise a level would meet
  cells four times smaller than its own, which a fan around one node cannot close); at most
  `MaxNestedLatticeLevels` (16) windows and `MaxNestedLatticeNodes` (2^26) lattice nodes.
- `countNestedLattice()` gives the vertex and triangle counts without building: what a caller sizes a budget on. The
  build checks itself against it.
- Proof: `test_VertexFactoryNestedLattice.cpp` — every side of every triangle met by exactly one other triangle
  unless it lies on the outer border, whose whole length is covered; one winding; the outer window's area covered
  once; one vertex per node. The check fails on a triangulation without the fan.
- See: `NestedLattice.hpp`, `NestedLattice.cpp`

**ShelterMap** (2026-11-24) - `buildShelterMap()`: for each cell of a grid of heights (`HeightCells`), how OPEN the
water is there — whether the sea around the grid reaches it, or land shelters it. What an ocean calms its waves with
(a lake inside an island is not as rough as the open sea).
- A cell whose height is under `ShelterMapOptions::waterLevel` is water; any other is land, a height that is not a
  number included. The grid lies in the open water: beyond its border is the sea, one cell from the border's water.
- **The open water spreads from the border**, cell to cell by their sides, through every water cell that stands
  farther from the land than half `minimumPassage` (in cells): it does not enter a passage narrower than that. What
  it reached is `OpenWater` (255), and so is the water within that half passage plus two cells of it — the strip
  along a coast, the mouth of a passage. Farther in, the openness falls to `ShelteredWater` (0) over `fade` cells
  (smoothstep). A basin the water does not join to the border is sheltered; so is every water of a grid whose
  border is all land.
- Distances are chamfer distances (5 a side, 7 a diagonal: within 8 % of the straight line), measured THROUGH the
  water from the open water (Dial's buckets: small integer distances). ⚠️ Water never passes between two land cells
  that touch by a corner: a wall drawn in diagonal holds.
- A LAND cell takes the openness of the water cell nearest to it, so a filtered read along a shore mixes values of
  that shore alone. A grid without water is open everywhere.
- Refused with one error on `VertexFactory::ShelterMap`, the map left empty: no cell or more than
  `MaxShelterMapSide` (4096) a side, a height count that is not the cell count, a water level that is not a number, a
  passage or a fade outside 0 to `MaxShelterMapReach` (1024 cells).
- Cost: linear in the cells; 54 ms for 1024 × 1024 cells, the reading of the heights included (measured in the
  engine, Release, one thread).
- Proof: `test_VertexFactoryShelterMap.cpp` — a lake inside an island, a grid closed by land, a passage narrower
  and wider than the threshold, the fade (never rising again), the diagonal wall, a NaN height, the refusals.
  ⚠️ The fade's test failed first on a real gap: a strip of water along the border too narrow to hold a cell far
  enough from the land was sheltered — the sea beyond the border is now a source too.
- See: `ShelterMap.hpp`, `ShelterMap.cpp`

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
**Silhouette** - Silhouette (contour) edge extraction for a perspective (`build(eyePosition)`) or an
orthographic (`buildOrthographic(viewDirection)`) viewer, both in the shape's local space. A contour
edge joins a front- and a back-facing triangle; a boundary edge is kept when its triangle faces the
viewer. `prepare(shape)` computes the adjacency and the triangle planes once; a build then allocates
nothing (redo `prepare()` when positions change: skinning, deformation). Edges come from the visible
triangle, counter-clockwise, so their indexes carry the visible side of a seam.
- ⚠️ It does NOT use `Shape::edges()`: those half-edges pair by vertex INDEX, so a cuboid (one vertex
  set per face) pairs no edge between faces and a UV seam splits a sphere. It welds by EXACT position
  instead (meshoptimizer's shadow index buffer approach). Positions that differ by float noise stay
  apart and read as a hole — which is how the generators' pole defect below was found.
- Rewritten 2026-09-28: the old class refused every valid shape (inverted `isValid()` test), called
  methods `Shape` does not have (it never compiled once instantiated), returned the front wireframe
  instead of the contour, and was orthographic only. Tests: `test_VertexFactorySilhouette.cpp`.
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
- `ReadOptions`: `scaleFactor`, `flip{X,Y,Z}Axis`, `request{Normal,TangentSpace,TextureCoordinates,VertexColor}`,
  `weldVertices` (STL only, off by default).
- `WriteOptions`: currently empty.
- See: `FileFormatInterface.hpp`

**FileFormatNative** - Emeraude native binary (`EE3D_V1`)
- 32-byte header (magic, `uint16` version at 8, the two precisions at 10 and 11, the optional tables at 12) + three
  `uint64` counts (vertices/triangles/colors) + the raw vertex and triangle records + the colors, then the optional
  tables: the skinning (influences then weights, per vertex; flag bit 0) and the color indexes (three `index_data_t` per
  triangle; flag bit 1, written when the shape has colors). An unknown flag refuses the file; every table is checked
  against the stream size before any allocation.
- **Version 4 is written** (2026-11-26); versions 2 and 3 are still read, through their former records (`LegacyVertex`,
  `LegacyTriangle`): a record's skinning goes to the table when it is not the default, its color indexes when the file
  has colors, its edge indexes are dropped (the edges are not stored).
- Read **and** write. The only format with full StreamIO write fidelity.
- See: `FileFormatNative.hpp`

**FileFormatOBJ** - Wavefront OBJ (ASCII), read + write
- Read in two passes (count, then assemble). A stream that seeks is read through a 1 MiB window, line by line
  (`IO::StreamLineReader`, the second pass after a seek back): the file is never held whole. A stream that cannot seek
  is read whole first, its lines views into the buffer (`IO::LineReader`). A failed read of the stream refuses the file
  (it would otherwise be the start of the file). Measured 2026-11-26 on a 312 MB file (3.9 M vertices): peak 1461 →
  **822 MiB** with the V-mode change below; source-tree doc 13 § Reading a buffer in place.
- A line is classified by its first word, the blanks being space, tab and '\r' (`lineKind()`, `lineData()`): `v\t1\t2\t3`
  is a vertex (2026-11-26: only `"v "` was, a file written with tabs was refused as holding no vertex). `g` alone is a
  group; `v` alone is a vertex without coordinates, refused.
- Coordinates are read strictly (`readCoordinates()`): a `v` and a `vn` need three numbers, a `vt` its `u` (`v` is 0 when
  absent); a number ends at a blank or the end of the line; a leading `+` is read. A missing or unreadable coordinate
  refuses the file with an error (2026-11-26: it was read as 0 without a word — `"v 1 2"`, `"v 1,5 2,5 3,5"` — and
  `"+1.5"` was 0, `std::from_chars` refusing the sign). The words after the coordinates (`w`, vertex colors) are ignored.
- Negative (relative) face indices supported via `resolveIndex()` (1-based), relative to the attributes read SO FAR (the
  format's definition: each pass counts the `v`, `vt` and `vn` lines met). ⚠️ Until 2026-11-26 the list's FINAL size
  was used: with vertices declared after a face, `f -3 -2 -1` named the last three vertices of the file.
- Only the tables the face mode reads are allocated. In the V mode (`f 1 2 3`) none: a `v` line writes its shape vertex
  directly (`parseFaceAssemblyV()`), so an unreferenced vertex keeps its real position, the vertex count is the `v`
  count, and `vt` / `vn` lines are counted, not read nor declared (before 2026-11-26 a V-mode file with `vn` lines
  declared all-zero normals available). The three indexed forms (V_VN, V_VT, V_VT_VN) are one function,
  `parseFaceAssemblyIndexed()`, the face mode saying which of `vt` and `vn` belong to the vertex key.
- One shape vertex per unique `(v, vt, vn)` triple (`vertexIndexOf()`, a hash map of the triples met), like
  tinyobjloader / Assimp. ⚠️ Until 2026-10-05 a vertex was keyed by ONE index (the most numerous attribute) and
  re-created only when the POSITION differed: a hard edge or a UV seam inherited the first face's normal / UV
  (Critical Attention Points).
- See: `FileFormatOBJ.hpp`

**FileFormatSTL** - Stereolithography (binary + ASCII), read + write
- A stream that seeks is read through a 1 MiB window (`IO::ByteStreamBuffer`, a `std::istream` on it); one that cannot
  is read whole first (`IO::MemoryStreamBuffer`). MDx reads the same way. A 390 MB binary STL peaked at 3274 MiB (the
  file twice) before 2026-11-26, 2532 MiB with the window, **1640 MiB** with the compact vertex (23.4 M of 60 bytes:
  each corner its own vertex, with its facet's normal).
- **`ReadOptions::weldVertices`** (2026-11-26, owner: off by default): the corners sharing a position (bit-identical,
  -0 and +0 being one) become one vertex, through an open-addressing table of indexes at most half full (4 bytes a
  slot, `PositionWelder`). A welded vertex keeps no normal from the file (its triangles' facet normals differ):
  `requestNormal` computes smooth ones, otherwise the vertex normal is the default. Same file: 3.9 M vertices,
  **557 MiB**, but 1.5 s instead of 0.46 s (a hash probe per corner, 23.4 M of them, cache misses). The welded list
  reserves half the triangle count plus 1/16: a closed mesh holds T/2 + 2 per body positions, and at exactly T/2 the
  list doubled (+223 MiB). A triangle whose corners weld together is kept (degenerate, as the file wrote it). Tests
  `VertexFactorySTL.weldingSharesTheCornersOf{ABinary,AnAscii}File`.
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
