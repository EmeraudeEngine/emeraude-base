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
