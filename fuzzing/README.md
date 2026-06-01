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

A crash writes a `crash-<hash>` reproducer in the run directory; replay it with
`fuzzing/build/<target> crash-<hash>` to get the full sanitizer stack. Reproducers, the `build/`
directory and accumulated corpora are git-ignored (see `.gitignore`).

## Findings

- **FastJSON non-object node → `Json::LogicError` → `terminate`** (fixed): `fuzz_json_sfx`
  reached `FastJSON::getValue`/`getArray`/`getObject` with a top-level JSON array; jsoncpp's
  `isMember()`/`operator[]` throw on a non-object value. Guarded with an `isObject()` short-circuit;
  regression in `test_FastJSON.cpp::nonObjectNodeAccessorsAreSafe`.