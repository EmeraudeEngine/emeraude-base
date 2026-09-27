## Base-Specific Rules

### Philosophy: Critical Agnostic Foundation
- **Engine foundation**: every engine system (Graphics, Physics, Audio, Scenes) builds on Base
- **Agnostic**: NO dependencies on high-level systems (Scenes, Physics, Graphics, etc.)
- **Uniformity**: Provide common types and concepts for the entire engine
- **Reusable**: Generic code, not specific to any use case

### Architecture by Domain

**Algorithms/** - Useful multimedia algorithms
- Simple implementations of classic algorithms
- Optimized for real-time usage
- **DiamondSquare**: Procedural terrain heightmap generation (see below)
- **DelaunayTriangulation**: Constrained Delaunay 2D triangulation with boundary polygon filtering (Bowyer-Watson + ray-casting interior test)
- **WorleyNoise** (Sep 2026): TILEABLE 3D cellular noise (F1) with an ARBITRARY period and a
  cross-platform deterministic integer hash ("lowbias32", C. Wellons, public domain), plus
  `generateBillows()` (fractal `1 - F1`, the cumulus billows of Schneider & Vos 2015). Written for the
  engine's volumetric clouds (their 32³ detail texture must tile, their shapes must grow identically on
  every platform). ⚠️ **Not a duplicate of `VoronoiNoise`, but an overlap to consolidate** (owner
  question, open): `VoronoiNoise` gives F1+F2 over a FIXED 256-cell permutation period and seeds it
  through `std::default_random_engine` + `std::ranges::shuffle`, whose sequence is
  implementation-defined — the same seed is a different pattern on MSVC and libstdc++. Tests
  `AlgorithmsWorleyNoise.*` (period, billow period, 1-Lipschitz, range, seed determinism).

**Compression/** - Compression/decompression abstraction
- Standardized data compression logic
- Wrappers: zlib, lzma
- Common interface for all algorithms

**Debug/** - Stats and development tools
- Profiling, timings, statistics
- Debug helpers

**GamesTools/** - Game utility classes
- Gameplay-specific helpers
- Generic game concepts

**Hash/** - Hashing algorithms
- MD5, SHA-1, SHA-256, SHA-512, CRC-32 (IEEE 802.3 / zlib / PNG), FNV-1a
- For checksums, identifiers, caching
- All cryptographic-style classes share the same API: `update(bytes, length)`, `final(digest)`, `reset()`. Free helpers in `Hash.hpp` (`md5/sha1/sha256/sha512/crc32(string)`) return lowercase hex strings. SHA-1 is cryptographically broken — legacy interop / checksums only.

**IO/** - Generic I/O abstractions (shared foundation for all factories)
- **ByteStream**: Abstract polymorphic interface for byte-level I/O (`read/write/seek/tell/size/isOpen`)
- **FileStream**: File-backed implementation (`Mode::Read/Write`) wrapping `std::ifstream`/`std::ofstream`
- **MemoryStream**: Memory-backed with random-access writes (position-based, not append-only)
- Archive support (ZIP via external lib)
- File/folder manipulation
- See: `IO/ByteStream.hpp`, `IO/FileStream.hpp`, `IO/MemoryStream.hpp`

**Animation/** - Skeletal animation data types (header-only)
- **Joint**: Joint struct (name, parentIndex, localT/R/S, inverseBindMatrix). Namespace: `EmEn::Base::Animation`
- **Skeleton**: Ordered joint collection with name lookup and hierarchy validation (topological ordering)
- **AnimationChannel**: Keyframes for ONE target (`VectorKeyFrame` for T/S, `QuaternionKeyFrame` for R), 3 interpolation modes (Step, Linear, CubicSpline), `ChannelTarget` enum, and the sampling itself — `sampleVector(t)` / `sampleQuaternion(t)`.
  ⚠️ The field is **`targetIndex`**, renamed from `jointIndex` (Aug 2026): a clip is target-AGNOSTIC. A skeletal clip indexes a `Skeleton`'s joints, a NODE clip indexes an imported hierarchy's nodes — the engine has an evaluator for each. Producer and consumer must agree on which structure it indexes; nothing in the type can check it.
  ⚠️ The sampling lives on the CHANNEL, not in an evaluator, precisely because there are now two consumers: a second copy is a second place for the CubicSpline stride and the tangent scaling to drift.
- **AnimationClip**: Named collection of channels, duration auto-computed, skeleton-independent (joints referenced by index)
- **Skin**: Mesh-to-skeleton binding (joint index remapping, inverse bind matrices, GLTF JOINTS_0 indirection)
- Pure data types — no runtime playback. Consumed by `src/Animations/` for runtime evaluation and by loaders (GLTF, MD5)

**Math/** - Complete 2D/3D math library
- **Vector**: 2D/3D/4D vectors
- **Matrix**: Transformation matrices
- **Quaternion**: 3D rotations (includes `toRotationMatrix4()` for standard column-major output)
- **CartesianFrame**: Coordinate system (position + orthonormal basis), `fromQuaternion()` factory, `toQuaternion()` extractor

> [!CRITICAL]
> **`CartesianFrame` stores `m_upward` (Y+) and `m_backward` (Z+). Read the PARAMETER, not the name
> you remember.** The `m_downward` → `m_upward` rename of the Y-up migration (53 sites) left a trail
> of parameters, locals and comments still saying "downward" while feeding the UP axis. Cleaned up
> Aug 2026; if you meet one again, it is stale, not a hint.
>
> **Three accessors, and they are NOT interchangeable any more:**
> - `localYAxis()` → `m_upward` — the Y **basis column**, no up/down meaning. Use it for anything
>   structural: composing `(right, Y, backward)`, mirroring a stored axis, a gizmo handle, a local-Y
>   translation.
> - `upwardVector()` → `m_upward`, `downwardVector()` → `m_upward.inversed()`. Reserve these for
>   callers that genuinely mean gravity's direction or its opposite.
>
> ⚠️ Its doc used to claim `localYAxis()` was "the same value as `downwardVector()` today, the two
> are about to swap". **The swap happened**: they are now OPPOSITE. Believing that note inverts the
> axis silently.
>
> ⚠️ **Fixed Aug 2026 — `setBackwardVector(x, y, z)` called ITSELF** (missing braces around the three
> scalars): infinite recursion, stack overflow at the first real call. It shipped that way and never
> crashed only because nothing in the whole cascade ever called that overload. A defect no test can
> reach is still a defect.
- **TransformUtils**: `TRSDecomposition<T>`, `decomposeTRS(Matrix4)`, `composeTRS(T,R,S)` — roundtrip-safe TRS matrix decomposition
- **Primitives**: Point, Line, Segment, Sphere, Capsule, Triangle, AACuboid
- **Collision/Intersection**: Geometric detection between primitives
- **Bezier curves**: Smooth interpolation
- All current and future 2D/3D math logic

**Network/** - The HTTPS/TLS client stack (production, Linux + macOS + Windows verified 2026-08-28)
- `HTTPSClient` — blocking, redirect-following HTTPS/1.1 over LibreSSL. Three entry points:
  `get()`/`head()`, `download()` (streams to a file, progress hook), and `request()` (arbitrary
  method, caller headers, request body — the **API-traffic** path, added 2026-08-28)
- `TLSConnection` (transport + proxy CONNECT), `TrustStore` (system anchors + optional CA bundle),
  `HTTPResponseParser` (chunked, read-until-close, hardened ceilings), `URI`/`URIDomain` (validated
  host, no CRLF injection), `HTTPRequest`/`HTTPResponse`/`HTTPHeaders`, `PercentEncoding`, `Query`
- ⚠️ **HTTPS only** — plaintext `http://` is refused by decision; the legacy `Network::download()`
  was removed 2026-08-27
- ⚠️ `HTTPSClient::isRequestHeaderAcceptable()` gates every caller-supplied header: the request is
  built by concatenation, so a CR/LF in a value is a header-injection primitive
- Engine consumers: `EmEn::Net::Manager` (downloads) and `EmEn::Net::APIClient` (web APIs)

**PixelFactory/** - Image manipulation (uses unified ByteStream I/O)
- Load/save image formats (JPEG, PNG, Targa) via `FileIO`/`StreamIO`
- Pixel transformations (resize, crop, filters)
- Procedural image generation
- **TextProcessor**: Text rendering on Pixmap with bounds protection
- **Pixmap**: Image container with `blendPixel()` (assert) and `blendFreePixel()` (bounds-safe)
- See: `PixelFactory/FileIO.hpp`, `PixelFactory/StreamIO.hpp`

**VertexFactory/** - 3D geometry manipulation (uses unified ByteStream I/O) - See [`@VertexFactory/AGENTS.md`](../../../src/VertexFactory/AGENTS.md)
- Procedural mesh generation
- Geometric transformations
- Normal, tangent, UV calculations
- **Grid**: Terrain height/normal queries with edge clamping (see below)
- **Shape**: Core geometry container with `BoundaryLoop` support, `surfaceArea()`, `volume()`, `isSurfaceOpened()`. Does NOT carry skeletal data (moved to `ShapeLoadResult` and `SkeletalDataTrait`).
- **ShapeLoadResult**: Bundles `Shape` + `optional<Skeleton>` + `optional<Skin>` — returned by all `FileFormatInterface::readStream()` implementations. See `VertexFactory/ShapeLoadResult.hpp`.
- **ShapeProcessor**: Geometry analysis and modification (vertex dedup, boundary loop detection, ear-clipping sealing)
- **ShapeSplitter**: Plane-based geometry splitting with optional integrated cap sealing (`sealCut` option)
- Format handlers: Native (ee3d), OBJ, STL, MDx (MDL/MD2/MD3/MD5)
- MDx formats are read-only (no write support). MD5 loader builds `Skeleton`, `Skin`, and vertex influences/weights (top-4 by bias with renormalization) via `ShapeLoadResult`
- **MD5 loader uses direct Shape population** (NOT ShapeBuilder). Two-pass approach matching id Tech 4 `Model_md5.cpp`:
  - Pass 1: Create all vertices (one per MD5 vert, shared between triangles) with position, UV, bone influences
  - Pass 2: Create triangles with MD5 indices + per-mesh vertex offset, reverse winding (2,1,0)
  - TBN computed via `computeTriangleTBNSpace()` + `computeVertexTBNSpace()` on shared vertices (smooth normals)
  - Normals negated after computation to compensate coordinate conversion reflection (det=-1 of (y,-z,x))
  - `declareTextureCoordinatesAvailable()` required (direct population bypasses auto-declaration)

> [!WARNING]
> **ShapeBuilder is for procedural shape construction only** (gem cuts, parametric geometry).
> File format loaders (MD5, OBJ, etc.) should populate the Shape directly — ShapeBuilder's
> data economy (vertex deduplication) reorders vertices, which breaks any external index
> mapping (bone influences, shared vertex references, etc.).
- See: `VertexFactory/FileIO.hpp`, `VertexFactory/StreamIO.hpp`

**WaveFactory/** - Audio manipulation (uses unified ByteStream I/O) - See [`@WaveFactory/AGENTS.md`](../../../src/WaveFactory/AGENTS.md)
- Audio format loading (WAV, FLAC, OGG via libsndfile + `sf_open_virtual`)
- Procedural audio from JSON definitions (mono Synthesizer, multi-track via SFXScript)
- MIDI to synthesized audio conversion
- Audio sample transformations
- See: `WaveFactory/FileIO.hpp`, `WaveFactory/StreamIO.hpp`

**Time/** - Temporal management
- Chronometers
- Timers
- Timing helpers via objects and interfaces

**General concepts (`src/` root)**:
- **Observer/Observable**: Event pattern
- **Versioning**: Version management
- **JSON**: Fast JSON parsing/writing
- **Flags**: Binary flag management
- **Traits**: Helpers (NamingTrait, etc.)
- **Strings**: String manipulation
- **TokenFormatter**: Case style detection and conversion (camelCase, snake_case, PascalCase, etc.)
- **ThreadPool**: High-performance thread pool (`enqueue` + data-parallel `parallelFor`, per-index & range overloads)
- **INIParser**: INI-style file parsing (sections, key-values)
- **SourceCodeParser**: Source code parsing with annotations and formatting

### Integrated External Dependencies
- **libsndfile**: Audio format loading via `sf_open_virtual` (WaveFactory)
- **libsamplerate**: Audio resampling (WaveFactory/Processor)
- **TinySoundFont**: SF2 MIDI rendering (WaveFactory/FileFormatMIDI)
- **zlib**: Compression (Compression)
- **lzma**: Compression (Compression)
- **ZIP library**: Archives (IO)
