## Algorithms: DiamondSquare

### Overview

The `DiamondSquare` algorithm generates fractal terrain heightmaps. It produces natural-looking landscapes with controllable roughness.

### Key Design: Normalized Output

**Output values are normalized to [-1, 1] range.** This means the `factor` parameter when applying to a Grid represents the actual maximum height displacement in world units (meters).

```cpp
// DiamondSquare generates values in [-1, 1]
// When applied with factor=100, terrain heights will be ±100 meters
grid.applyDiamondSquare({100.0F, 0.5F, 0});  // factor=100m, roughness=0.5
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `size` | `size_t` | Grid dimension (must be 2^n + 1, e.g., 3, 5, 9, 17, 33...) |
| `roughness` | `float` | 0.0-1.0, scale of every level's displacement against the random corner values |
| `hurst` | `float` | per-level decay `2^-H` of the displacement (default 1 = Brownian, the historical relief). ⚠️ At 1 the finest level is white noise at the vertex frequency, which a lit 1 m grid shades as a regular LATTICE; measured on a 4096 × 1 m grid the vertex-frequency curvature goes 0.44 m (1.0) → 0.083 (1.25) → 0.017 (1.5) → 0.004 (1.75) while the 64 m slope only drops 0.25 → 0.15. `DiamondSquareParams::hurst` is declared LAST so a positional `{factor, roughness, seed}` keeps its meaning |
| `normalize` | `bool` | Default `true`. Set to `false` for raw algorithm output |
| `useSameValueForCorner` | `bool` | If `true`, all four corners start with same random value (tileable edges) |
| `seed` | `int32_t` | Optional random seed for reproducible results |

### Usage Example

```cpp
#include "Algorithms/DiamondSquare.hpp"

// Direct usage
Algorithms::DiamondSquare<float> ds(42, true);  // seed=42, same corners
if (ds.generate(129, 0.5F)) {  // 129x129 grid, 0.5 roughness
    float height = ds.value(64, 64);  // Query center point
    // height is in [-1, 1] range
}

// Via VertexFactory Grid (typical usage)
Grid grid(8192.0F, 256);  // 8km terrain, 256 subdivisions
// Streaming a window: ask WHERE it can go before extracting anything — subGridCenter() is the
// single clamp subGrid() applies (snapped to a cell, or to a multiple of `snapCells`, held inside the
// grid). A caller that compares the raw camera position instead regenerates the same window on every
// cycle at the grid border.
// const auto centre = grid.subGridCenter({cameraX, cameraZ}, 4096U, 1024U);
// if ( Math::Vector< 2, float >::distance(centre, heldCentre) > slack ) { auto window = grid.subGrid(centre, 4096U, 1024U); }
// A window's texture coordinates are its PARENT's at the same points (a UV offset carries where it
// starts), and its bounding box is where the window IS (it carried no world offset until 2026-09-22).
// A coarse copy that coincides with the fine grid at every shared point — for a far mesh around the
// window — is grid.coarsened(step): point samples, never an average, or the shared vertices would no
// longer share a height. The OTHER coarse copy — the next level of a height PYRAMID (a clip level,
// a mip) — is grid.halvedTent(): half the cells, each point the 1-2-1 × 1-2-1 tent mean around its
// coincident parent point (edges replicated, even cell count or INVALID). A point-sampled level
// aliases every relief finer than its cell, and so does a normal baked from it; the CDLOD terrain's
// clipmap is built with this one (2026-09-22).
// A HEIGHTMAP IMAGE goes through grid.applyDisplacementMapping(pixmap, factor) (any pixel type, gray
// normalized to 0..1, height = gray x factor), rewritten 2026-09-22 for the `terrain` demo's land003:
// - Catmull-Rom between pixels (separable, border extrapolated linearly): the former COSINE
//   interpolation had a zero slope at every pixel — an egg-crate of one bump per pixel once upsampled;
// - an INTEGER image is first DEQUANTIZED: the smoothest field inside every pixel's rounding interval
//   (coarse-to-fine constrained Jacobi), then two binomial passes (sigma ~1.4 px) round the creases the
//   constraint leaves. 8 bits over 1200 m is a 4.7 m step: terraces on every gentle slope, drawn as
//   contour lines by a per-pixel normal, without it;
// - the bounding box is recomputed (it was left stale, and a CDLOD terrain takes its height range from it).
// ⚠️ updateBoundingBox() (private, unused) merges points at X = Z = 0: never call it as is.
// Tests: src/Testing/test_VertexFactoryGrid.cpp.

grid.applyDiamondSquare({
    .factor = 100.0F,   // heights will be ±100 meters
    .roughness = 0.5F,  // moderate detail against the corner values
    .seed = 7,          // reproducible terrain
    .hurst = 1.25F      // damp the finest levels: no vertex-frequency lattice on a fine grid
});
```

### Why Normalization Matters

Without normalization, the algorithm produces values proportional to grid size:
- 33×33 grid → values roughly ±32
- 16385×16385 grid → values roughly ±16384

This made the `factor` parameter confusing (factor=1.0 on a 16k grid gave ±16km heights!).

**With normalization (default):**
- All grid sizes → values in [-1, 1]
- `factor` directly represents maximum height in world units
- `factor=100` means ±100 meters regardless of grid resolution

**Code reference:**
- `Algorithms/DiamondSquare.hpp:normalizeData()` - Min/max normalization to [-1, 1]
- `Algorithms/DiamondSquare.hpp:generate()` - `normalize` parameter (default `true`)
