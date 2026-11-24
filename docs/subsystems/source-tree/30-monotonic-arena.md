## MonotonicArena — a memory arena that only grows, std::pmr where the system has it (2026-11-24)

`src/MonotonicArena.hpp` (header-only, `EmEn::Base::MonotonicArena`, `EmEn::Base::MonotonicBlockArena`,
`EMERAUDE_STD_PMR_AVAILABLE`). Owner decision 2026-11-24: the library's own arena in a reusable header, std::pmr kept
wherever it loads, the fallback deleted the day every supported macOS has std::pmr.

**Why.** The shape decimator kept its per-vertex adjacency lists in a `std::pmr::monotonic_buffer_resource`
(`fd9dc5c`, 2026-10-09: one arena, released in a few frees, instead of millions of node frees). Apple's system libc++
exports std::pmr from **macOS 14.0** only, and its headers carry no availability attribute for it
(`<__configuration/availability.h>`: `_LIBCPP_AVAILABILITY_PMR` is defined empty, "TODO: Enable std::pmr markup once
https://llvm.org/PR40340 has been fixed"). A build for macOS 12 or 13 therefore compiled and linked without a word, and
the dynamic loader refused the library at launch on macOS 13.7: "Symbol not found:
`__ZNSt3__13pmr15memory_resourceD2Ev`" (a downstream application, reported 2026-11-09, measured 2026-11-24: `nm -um`
listed `pmr::memory_resource::~memory_resource()`, `pmr::get_default_resource()` and the vtable of
`pmr::monotonic_buffer_resource`). No crash report and no log: the process never starts. Caution point: § Build /
Compiler, *`std::pmr` on macOS before 14.0*.

**Contract.**
- `MonotonicArena` is `std::pmr::monotonic_buffer_resource` when `EMERAUDE_STD_PMR_AVAILABLE` is 1, and
  `MonotonicBlockArena` otherwise. The library calls only `allocate(bytes, alignment)` (and the destructor) on it, the
  subset both share; `release()` exists on both too.
- `EMERAUDE_STD_PMR_AVAILABLE` is 1 everywhere except on Apple below a deployment target of 14.0
  (`__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 140000`, the floor of libc++'s `_LIBCPP_INTRODUCED_IN_LLVM_16`,
  read in the Xcode 27.2 SDK). A compile definition overrides it: `-DEMERAUDE_STD_PMR_AVAILABLE=0` builds the
  fallback on any platform — the way the whole suite is run against it on Linux.
- With the deployment target at 14.0 or more, the build switches back to std::pmr by itself: nothing to edit.
- `MonotonicBlockArena`: blocks from `::operator new`, NOT zeroed; the first is 1024 bytes by default (a constructor
  argument chooses it), each new one twice the previous, or the request plus its worst alignment padding when that is
  larger. `allocate()` never returns null and gives distinct pointers even for 0 bytes (like std::pmr); a request the
  system cannot serve ends the program, as `operator new` does in the library's `-fno-exceptions` build (and as std::pmr
  did there); a size whose padding would wrap saturates instead of yielding a short block. `release()` frees every
  block, and the next allocation starts again from the initial size. Not thread-safe, neither copyable nor movable (the
  allocations point into it), like the standard resource.
- The decimation is the SAME with either arena: the collapse order follows the insertion order of the lists, never an
  address (the suite's decimator tests pass on both paths).
- **No measurable cost** (2026-11-24, Linux i9-14900K, Release, NOT idle — an ASan suite ran beside; two runs each,
  the disabled measures of `test_VertexFactoryShapeSimplifier.cpp`): on 2.24 M triangles the worst stop latency is
  10.3 / 11.9 ms with `MonotonicBlockArena` against 18.4 / 12.4 ms with std::pmr (the START row in all four runs, as
  before); `DISABLED_ProfileALargeDecimation` 9.50 / 9.25 s against 9.36 / 9.12 s.

**Users.** `VertexFactory::ShapeDecimator`: the adjacency arena of `decimate()`, `ArenaList`, `VertexData`,
`CornerUVTable`.

**Tests.** `src/Testing/test_MonotonicArena.cpp` (5 tests: aligned and distinct allocations, 0 bytes, the geometric
growth and a request larger than the next block, `release()` and the reuse, the selected alias). The fallback is
compiled and tested on every platform; the loader failure itself is only visible on the macOS binary: `nm -um <binary> |
grep pmr` must be empty when the deployment target is below 14.0 (0 lines on the fallback build, 3 on the std::pmr one,
measured on the Linux test binary 2026-11-24). A downstream packager can check it (the application of the report does).

**When it can go.** Once every supported macOS is 14.0 or newer: `docs/todo/drop-the-monotonic-block-arena-at-macos-14.md`.
