## Critical Attention Points

- **Engine foundation**: everything depends on Base, critical stability
- **Zero high-level dependencies**: Base must NEVER include engine headers (Scenes/Physics/Graphics/etc.)
- **Agnostic**: Generic code, not specific to any use case
- **Uniformity**: Math and PixelFactory types must be used EVERYWHERE — `Vector< 3, float >` for directions/positions, `Color<>` for colors, `Matrix` for transforms, `CartesianFrame` for coordinate systems. Never use raw `float x, y, z` or `float r, g, b` in structs, members, or API signatures when an engine type exists. The only exception is GPU push constant structs that require exact POD memory layout.
- **Thread-safe**: Consider thread-safety for shared utilities
- **Performance**: Critical code (used everywhere), optimize if necessary
- **Documentation**: Document well, many systems depend on Base
- **Exhaustive tests**: a bug in Base affects the entire engine and every standalone consumer
