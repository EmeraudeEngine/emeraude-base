# Fuzzing — emeraude-base parsers (Ave robustus! A.3)

Coverage-guided fuzzing of the untrusted-input parsers, complementing the manual hidden-throw
audit and the gtest characterization suite. The goal is the A.3 exit criterion: *no parser
exhibits a crash / UB / OOM / hang on hostile input*.

## Engine

**clang libFuzzer** + AddressSanitizer + UndefinedBehaviorSanitizer. Each target is built with
the same `-fno-exceptions` policy the library uses, so a hidden `throw` (e.g. jsoncpp's
`LogicError`) surfaces as a `terminate` the fuzzer catches — exactly as it would at runtime.

## Prerequisites

- `clang++` (tested with Debian clang 19).
- The clang runtimes for libFuzzer + sanitizers: **`libclang-rt-<ver>-dev`** (Debian/Ubuntu).
  Without it, linking `-fsanitize=fuzzer,address` fails with missing `libclang_rt.*`.
  ```
  sudo apt install libclang-rt-19-dev
  ```
- A configured/built emeraude-base build dir for the generated config header and the static
  library — defaults to `.claude-build-debug` (override with `BASE_BUILD_DIR`).

## Design

The library stays a normal g++ build. Each fuzz target is a **standalone clang executable** that:
- pulls in the (mostly header-only) parser code, which clang instruments for coverage + sanitizers;
- links the g++-built `libEmeraudeBase.a` for compiled symbols (`Logging`, `Processor`, `FastJSON`, …)
  and the ext-deps audio/codec chain + jsoncpp.

This avoids recompiling the whole library under clang while still instrumenting the code under test.
`fuzz_midi` additionally compiles the TinySoundFont implementation in-target (the library does not
provide it; the SF2 render path is odr-used by `renderToWave()`).

## Build & run

```sh
fuzzing/build-fuzzers.sh                       # builds every target into fuzzing/build/
ASAN_OPTIONS=detect_leaks=1 fuzzing/build/fuzz_midi fuzzing/build/fuzz_midi.corpus -max_total_time=60
```

Targets:

| Target            | Parser under test                                  |
|-------------------|----------------------------------------------------|
| `fuzz_midi`       | `WaveFactory::FileFormatMIDI` (hand-rolled SMF)     |
| `fuzz_obj`        | `VertexFactory::FileFormatOBJ`                      |
| `fuzz_wav`        | `WaveFactory::FileFormatSNDFile` (libsndfile + our virtual I/O) |
| `fuzz_json_sfx`   | `WaveFactory::FileFormatJSON` (jsoncpp + SFXScript) |
| `fuzz_png`        | `PixelFactory::FileFormatPNG` (libpng + our ByteStream) |
| `fuzz_jpeg`       | `PixelFactory::FileFormatJpeg` (libjpeg-turbo + our ByteStream) |
| `fuzz_targa`      | `PixelFactory::FileFormatTarga` (hand-rolled RLE/raw) |
| `fuzz_native`     | `VertexFactory::FileFormatNative` (ee3d binary)     |
| `fuzz_stl`        | `VertexFactory::FileFormatSTL` (binary + ASCII)     |
| `fuzz_mdx`        | `VertexFactory::FileFormatMDx` (MDL/MD2/MD3/MD5)    |
| `fuzz_compression`| `Compression::LZMA` + `Compression::ZLIB` decompressors |
| `fuzz_ini`        | `INIParser` (via a per-process temp file)           |

A crash writes a `crash-<hash>` reproducer in the run directory; replay it with
`fuzzing/build/<target> crash-<hash>` to get the full sanitizer stack. Reproducers, the `build/`
directory and accumulated corpora are git-ignored (see `.gitignore`).

## Findings

- **FastJSON non-object node → `Json::LogicError` → `terminate`** (fixed): `fuzz_json_sfx`
  reached `FastJSON::getValue`/`getArray`/`getObject` with a top-level JSON array; jsoncpp's
  `isMember()`/`operator[]` throw on a non-object value. Guarded with an `isObject()` short-circuit;
  regression in `test_FastJSON.cpp::nonObjectNodeAccessorsAreSafe`.

### Extended campaign (PixelFactory, VertexFactory, Compression)

Adding the image / geometry / compression targets surfaced nine real defects, every one a
malformed-input crash on a parser the engine reaches at runtime (textures, meshes, compressed
payloads). All are fixed with a regression test (`test_PixelFactoryFileFormats.cpp`,
`test_VertexFactoryFileFormats.cpp`, `test_Compression.cpp`), and each fixed target then fuzzed
clean for millions of runs under ASan + UBSan (`UBSAN_OPTIONS=halt_on_error=1`).

- **PNG `abort()` on a malformed chunk** (`fuzz_png`): libPNG's error path calls `png_longjmp`,
  but `FileFormatPNG` installed a *returning* error callback, so libPNG fell back to `PNG_ABORT()`.
  Fixed with the canonical `setjmp(png_jmpbuf())` pattern (two phases — header then image — so the
  `rowPointers` RAII local never crosses a `setjmp`); the callback now `png_longjmp`s. writeStream
  guarded the same way.
- **JPEG `exit()` on a bad marker** (`fuzz_jpeg`): libjpeg's default `error_exit` calls `exit()`.
  Replaced with a custom `jpeg_error_mgr` whose `error_exit` `longjmp`s into a `setjmp` armed in
  read/writeStream (placed after `jpeg_mem_src` to keep `sourcePtr`/`sourceSize` clear of `-Wclobbered`).
- **Targa OOM** (`fuzz_targa`): 16-bit `width`*`height` allowed a ~17 GB pixmap from an 18-byte
  header. Guard the declared dimensions against the stream payload (max RLE expansion) before
  `Pixmap::initialize`.
- **Targa stack-buffer-overflow** (`fuzz_targa`): `imagePixelSize/8` (up to 31) was read into a
  4-byte stack pixel buffer. Reject any depth outside {8,16,24,32}.
- **MDx MD2/MDL OOB + null-deref** (`fuzz_mdx`): empty frame table indexed `frames[0]`; triangle
  vertex/texcoord/normal indices read straight from the file indexed their tables unchecked. Bounds
  every index; reject empty frame tables.
- **MDx MD3 SEGV + OOM** (`fuzz_mdx`): surface triangle indices indexed the vertex table unchecked,
  and the summed `totalTriangles` drove a ~64 GB `reserveData`. Bound the index and the triangle
  total against the stream; also widened the surface-offset accumulator to 64-bit (signed-overflow UB).
- **MDx MD5 null-deref** (`fuzz_mdx`): a file declaring `numJoints` but omitting the `joints {`
  block left `joints` empty while the build indexed it; weight→joint, vertex→weight and triangle→vertex
  references were unchecked. Derive the joint count from the parsed vector and validate every
  cross-reference before building.
- **LZMA decoder leak** (`fuzz_compression`): `lzma_stream_decoder` state leaked on every
  malformed/truncated input (freed only on success). Wrapped in an RAII guard calling `lzma_end()`.
- **ZLIB OOM** (`fuzz_compression`): an untrusted per-chunk uncompressed-size header drove an
  unbounded `resize`. Validate both chunk sizes against a cap and zlib's max expansion ratio before
  allocating; also corrected the `uncompress()` return-code check (`> 0` → `!= Z_OK`).

> The fuzzers are header-instrumented by clang while linking the g++ `libEmeraudeBase.a`; the
> `setjmp`/`longjmp` error handling for libpng/libjpeg is the sanctioned mechanism under the
> library's `-fno-exceptions` policy. See [`../docs/error-handling.md`](../docs/error-handling.md).