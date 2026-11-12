# Development History Log

## Version 1.6.0 (2026-11-12)
 - **`WaveFactory::Frequency` is a strong type (API change).** Any rate in [1 kHz, 384 kHz] through `Frequency::fromHz()`, the ten standard rates as named constants, read with `hz()`. A 22255 Hz sound now loads; `Processor::resample()` refuses an invalid rate and a ratio outside libsamplerate's bounds.
 - **`URI` keeps its encoded bytes.** `URI::encodedPath()` and `URI::encodedQuery()` re-emit the parsed path and query unchanged, so a presigned URL keeps its signature; `path()` and `query()` are decoded views. `PercentEncoding::encodePreservingEscapes()` escapes only what is illegal.
 - `HTTPRequest::toString()` writes the encoded target with its query (the decoded path went out and the query was dropped), and returns an empty string for an invalid request.
 - **HTTP fields refuse CR/LF and control characters.** `HTTPHeaders::isFieldNameValid()` and `isFieldValueValid()`; `HTTPHeaders::add()` and `set()` return a `[[nodiscard]] bool` (API change); `HTTPResponse::setTextResponse()` returns a `bool`; a received response with an invalid field or reason phrase is refused. `HTTPHeaders::parse()` names a refused start line instead of "the HTTP header".
 - HTTPS: `HTTPSClientOptions::totalTimeout` is one deadline that caps every transport step (`TLSConnection::setDeadline()`); TLS 1.2 is the floor of every connection; certificate revocation is documented as not supported.
 - Sockets private to the process: `Network::preventSocketInheritance()` and `preventHandleInheritance()` (close-on-exec on POSIX, non-inheritable on Windows), applied to the `HTTPServer` acceptor, its connections and every Happy Eyeballs attempt. A child process no longer keeps a server's port after the application ends.
 - **No direct stream access in the base.** Every diagnostic goes through the `Logging` sink (333 sites migrated, libtiff and libjpeg messages routed); `Logging::setSink()` returns the sink it replaces. A timer overrun (`PrintScopeRealTimeThreshold`, `PrintScopeCPUTimeThreshold`) is a Warning; DEBUG-only lines are Debug.
 - Test suite: a guard sink fails the running test on any diagnostic outside a capture (`Testing::DiagnosticsCapture`, `Testing::captureDiagnostics()`); a test never prints, its measurements go to `RecordProperty()`.
 - Fuzzing: new target `fuzz_uri`; `build-fuzzers.sh` accepts a cascade build directory.

## Version 1.5.4 (2026-11-04)
 - **`Base::ExactKey`: the exact identity of a cached object.** The byte sequence of every input, compared byte for byte; the hash only places it. `add(const char *)` is deleted. `ExactKey::reset(domain)` rebuilds a key without releasing its storage.
 - `IO::systemErrorText()`: an error code's text in UTF-8 on every OS (MSVC's `std::error_code::message()` answers in the ANSI code page). IO, ZipWriter, HTTPServer, TLSConnection and TrustStore use it.
 - PixelFactory: `Pixmap::markEverythingUpdated()` now resets the top of the marker; a partial upload no longer misses the rows above the last rectangle.
 - CMake: the macOS deployment target follows `APP_OSX_DEPLOYMENT_TARGET` when a consumer sets it, 13.0 by default (a fixed 12.0 before).
 - Docs: `EMERAUDE_ENABLE_PCH` is Off by default, as `CMakeLists.txt` already had it (the documentation said On).

## Version 1.5.3 (2026-10-29)
 - **`Math::Matrix::noTranslation()` returned the transpose.** Fixed: `ShapeAssembler::merge(shape, transform)` turned the normals of a shape merged at 90 or 270 degrees the wrong way, and its triangle-mesh collisions were inside-out.
 - **OpenMP removed from the base.** `EMERAUDE_ENABLE_OPENMP` and `SetupOpenMP.cmake` are gone and libgomp is no longer linked; the `Shape` computations are serial (one thread team per calling thread, a data race and an out-of-bounds write in `generateTextureCoordinates()`).
 - **Astronomy.** `Math/Astronomy.hpp`: a planet-generic Keplerian sky (the star, the moons, the sidereal rotation, the celestial pole), a planet's name and distance to its star, lunar photometry. `Math/SolarSystem.hpp`: Julian day and calendar date, Delta T, sidereal time, obliquity, precession from J2000, the planets, the Sun, the Moon and `topocentric()`; the planets' size, colour, magnitudes, IAU poles and prime meridians.
 - WaveFactory: a DLS to SoundFont 2 converter (`DLS::convertToSoundFont()`), and an RMID plays its embedded bank. Stoppable decode (`ReadOptions::stopToken`) and a streamed resample (`Processor::resample(frequency, stopToken)`).
 - MIDI: the additive renderer's polyphony is bounded to 256 voices; both renderers are stoppable.
 - `FlatHashMap::reserve(count, stopToken)`: an interruptible reserve; the decimator's tables stop within the bound.
 - `VertexFactory::Grid`: `applyFractalNoise(FractalNoiseParams)` (seeded fBm) and `applyEdgeFalloff(EdgeFalloffParams)`.
 - Math: compound inertia and a quaternion from a 3x3 matrix.
 - ShapeDecimator: the border between two groups is anchored like a UV seam.
 - PixelFactory: a TrueType font kerns its pairs and `TextProcessor` applies them; `Pixmap::averageColor()` accumulates in double.
 - include-what-you-use mapping fixed for Apple libc++'s private headers and glibc's `<features.h>`.

## Version 1.5.2 (2026-10-21)
 - `PixelFactory::TextureName`: parses the `<TextureName>.<Purpose>` naming of GPU textures; every refusal is a `TextureNameError`.
 - **Mip filter.** `Processor::downsample()` takes a `ReductionFilter` (Box, or Kaiser as in NVIDIA Texture Tools) and an sRGB flag (colour filtered in linear light); new `ReductionKernel`; the intermediate is a ring of filtered rows instead of a whole double image.
 - `Processor::fillTransparentColour()`: pull-push dilation of the colour under transparent pixels.
 - `Pixmap::copyChannel()` and `Processor::resampleTexture()` (texel centres aligned, wrap or clamp, separable Catmull-Rom).
 - External dependencies v019 (rgbcx BC4 / BC5 in bc7enc_rdo, fastgltf and tinyusdz fuzzing fixes). An extracted folder gets a `.version` stamp, and a folder from another release is fetched again.

## Version 1.5.1 (2026-10-19)
 - **`Base::Thread` on the `std::jthread` model (API change).** A body invocable with a `std::stop_token` receives the token of its start; `requestStop()`, `stopToken()`; the destructor and the move assignment request the stop, then join; `detach()` is removed.
 - **`Base::TaskGroup` and pool stop semantics (API change).** A `TaskGroup` owns N tasks with one stop source (`ThreadPool::submit(group, callable)`); a `TaskHandle` or `TaskGroup` task stopped before it starts never runs; a stop removes the owner's queued tasks from the pool's queue. `ShapeDecimator::setCancellationFlag()` is removed.
 - `Base::ScopedRegistration`, a move-only registration token. `EventTrait::createTimer()` and `fire()` return a `[[nodiscard]] TimerHandle` whose end stops and joins the timer (API change).
 - **Thread-safety analysis.** `ThreadSafety.hpp` (Clang attributes such as `EMEN_GUARDED_BY`), `Base::Mutex` and `Base::MutexLock`, `Base::ConditionVariable`, `Base::SharedMutex`, `Base::MutexPairLock` (two mutexes without a lock-order deadlock). The base's own mutexes are annotated; the observer queries are locked (a data race).
 - **Sanitizer switches (API change).** `EMERAUDE_ENABLE_ASAN`, `EMERAUDE_ENABLE_LSAN`, `EMERAUDE_ENABLE_TSAN`, `EMERAUDE_ENABLE_UBSAN` and `EMERAUDE_ENABLE_MSAN` instrument the whole cascade through `EMERAUDE_COMPILE_OPTIONS` and `emeraude_base_flags`; `EMERAUDE_ENABLE_SANITIZERS` is removed.
 - `Hash::HashType` is a scoped `enum class` (API change); the hash classes are usable by name.
 - IO: every reader refuses what is not a regular file (`IO::isReadableRegularFile()`: a FIFO blocked a read forever); every writer refuses an existing path that is not a regular file (`IO::isWritableTarget()`, `IO::openOutputFile()`); file writers write aside.
 - File readers hardened against hostile data: PNG, JPEG and TIFF dimensions bounded, every TGA 2.0 variant decoded, MDL / MD3 read per their format, ZLIB and LZMA outputs bounded (65 MiB decoder limit), ZipReader sizes bounded; `Pixmap` refuses an element count beyond uint32; a triangle index outside its table and non-finite geometry refuse the file.
 - ZipWriter: an archive is written whole or not at all, from file sources; a duplicate entry name is refused.
 - PixelFactory: the channel conversions keep the alpha; font glyphs keep their aspect and the text advances in pixels.
 - Network: `HTTPRequest`'s Host field is the resource's authority, port included, and follows it; a strict response status line.
 - Synthesis and MIDI: effect parameters kept in their domains, a saturated output, a 30-minute ceiling for both MIDI renderers; `SFXScript` refuses a parameter of the wrong type.
 - `Variant` prints an 8-bit integer as a number; the LZMA stream classes are reusable; `ShapeSplitter` keeps the holes of a sealed cut.
 - Static analysis: cppcheck 2.22.0 and include-what-you-use configured and swept; a template-instantiation guard compiles every header and class template (19 never-compiled members fixed, among them `ShapeProcessor::generateUVUnwrap()` and `Vector::refract()`); `.cpp`-instantiated specializations are declared `extern template`; clang-tidy with four more checks and `WarningsAsErrors '*'`; MSVC /analyze findings fixed. The fuzzers build on macOS.
 - Windows paths: `BlobTrait` writes a path in UTF-8; `String::isValidUTF8()`; the CA bundle is read through IO.

## Version 1.5.0 (2026-10-11)
 - **`Base::Any` replaces `std::any` in the Observer payload (API change).** `ObservableTrait::notify()` and `ObserverTrait::onNotification()` carry a `const Base::Any &`, read with `anyCast()` / `anyValue()`; no RTTI needed.
 - **`Base::Thread`: a thread started without throwing.** `[[nodiscard]] bool start(callable)`; `ThreadPool`, `TimedEvent` and `HTTPServer` start through it. A self-join aborts in every build.
 - **Owned pool tasks (API change).** `ThreadPool::submit(callable)` returns a move-only `TaskHandle`; the task receives a `std::stop_token`; `ShapeDecimator::setStopToken()`.
 - `PortableRandom`: seeded numbers identical on every platform (`uniformInteger()`, `uniformReal()`, `shuffle()`); `Randomizer`, `PerlinNoise` and `VoronoiNoise` draw through it, so every seeded result changes once.
 - `FlatHashMap`: open addressing, one allocation, the same layout on every platform; the decimator runs on flat arrays (the same decimation on every OS) with double-precision quadrics.
 - **Collision foundation.** Contact manifolds (oriented box, box-box, sphere-box, capsule-box, box-triangle, triangle and round shapes); linear shape casts (ray, sphere, capsule); convex distance (GJK) and box casts; `Space3D::TriangleMesh` static collision meshes with a SAH hierarchy and active edges; rigid-body helpers; `Math::PiecewiseLinear`.
 - **Network.** `Network::HTTPServer` (limits, timeouts, smuggling refusals, Host / Origin / bearer checks, Range, SSE); `HTTPSClientOptions::allowPrivateCleartext`; `HTTPRequestOptions::cancel`; connection keep-alive (`HTTPSClientOptions::reuseConnections`, on by default); Happy Eyeballs (`Network/HappyEyeballs.hpp`, `connectFirstReachable()`); `GracefulCloser`; `URI::redacted()`; `Hash::hmacSha256()`; the SHA-1/256/512 length field fixed.
 - **Windows paths.** Strict `String::utf8ToUTF16()` and `utf16ToUTF8()`; every `IO::` wrapper hands the system the `\\?\` form from 248 characters (`IO::systemPath()`, `IO::windowsExtendedLengthPath()`); `IO::renameFile()`; a directory walk returns entries in the caller's form.
 - **VertexFactory.** Secondary texture coordinates (native format version 3); `simplifyShape()` through meshoptimizer, group by group (the base links meshoptimizer); a shape read back from its vertex buffers; a final shape releases its merge indexes; `memoryOccupied()`; OBJ reads one vertex per (v, vt, vn) triple; the tangent frame follows the UV winding; `Shape::transform()` no longer zeroes the bitangents; decimated UVs no longer fold over.
 - Math: `Vector` and `Quaternion` scalar division follow IEEE 754 (a tiny non-zero divisor no longer gives NaN); `Vector::normalize()` at any scale; `Quaternion::toAngleAxis()` exact for small rotations; `CartesianFrame`'s pitch, yaw and roll are real ZYX Tait-Bryan angles (behaviour change).
 - `Skin::setRootTransform()`; `IO::fileGetRange()`; `processCPUTimeResolutionNanoseconds()`.
 - TrueType fonts load (glyphs on a baseline, size capped at 256 px).
 - No `.at()` left in the base: inconsistent shapes are refused.
 - Build: external dependencies v018 (hwloc 3.0; tinyusdz renamed LightUSD, linked as `lightusd::lightusd_static`); `EMERAUDE_THIRD_PARTY_COMPILE_OPTIONS` for vendored sources; no D9025 in a standalone MSVC build; the macOS SDK sysroot is set only by the top-level project; the paranoid warning set at zero on GCC, Clang, AppleClang and MSVC. New fuzz target `fuzz_hdr`.

## Before 1.5.0
 - The history before 1.5.0 is in the git log.
