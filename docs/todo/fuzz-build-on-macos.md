---
id: fuzz-build-on-macos
title: build-fuzzers.sh does not run on macOS (bash 3.2, no libFuzzer in AppleClang)
status: in-progress
priority: high
scope: src/Fuzzing/build-fuzzers.sh
opened: 2026-10-18
tags: [fuzzing, tooling, macos, defect]
---

# build-fuzzers.sh does not run on macOS (bash 3.2, no libFuzzer in AppleClang)

## Why
macOS peer, 2026-10-18 (base 692ea8c): the script stops at once — `fuzz_compression: unbound variable` (line 64).
macOS ships bash 3.2.57 as `/bin/bash`: `declare -A` does not exist, the keys of the associative array are read as
arithmetic and `set -u` stops. Past that, AppleClang has no libFuzzer runtime (`libclang_rt.fuzzer_osx.a` not found
for `-fsanitize=fuzzer`). No target can be fuzzed on macOS.

## Done on Linux (2026-10-19)
- The script is portable to bash 3.2: the per-target extra sources through a `case` function, the empty-array
  expansion guarded (`${array[@]+"${array[@]}"}`: before bash 4.4 an empty array under `set -u` is unbound).
- On Darwin the default compiler is `$(brew --prefix llvm)/bin/clang++`, with an explicit error when Homebrew's LLVM is
  missing; `CXX` overrides it. Linux unchanged: the 14 targets build, `fuzz_ini` 555 k runs in 10 s.

## What remains
- Make the script portable to bash 3.2: the per-target extra sources through a `case` statement (or parallel indexed
  arrays) instead of `declare -A`.
- The compiler: Homebrew LLVM (`/opt/homebrew/opt/llvm/bin/clang++`) links and runs a fuzz target; the base library
  must then be built with the SAME compiler and libc++ (an AppleClang-built `libEmeraudeBase.a` mixed with Homebrew's
  libc++ is untested). Either the script builds its own instrumented base sources (it already compiles the `.cpp`
  parsers of each target), or it documents the required toolchain.
- Verify one target builds and runs 60 s on macOS.

## References
- `src/Fuzzing/build-fuzzers.sh`; projet-alpha `docs/unit-testing.md` (fuzzing).
