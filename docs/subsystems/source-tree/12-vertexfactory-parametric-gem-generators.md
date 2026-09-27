## VertexFactory: Parametric Gem Generators

### Overview

`ShapeGenerator.hpp` contains 12 parametric gemstone generators plus all standard primitives. Each gem generator is a template function producing a `Shape` with flat-shaded triangles, volumetric vertex colors, and per-face UV mapping.

### Generator Functions

| Function | Cut Type | Ring Vertices | Key Parameters |
|----------|----------|---------------|----------------|
| `generateDiamondCutGem()` | Brilliant round | N (circular) | radius, depth, tableRatio, segments |
| `generateEmeraldCutGem()` | Step-cut rectangular | 8 (octagonal) | length, width, depth, cornerBevel, steps |
| `generateAsscherCutGem()` | Step-cut square | 8 (octagonal) | Wrapper around emeraldCutGem with length=width |
| `generateBaguetteCutGem()` | Step-cut rectangular | 4 (rectangular) | length, width, depth, steps |
| `generatePrincessCutGem()` | Modified brilliant square | 8 (square+midpoints) | size, depth, chevronDepth |
| `generateTrillionCutGem()` | Triangular brilliant | 6 (beveled triangle) | size, depth, cornerBevel, steps |
| `generateOvalCutGem()` | Elliptical brilliant | N (elliptical) | length, width, depth, segments |
| `generateCushionCutGem()` | Superellipse brilliant | N (superellipse) | length, width, depth, power, segments |
| `generateMarquiseCutGem()` | Pointed elliptical | N (marquise) | length, width, depth, sharpness, segments |
| `generatePearCutGem()` | Teardrop | N (asymmetric) | length, width, depth, sharpness, segments |
| `generateHeartCutGem()` | Heart-shaped | N (parametric heart) | size, depth, segments |
| `generateRoseCutGem()` | Domed top, flat base | N (circular) | radius, height, rings, facets |

### Common Architecture

All gem generators follow the same pattern:
1. `ShapeBuilder` with `ConstructionMode::Triangles`
2. `emitTriangle` lambda that **swaps B/C** for winding convention (emits A, C, B)
3. `computeFaceUV` lambda with **world-Y projected tangent frame** for consistent UV orientation
4. Volumetric vertex color: `(pos * invExtent + 1) * 0.5` mapping position to RGBA
5. `endConstruction()` auto-computes tangent space

### UV Mapping (`computeFaceUV`)

Per-face tangent-frame UV projection with two key properties:
- **Consistent orientation**: Tangent direction derived from world Y-axis projected onto face plane (falls back to X-axis for horizontal faces). Prevents random texture rotation between adjacent faces.
- **Aspect-ratio preserving**: Single `invScale = 1 / max(rangeU, rangeV)` prevents stretching.

### Winding Conventions

See: [`docs/coordinate-system.md`](../../../../docs/coordinate-system.md#winding-conventions-for-parametric-geometry) for crown/pavilion/table/culet patterns.

### Code References
- `VertexFactory/ShapeGenerator.hpp:generateDiamondCutGem()` — Reference brilliant-cut implementation
- `VertexFactory/ShapeGenerator.hpp:generateEmeraldCutGem()` — Reference step-cut implementation
- `VertexFactory/ShapeGenerator.hpp:generateRoseCutGem()` — Dome geometry (pavilion-style, dome points +Y)
- `Graphics/Geometry/ResourceGenerator.hpp` (emeraude-engine repository) — GPU resource wrappers for all gem generators
