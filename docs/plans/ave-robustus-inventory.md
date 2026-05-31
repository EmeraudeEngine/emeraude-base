# "Ave robustus!" — Phase 0 Intent Contract (inventory)

> Companion to [`ave-robustus.md`](ave-robustus.md). This is the **promised-vs-delivered**
> inventory per module — the deliverable of Phase 0. Built 2026-05-31 from a parallel
> read-only audit of all 15 modules. Gap legend: **A** = present-but-fragile · **B** =
> promised-but-missing · **none** = complete AND tested.
>
> Owner adjudicates the **B** gaps case by case (see §"Open decisions" at the bottom and
> the parent plan §4).

## Severity tiers (where the risk concentrates)

| Tier | Modules | Why |
|------|---------|-----|
| **1 — untrusted-input parsing** | network, vertex, wave, io, compression, pixel, core (FastJSON/KVParser) | Hostile/malformed input reaches these; 0 or happy-path-only tests. The robustness front line. |
| **2 — untested logic** | math (spline/OBB peripherals), algorithms, animation, time, hash, gametools, debug | Implemented, plausible, but unverified; several carry concrete latent bugs. |
| **3 — done-ish** | platform | CMake split done; only minor doc/compiler-detection gaps. |

## Cross-cutting findings (beyond the seed list)

These emerged from the audit and matter for sequencing:

1. **`std::abort()` under `-fno-exceptions` (Network).** ASIO's throw path is overridden to
   print + `std::abort()` — hostile/unreachable input *kills the process* instead of failing
   gracefully. Direct conflict with the A.0 error contract.
2. **IO permission docs are stale lies.** `IO.hpp` carries 3× `@todo Implement Windows
   permission checking` + `@warning Windows always returns true`, but `IO.cpp` *fully
   implements* the Windows `GetFileSecurityW`+`AccessCheck` path. → This is a **doc defect
   (close by correcting the claim)**, NOT a missing feature. Corrects seed finding #2.
3. **Real correctness bugs found** (not mere fragility) — see the per-module "🐞" rows:
   `Time::Elapsed::CPUTime` unit error; `Time::EventTrait::resetTimer` calls non-existent
   `resetTop()`; `Debug::Statistics` ns timer ignores `tv_sec`; `KVParser::getLineType`
   misclassifies values with `#`/`[`/`@`; `ZLIB::compressString` passes level into chunkSize;
   `LZMA::decompressString` leaks on error; `Query::operator<<` emits spurious entries;
   `Hash::Types` enum advertises SHA1 (no impl) and omits SHA512 (implemented);
   `Animation` CubicSpline mode undeliverable (no tangent storage); `PerlinNoise::grad()`
   tool-corrupted parentheses.
4. **No round-trip test anywhere in PixelFactory** — every codec test asserts only
   "write succeeded", never reads back to compare pixels. JPEG read path: 0 tests.
5. **Optional deps not optional (Wave).** libsamplerate + TinySoundFont are included/called
   unconditionally (no `#ifdef` guard), unlike sndfile — the build hard-requires them.

---

## Tier 1 — untrusted-input parsing

### Module: network
- **Target/kind/deps:** `emeraude::base::network` — OBJECT, dep ASIO (standalone, exceptions disabled). Status TODO.
- **Test files:** none.
- **Overall:** A thin hand-rolled URI/HTTP toolkit functional only on well-formed friendly input. Naive parsing, no percent-encoding, toy synchronous HTTP/1.0 GET, no TLS, no redirects, nothing tested. The most fragile module in the library.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| Parse generic URI `[scheme://user@host:port/path?query#frag]` | `URI::parseRawString` | `String::explode` splits, no RFC 3986 grammar, no percent-decode, no IPv6 `[::1]`, no validation | A | `http://[::1]:8080/`, percent-encoded path, `:`-in-path all parse correctly or reject cleanly |
| Validate scheme per RFC | `URI.hpp:93` TODO | Not implemented; any garbage becomes a "scheme" | A | `123://x` / `foo bar://x` rejected |
| Distinguish simple path vs URI | `URI::checkSimplePath` | Heuristic: any `:` in a segment flips path→URI (`time:12:00` misparsed) | A | path with `:` stays a path |
| Host → subdomain/domain/TLD | `Hostname::fromString` | Heuristic "label > 3 chars = domain"; breaks on `co.uk`, `github.io`, IPs | A | `www.example.co.uk`, `127.0.0.1` split correctly |
| Port / userinfo / credentials | `URIDomain::extract*` | Hardcoded `switch` on chunk counts; `>3` `:` silently kept raw; port 0 overloaded | A | malformed authority `a:b:c:d@h` rejected, not partially kept |
| Query string → key/values | `Query::fromString` | No percent-decode, no `+`→space; 🐞 `operator<<` pre-sizes then appends → spurious empty `=` entries | A + 🐞 | round-trip query has no stray entries; `a=%20` decoded |
| Parse HTTP req/resp headers | `HTTPHeaders::parse` | Works on clean input; any bad line fails whole parse; no size limits (header bombs) | A | oversized/folded header handled; bomb rejected |
| `download(uri,file)` | `Network.cpp:57` | Synchronous; **no TLS**, **redirects unhandled** (`continue`s), needs Content-Length, resolves by scheme name | A | HTTPS / redirecting / chunked URL works or fails cleanly |
| `hasInternetConnexion()` | `Network.cpp:46` | DNS-resolve only (no connect); ASIO failure → `std::abort()` | A | captive portal not reported "connected"; resolver failure returns false, not abort |
| Exception-free operation | `asio_throw_exception.hpp` | ASIO throw path → `std::abort()` (process death) | A | internal ASIO error returns an error, never aborts |
| `URL` class | `URL.hpp` | `@deprecated` thin shim over URI, inherits all fragility | A | — |

### Module: vertex
- **Target/kind/deps:** `emeraude::base::vertex` — OBJECT, no ext deps. Status TODO. ~22k LOC, header-only (1 `.cpp`).
- **Test files:** none.
- **Overall:** Broad read paths (ee3d/OBJ/STL/MDx), real write for ee3d/OBJ/STL, intentional read-only MDx; large procedural ShapeGenerator suite. Entire surface untested; MDx reader trusts header counts/offsets without bounds checks.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| Read geometry by extension (ee3d/obj/stl/mdl/md2/md3/md5mesh) | `FileIO.hpp` read() | Delivered, per-format dispatch | A | each format reads expected vertex/triangle counts |
| Write geometry by extension | `FileIO.hpp` write() | ee3d/obj/stl real; MDx→false | A (ee3d/obj/stl) | round-trip write→read reproduces shape |
| Native ee3d read+write (magic/version/precision guards) | `FileFormatNative.hpp` | Delivered with guards | A | byte-exact ee3d round-trip |
| StreamIO (in-memory) — native only | `StreamIO.hpp:51` | Hardwired to ee3d; **owner ruled: full parity required** | **B** | obj/stl/native all round-trip in-memory |
| MDx read (MDL/MD2/MD3 binary + MD5 text) | `FileFormatMDx.hpp` | 🐞 allocates vectors from untrusted `header.num_*`, seeks untrusted `offset_*`, **no size validation** → OOM/OOB on malformed input | A | truncated/hostile MD2/MD3 fails gracefully, no OOM/OOB/crash |
| MDx write | `FileFormatMDx::writeStream` | Intentional stub (returns false, logged) | none — **owner ruled: assumed limitation, document it** | writeStream returns false, stream untouched |
| STL ASCII/binary auto-detect | `FileFormatSTL::isAscii` | Heuristic; binary file starting with "solid" misdetected | A | binary STL with "solid" in header parsed as binary |
| OBJ read+write | `FileFormatOBJ.hpp` | Delivered; write has no vt/vn dedup (per-vertex 1:1) | A | OBJ write→read reproduces pos/UV/normal; opens in a viewer |
| Procedural shapes (~40 generators) | `ShapeGenerator.hpp` | Large, apparently complete | A | sphere/cuboid have valid manifold topology + correct counts |
| Mesh decimation (QEM) | `ShapeDecimator.hpp` | Implemented | A | decimate to N% reduces tris ~N%, preserves silhouette |
| Shape processing (hole fill, split, assemble, X-ray, triangles) | `ShapeProcessor/Splitter/Assembler`, `TriangleGenerator.hpp:87` FIXME, `ShapeBuilder.hpp:636` FIXME | Implemented + 2 open FIXMEs | A | hole-fill closes a known holed mesh; X-ray count matches |
| Heightfield grid | `Grid.hpp` | Implemented | A | grid res yields (res+1)² verts, correct heights |

### Module: wave
- **Target/kind/deps:** `emeraude::base::wave` — OBJECT, deps sndfile/samplerate/TinySoundFont. Status TODO. ~7.4k LOC, mostly header-only.
- **Test files:** none.
- **Overall:** Feature-rich (50+ DSP ops, full MIDI parser + SF2 renderer), but the entire untrusted-input surface (libsndfile virtual I/O, 1720-line MIDI parser, JSON SFX) has 0 tests.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| Read WAV/FLAC/OGG via libsndfile | `FileFormatSNDFile.hpp` | `int16_t` specialization only; generic template stubbed false; behind `#ifdef LIBSNDFILE_ENABLED` | A | malformed/truncated WAV/OGG fails gracefully |
| Write WAV/FLAC/OGG (File+Stream IO) | `FileFormatSNDFile.hpp:346` | `int16_t` only; 🐞 `sf_writef_short` return ignored (no short-write check) | A | write→read round-trip; partial-write detected |
| FileIO vs StreamIO parity | `StreamIO.hpp:50` | StreamIO libsndfile-only (no JSON/MIDI in-memory) — **owner ruled: full parity required** | **B** | in-memory MIDI/JSON decode works |
| Resampling (libsamplerate SINC_BEST) | `Processor.cpp` | Delivered; ⚠️ `<samplerate.h>` included **unconditionally** (no `#ifdef`) → build breaks if dep absent | A | resample ratio-edge correctness; build with dep off |
| MIDI parse (Fmt 0/1, running status, VLQ, tempo, RMID) | `FileFormatMIDI.hpp` (1720 L) | Delivered, read-only; SMPTE rejected; EOF guards | A | truncated/adversarial `.mid` degrades gracefully |
| SF2 render via TinySoundFont (256 voices, CC, pitch, pan) | `FileFormatMIDI.hpp` | Delivered when soundfont provided; ⚠️ tsf calls not `#ifdef`-guarded | A | SF2 render produces expected sample length |
| MIDI Reverb/Chorus + poly key pressure | AGENTS.md Limitations | Promised in CC table then retracted (TSF limitation) | **B** | — |
| JSON procedural SFX (`SFXScript`) | `SFXScript.hpp` | Delivered, read-only (`FileFormatJSON::writeStream`→false) | A | malformed/hostile JSON SFX rejected cleanly |
| Synthesizer (50+ generators/FX, mono) | `Synthesizer.hpp` (2593 L) | Delivered header-only | A | filter coeffs / ADSR / pitchShift numerically correct |
| Processor (trim/crop/pad/concat/mix/analysis/normalize/bit-depth) | `Processor.cpp` | Delivered (only compiled TU); 28 untested `return false` paths | A | channel/sample-range edge cases handled |
| `dataConversion<In,Out>` int16↔float | `Wave.hpp` | Delivered | A | normalization/clamping correct across conversions |
| PCM freq/channel enums | `Types.hpp` | mono/stereo + fixed rate set; non-standard rate → Invalid | A | contract on unsupported rate pinned by a test |

### Module: io
- **Target/kind/deps:** `emeraude::base::io` — OBJECT, dep libzip. Status TODO.
- **Test files:** none.
- **Overall:** Broad, real, complete implementations (incl. a fully-coded Windows AccessCheck path) — but zero tests, and header docs lie about the Windows permission behavior.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| `ByteStream` abstraction (shared by factories) | `ByteStream.hpp` | Clean interface; never unit-tested | A | round-trip bytes through a stream |
| `FileStream` file-backed | `FileStream.hpp` | Implemented, single-mode; untested | A | failed-open / partial-read / seek-OOR behavior verified |
| `MemoryStream` grow-on-write + seek | `MemoryStream.hpp` | Implemented; 🐞 possible `size_t` overflow in `m_position+size` bounds check | A | overflow/short-read/negative-seek edge cases |
| File utilities (exists/size/create/erase/dir/get/put/u8path) | `IO.cpp` | Complete, `noexcept`, error_code | A | UTF-8 / Windows-path handling verified |
| `readable/writable/executable` cross-platform | `IO.hpp:194-237` | **Code complete incl. Windows AccessCheck**; docs falsely claim a Windows stub → **doc defect, close by correcting the claim** (corrects seed #2) | A (untested) + doc-fix | all platform branches tested; docs match code |
| ZIP read (open/entries/extract/extractAll) | `ZipReader.cpp` | Complete; 🐞 `extract(...,buffer)` ignores `zip_fopen` return → null-deref; empty entries fail | A | corrupt/empty-entry handled; no null-deref |
| ZIP write (addFile/addDir/create) | `ZipWriter.cpp` | Complete; loose source validation; fails if archive exists | A | malformed-source / overwrite / large archive |

### Module: compression
- **Target/kind/deps:** `emeraude::base::compression` — OBJECT, deps ZLIB/LZMA. Status TODO.
- **Test files:** `test_Compression.cpp` (happy-path round-trips only).
- **Overall:** All four paths implemented + happy-path tested. Gaps are entirely on the bad-input axis; the "not supported" cerr lines are legit libzma error mappings (corrects seed fact), NOT stubs.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| ZLIB string round-trip | `ZLIB.hpp` | Works on valid data | A | corrupt/truncated decompress path tested |
| ZLIB stream (chunked) | `ZLIB.hpp` | 🐞 `compressString` passes `level` into `chunkSize` param (works by luck); raw `size_t` chunk headers (non-portable 32/64/endian); always cerr "stream seems broken" unless sentinel | A + 🐞 | direct stream test; portable framing |
| LZMA single-shot string | `LZMA.cpp` | Implemented; 🐞 `lzma_end` not called on early-error returns → leak on bad input | A + 🐞 | corrupt/truncated/empty input, no leak |
| LZMA multithreaded Compressor/Decompressor | `LZMA/*.hpp` | Implemented; MT + non-MT round-trips tested; suspicious `min/max` thread clamp | A | thread-count clamp + error branches |
| Graceful corrupt/truncated/empty handling | all `decompress*` | Error paths exist but **no test drives them** | A | the headline safety property is verified |

### Module: pixel
- **Target/kind/deps:** `emeraude::base::pixel` — OBJECT, deps PNG/JPEG/Freetype. Status TODO. Header-only.
- **Test files:** `test_PixelFactoryColor/Pixmap/PixmapFormat/TextPixmap/Processor` (1559 L).
- **Overall:** Broad mature surface, core processing ops well-tested — but no round-trip pixel verification anywhere, JPEG read untested, colored stencil stubbed, JPEG RGBA rejected, Targa round-trip tests disabled.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| Read PNG/JPEG/Targa | `FileIO.hpp` read() | All 3 implemented; tests read PNG + Targa; **JPEG read 0 tests** | A | JPEG decode path exercised |
| Write PNG | `FileFormatPNG.hpp` | Implemented; tests **never read back** (assert write only) | A | round-trip pixel equality |
| Write JPEG | `FileFormatJpeg.hpp:187` | Implemented; RGBA explicitly rejected; no write test | A/B | RGBA→jpg contract; write tested |
| Write Targa | `FileFormatTarga.hpp` | Raw+RLE; GrayscaleAlpha→false; RLE/round-trip/grayscale tests **disabled** | A | disabled tests re-enabled, round-trip verified |
| In-memory codec (StreamIO) | `StreamIO.hpp` | Implemented all 3; **0 tests reference it** | A | memory encode/decode round-trip |
| Malformed/truncated image robustness | decoders | Return false on bad header; **no corrupt-input test** | A | corrupt/truncated/empty fails gracefully |
| Pixel processing ops | `Processor.hpp` | Implemented + **well-tested** (32 cases) | none | — |
| Stencil copy (colored source) | `Processor.hpp:1008` | �quad **stub: `// TODO...; return false`**; pixmap-source variant has `FIXME` mask check | **B** | colored stencil actually composites |
| Text rendering (Freetype) | `Font/TextProcessor` | Loads TTF + pixmap fonts; 1 smoke test, no content assert; ASCII-only | A | glyph position/metrics asserted; non-ASCII limit documented |
| Color handling | `Color.hpp` (59k) | Large API; test covers only `ColorFromInteger` | A | conversion/gradient/sRGB/blend tested |
| Procedural gen (noise/gradient) | `Processor` | Implemented; tested by write-success only | A | pixel-value assertions |
| Performance (SIMD/parallel/block-copy) | `Processor.hpp` TODOs | Scalar only; explicit unfulfilled TODOs (block-copy "first attempt failed") | **B** → A.5 | throughput improved + measured |

### Module: core (flat src root)
- **Target/kind/deps:** `emeraude::base::core` — OBJECT (FastJSON/Variant transitively pull jsoncpp + Math + Color). Status TODO.
- **Test files:** `test_String/ThreadPool/TokenFormatter/ObserverPattern/NodeTrait/Version/StaticVector`.
- **Overall:** Tested cluster solid. Concern: both untrusted-input parsers (FastJSON, KVParser) and the 20-type Variant ship with 0 tests; KVParser has a real classification bug.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| Fast JSON parse + typed extraction | `FastJSON.hpp/.cpp` | Robust *by config* (jsoncpp strict, exceptions off, optional returns); **0 tests** | A | untrusted/malformed JSON accept/reject pinned |
| INI-style KV config (sections, `#`,`@`) | `KVParser.hpp/.cpp` | 🐞 `getLineType` returns on first special char → value with `#`/`[`/`@` misclassified, key dropped; 0 tests | A + 🐞 | config with `#`/`[`/`@` in values keeps keys |
| `KVVariable` typed conversion | `KVParser.hpp` | Delivered; untested | A | bool/int/float/empty edge cases |
| 20-type Variant (exception-free `as*`) | `Variant.hpp/.cpp` | `std::get_if`-based, type/index invariant; **0 tests** | A | type/index invariant + mismatch paths |
| Source code annotation/formatting | `SourceCodeParser.hpp/.cpp` | Delivered; 0 tests | A | comment stripping / line math on edge input |
| Cross-platform file timestamps | `FileTimestamps.hpp/.cpp` | Delivered w/ platform caveats; 0 tests | A | platform-conditional fetch paths |
| String utilities | `String.hpp/.cpp` | **Tested**; TODO `pad` char-only = enhancement | none | — |
| ThreadPool / TokenFormatter / Observer / NodeTrait / Version | resp. files | **Tested** | none | — |
| Flag traits (`FlagTrait`, `FlagArrayTrait`) | resp. `.hpp` | Header-only; **0 tests** | A | bit-manipulation correctness |
| Misc traits (`Nameable/Blob/Randomizer/BaseUtility`) | resp. `.hpp` | Present; **0 tests** | A | widely-used helpers verified |
| `std::source_location` shim | `std_source_location.hpp:51` | **Deprecated** (intentional sunset) | none | — |

---

## Tier 2 — untested logic

### Module: math
- **Target/kind/deps:** `emeraude::base::math` — INTERFACE, no deps. Status TODO. ~21k LOC.
- **Test files:** `test_MathBasics/CartesianFrame/Matrix/Quaternion/Space2D/Space3D/TransformConversions/Vector` + `test_LineFormula`.
- **Overall:** Core linear-algebra + Space2D/3D collision stack genuinely complete and densely tested. Gaps concentrate in spline/curve + OBB peripherals (0 tests) + a FIXME in OrientedCuboid.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| Vector/Matrix/Quaternion/CartesianFrame/TRS | src/AGENTS.md, headers | Delivered + densely tested | none | round-trip identities (covered) |
| Space2D/3D primitives + collisions + MTV | src/AGENTS.md | Delivered, very dense coverage | none | MTV exactly separates known overlap (covered) |
| Line-vs-primitive intersections (3D) | `Space3D/Intersections/Line*.hpp` | Delivered; 8× `@todo` "duplicate of segment, check dup code" | A (dedup) | infinite-line hit beyond endpoints ≠ segment clone |
| BezierCurve | `BezierCurve.hpp` (doc names `Bezier.hpp`) | Implemented; **0 tests**; doc/file name mismatch | A | sample known control polygon vs hand-computed |
| BSpline | `BSpline.hpp` (526 L) | Implemented; **0 tests** (most complex untested header) | A | evaluate vs reference incl. knot continuity |
| OrientedCuboid (OBB) + SAT | `OrientedCuboid.hpp:392` | Implemented; **0 tests**; 🐞 `FIXME` w/h/d stored independent of vertices (desync on transform) | A + 🐞 | rotate OBB → w/h/d + SAT stay consistent with vertices |
| Plane / Range / DeterminantAverage | resp. `.hpp` | Implemented; 0 dedicated tests | A | signed-distance sign; Range min/max accumulation |
| LineFormula (2D) | `test_LineFormula.cpp` | Delivered; **only 1 test case** | A | vertical/horizontal/arbitrary lines |

### Module: algorithms
- **Target/kind/deps:** `emeraude::base::algorithms` — INTERFACE, no deps (uses Math/Randomizer/Pixmap). Status TODO.
- **Test files:** none.
- **Overall:** Five algorithms fully implemented, plausibly correct, entirely unverified; PerlinNoise has a code-health red flag.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| Perlin noise (3D, seeded) | `PerlinNoise.hpp` | Delivered; 🐞 `grad()` (L205) tool-corrupted nested parens (readability hazard); 0 tests | A | noise in [0,1], continuous; grad transcription correct |
| Diamond-Square heightmap | `DiamondSquare.hpp` | Delivered; size=2^n+1 validation; 0 tests | A | size validation, normalization range, corner seeding |
| Constrained Delaunay (Bowyer-Watson) | `DelaunayTriangulation.hpp` | Delivered, numerically delicate; 0 tests | A | degenerate/collinear inputs (highest-risk to ship blind) |
| Voronoi noise F1/F2 + caustic | `VoronoiNoise.hpp` | Delivered; 0 tests; absent from src/AGENTS.md (doc gap) | A | distance ordering + caustic clamping |
| Mandelbrot → Pixmap | `Mandelbrot.hpp` | Delivered; 0 tests; absent from doc list | A | output asserted; Pixmap integration |

### Module: animation
- **Target/kind/deps:** `emeraude::base::animation` — INTERFACE, no deps. Status TODO. **Data-only by design** (src/AGENTS.md:61: no runtime playback — evaluated engine-side).
- **Test files:** none.
- **Overall:** As a pure-data + MD5-loader layer, essentially complete; sampling is by-design out of scope. One genuine internal gap: CubicSpline.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| Joint POD (bind pose, parent, inverse-bind) | `Joint.hpp` | Delivered; 0 tests | A | `NoParent` sentinel / defaults pinned |
| Skeleton (lookup, roots, `isValid`) | `Skeleton.hpp` | Delivered; `joint(index)` unchecked (UB on bad index); 0 tests | A | isValid/findJoint/rootJoints verified |
| AnimationChannel + interpolation modes | `AnimationChannel.hpp` | Step/Linear OK; **CubicSpline undeliverable** (no tangent storage) | A + **B** (CubicSpline) | consumer selecting CubicSpline can honor it |
| AnimationClip (auto duration) | `AnimationClip.hpp` | Delivered; `channel(index)` unchecked; 0 tests | A | duration aggregation verified |
| Skin (JOINTS_0 remap, inverse-bind) | `Skin.hpp` | Delivered; no size-consistency check vs doc; 0 tests | A | jointIndices/inverseBind size match enforced |
| MD5AnimParser (.md5anim → clip) | `MD5AnimParser.hpp` | Full id-Tech parser; potential OOB read on flag-indexed `frameData`; 0 tests, no fixture | A | hostile/truncated `.md5anim` no OOB |
| Runtime sampling / pose eval | (disclaimed) | Intentionally engine-side | none | out of scope by design |

### Module: hash
- **Target/kind/deps:** `emeraude::base::hash` — OBJECT, no deps. Status TODO.
- **Test files:** `test_Hash.cpp` (md5/sha256/sha512, one custom vector each).
- **Overall:** Algorithms fully implemented; only 3 of 6 surfaces tested, with non-standard single inputs. CRC32/FNV1a/Types untested; enum advertises a non-existent SHA1 and omits SHA512.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| `md5` | `Hash.cpp:58` | Implemented; tested w/ one custom vector | A | RFC-1321 KATs (empty, `"abc"`) |
| `sha256` | `Hash.cpp:70` | Implemented; one custom vector | A | FIPS-180-4 KATs |
| `sha512` | `Hash.cpp:82` | Implemented; one custom vector; not in enum | A | FIPS-180-4 KATs |
| `crc32` (IEEE/zlib/PNG) | `CRC32.cpp` | Implemented standard reflected poly; **0 tests** | A | CRC32("123456789")=0xCBF43926 |
| FNV1a constexpr + `_hash` UDL | `FNV1a.hpp` | Implemented; **0 tests** | A | FNV reference vector; constexpr asserted |
| `HashType` enum + conversions | `Types.hpp` | 🐞 enum has SHA1 (no impl), lacks SHA512 (impl exists); helpers untested | **B** + A | round-trip `to_HashType(to_string(x))==x`; enum matches impls |

### Module: time
- **Target/kind/deps:** `emeraude::base::time` — OBJECT, no deps. Status TODO. (root + `Elapsed/` + `Statistics/`)
- **Test files:** none.
- **Overall:** Broad and feature-complete on paper, 0 coverage, and carries two concrete latent bugs.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| Uptime / UNIX-timestamp helpers | `Time.hpp:44-93` | Implemented; doc says "nanoseconds" but returns µs | A | monotonic, ordered s≤ms≤µs; doc fixed |
| `Elapsed::{CPUTime,RealTime}` | `Elapsed/*` | 🐞 CPUTime stores `clock()` ticks as ns → wrong by CLOCKS_PER_SEC | A + 🐞 | known sleep/busy-loop yields plausible duration |
| RAII scope timers + threshold-print | `ScopeRealTime/ScopeCPUTime/PrintScope*` | Implemented; 0 tests | A | scope accumulates into referenced duration |
| `TimedEvent` threaded timer | `TimedEvent.hpp:51` | `std::thread`/`condition_variable`; non-trivial concurrency; 0 tests | A | callback fires after granularity; once self-stops; clean join |
| `EventTrait` timer registry | `EventTrait.hpp:365` | 🐞 `resetTimer` calls non-existent `resetTop()` → won't compile if instantiated | **B** (broken) | `resetTimer` compiles + works |
| `Statistics::{CPUTime,RealTime}` (rolling avg, EPS) | `Statistics/*` | Implemented ring-buffer; 0 tests | A | known sequence → asserted avg/EPS |
| `Precision` enum / `TimerID` | `Types.hpp` | Defined; 0 tests | A | Precision switch branches in PrintScope |

### Module: gametools
- **Target/kind/deps:** `emeraude::base::gametools` — OBJECT, no deps. Status TODO.
- **Test files:** none.
- **Overall:** Complete self-contained card/dice utilities; untested; minor edge-case sloppiness.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| Dice roll [1,faces], coin-flip (min 2) | `Dice.hpp` | `mt19937`; double-seed (member init + ctor reset) | A | range bounds, 2-face floor, distribution |
| Deck/hands, shuffle deck & discard | `CardDeck.cpp` | Delivered | A | shuffle/reset exercised |
| Pick Top/Bottom/Random from deck/discard | `CardDeck::pick` | Delivered with guards | A | picked card moves deck→hand; Random math |
| Release/discard card to pile | `CardDeck::insert` | Delivered with guards | A | card-not-in-hand rejection |
| `CardHand::shuffle()` | `CardHand.hpp` | 🐞 fresh `mt19937` per call (wasteful, weaker) | A | per-call reseed flagged |

### Module: debug
- **Target/kind/deps:** `emeraude::base::debug` — OBJECT, no deps. Status TODO.
- **Test files:** none.
- **Overall:** Three dev-only diagnostics; functional but side-effect-only (hard to unit-test); timer is platform-gapped + buggy.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| Concurrent-entry detector (RAII) | `ConcurrencyDetector.hpp` | Logs to cerr only, never signals programmatically | A | detection assertable (needs a flag/return) |
| Trace SMF calls + live count | `Dummy.hpp` | All 5 SMFs + `<=>` + `s_instanceCount`; cout-only | A | `s_instanceCount` asserted |
| Nanosecond CPU timer | `Statistics.cpp` | 🐞 **Linux-only** (link errors elsewhere); `tv_nsec`-only subtraction wrong across second boundary | **B** + 🐞 | cross-platform; correct across 1s boundary |

---

## Tier 3 — done-ish

### Module: platform
- **Target/kind/deps:** `emeraude::base::platform` — INTERFACE. **Status DONE** (CMake split done).
- **Test files:** none.
- **Overall:** Pure-preprocessor OS/arch detection + constexpr mirror; robust for the mainstream matrix with hard `#error` fallbacks. "DONE" = the split, not coverage; no compiler-identity detection.

| Promised capability | Source | Real state | Gap | "Utility blinded" criterion |
|---|---|---|---|---|
| Arch detection (x86 32/64, ARM 32/64) | `emeraude_platform.hpp:53-87` | Delivered + `#error` fallback | A | non-host branches compiled in a build matrix |
| OS detection (Linux/Win/macOS) | `:89-106` | Delivered + `#error` fallback | A | non-host OS branch exercised |
| Target string + MSVC func-sig shim | `:108-113` | Delivered | A | MSVC shim validated on non-Linux |
| `constexpr` mirror of macros | `:116-131` | Delivered, well-justified | A | `static_assert IsLinux==bool(IS_LINUX)` invariant |
| Compiler identity (GCC/Clang/MSVC + version) | implied | **Missing** (no `__GNUC__`/`__clang__`) | **B** | `PLATFORM_COMPILER`/version available |

---

## Owner decisions — RESOLVED 2026-05-31

These were surfaced by the audit and have been ruled by the owner:

1. **Network scope → PRODUCTION-GRADE.** Full RFC 3986 URI parsing + robust HTTP**S** client
   (TLS, redirects, chunked), graceful no-abort errors. ⚠️ Adds a **TLS external dependency**
   via `ext-deps-generator` — largest item in the plan. (Parent plan §4.)
2. **`std::abort()` under `-fno-exceptions` → FORBIDDEN on runtime/input errors.** Must
   propagate gracefully; `abort`/`assert` only for programmer-contract violations, Debug-only.
   Codified in the A.0 error contract. The Network ASIO abort path must be replaced.
3. **New `B` gaps → ALL ruled real-intent, to finish:** Pixel colored-stencil; Hash enum
   (expose SHA512, resolve SHA1); Animation CubicSpline (add tangent storage); platform
   compiler-identity; Debug cross-platform ns timer. (See parent plan §4.)
4. **Correctness bugs (🐞) → compile-breakers fixed up front (A.0), rest folded into A.2/A.3.**
   Per the **No-fix-without-a-test** rule (parent plan §0), every one of these — including the
   up-front compile-breakers — ships with its regression test.
