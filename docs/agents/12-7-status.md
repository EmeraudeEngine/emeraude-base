## 7. Status

**Extraction from `emeraude-engine/src/Libs/` is complete.** All foundation code, the
test suite and the test fixtures now live here; `emeraude_platform.hpp` and
`emeraude_base_config.hpp` are the root config headers. The engine consumes
`emeraude::base` and inherits its external dependencies, compile options and language
standards. Build is green; the test suite (`EmeraudeBaseUnitTests`) passes.

Today the library ships as a **single umbrella target** `emeraude::base`
(STATIC by default, SHARED via `EMERAUDE_BASE_LIBRARY_TYPE`), producing `libEmeraudeBase`.
`emeraude::base::platform` is also exposed as a header-only INTERFACE target.

**Planned (internal refactor, no consumer impact):** split the umbrella into per-module
targets so a consumer can link only what it needs:

| Kind | Modules |
|------|---------|
| INTERFACE (header-only) | math, algorithms, animation, platform |
| OBJECT | core, hash, gametools, time, debug, compression, io, network, pixel, vertex, wave |

See [`docs/module-map.md`](../module-map.md) for the full mapping.
