## VertexFactory: Grid Terrain Queries

### Edge Clamping Behavior

`Grid::getHeightAt()` and `Grid::getNormalAt()` clamp out-of-bounds coordinates to the terrain edge instead of returning default values. This ensures smooth physics behavior at terrain boundaries.

**Why this matters:**
- Physics queries at scene boundaries (outside terrain grid) now return the nearest edge height/normal
- Prevents entities from falling through "phantom ground" at Y=0 outside terrain
- Ensures consistent terrain behavior for collision detection

**Code references:**
- `VertexFactory/Grid.hpp:getHeightAt()` - Clamps coordinates with epsilon margin before interpolation
- `VertexFactory/Grid.hpp:getNormalAt()` - Same clamping behavior for normal queries

**Implementation detail:**
```cpp
constexpr auto epsilon = static_cast<vertex_data_t>(0.0001);
const auto clampedX = std::clamp(positionX, -m_halfSquaredSize + epsilon, m_halfSquaredSize - epsilon);
const auto clampedY = std::clamp(positionY, -m_halfSquaredSize + epsilon, m_halfSquaredSize - epsilon);
```

### The rendered triangles of a region (`Grid::forEachTriangleInRegion()`, 2026-10-01)

What the physics collides with (engine physics overhaul P2): each cell overlapped by an XZ region gives two triangles,
split along its bottom-left → top-right diagonal — exactly `VertexGridResource`'s triangle strip (TL, BL, TR) + (BL, BR,
TR) — in `position()`'s frame (the world offset included), each wound with an UPWARD normal. The callback receives
(a, b, c, cellX, cellZ, half) and is taken by const reference (called once per triangle).
- ⚠️ Not the surface `getHeightAt()` answers: that one interpolates the cell bilinearly (a curved patch).
- ⚠️ `getHeightAt()` / `getNormalAt()` ignore `m_worldOffset` while `position()` adds it: equal for every grid
  with a zero offset (the grounds today), to reconcile before an offset grid is used for physics.
- A region outside the grid, inverted or non-finite visits nothing — no flat extension beyond the edge (unlike
  `getHeightAt()`'s clamp). Tests: `VertexFactoryGrid.Triangles*` (3).

### Fractal noise and edge falloff — an archipelago (2026-10-26)

`Grid::applyFractalNoise(FractalNoiseParams)` sums `octaves` of Perlin noise, each `lacunarity` times finer and `gain`
times weaker (fBm — F. K. Musgrave, "Texturing & Modeling: A Procedural Approach", 3rd ed., 2003, ch. 16), sampled at
each point's WORLD position (`wavelength` = the first octave's, in world units): a feature keeps its size whatever the
grid's size and division. Seeded (`seed` → `PerlinNoise(seed)`, the same relief on every platform); each octave reads
its own z slice (octave + 0.5), so the octaves are uncorrelated and the origin is not a zero of all of them. The sum
is divided by the octaves' total amplitude, then × `factor` + `offset` (a negative offset sinks it: only the highest
features emerge).
`Grid::applyEdgeFalloff(EdgeFalloffParams)` blends every height toward `edgeHeight` with smoothstep(`start`, 1, d), d
the "square bump" 1 − (1 − nx²)(1 − nz²) (A. Patel, Red Blob Games, "Making maps with noise functions" § Islands): 0 at
the centre, 1 on the WHOLE square border (a radial distance leaves the corners up) — the border is `edgeHeight`
exactly, the centre untouched.
Both refuse (false, heights untouched) a non-finite or out-of-range parameter and an empty grid, and leave the bounding
box tight (`updateBoundingBox()`). Tests: `VertexFactoryGrid.AFractalNoise*` (3), `*InvalidFractalNoise*`,
`*EdgeFalloff*` (2).
- ⚠️ **The normalised fBm is far from filling [-1, 1]**: Perlin noise rarely approaches its bounds and the octaves
  average out — WaterWorld's archipelago measured a maximum of about 0.39 × factor in the interior (factor 60 / offset
  −15 peaked at 8 m). Tune `factor` against the measured range, never against ±factor.
- ⚠️ `PerlinNoise::generate()` answers in **[0, 1]** (`(result + 1) / 2`), not [-1, 1]: `applyFractalNoise()` recentres
  it; the older `applyPerlinNoise()` does NOT (its doc said [-1, 1]) — a Perlin ground made with it never goes below
  0 × factor. Left as is: changing it would reshape every existing Perlin ground.
