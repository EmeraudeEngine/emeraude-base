# Tracy Profiler

emeraude-base owns the [Tracy](https://github.com/wolfpld/tracy) client integration for the whole
project. It is the only place that reaches all four C++ repositories: emeraude-engine, app_kernel
and app_system consume `emeraude::base`, so a single hook here gives every one of them
`<tracy/Tracy.hpp>` and a consistent set of Tracy feature macros — which Tracy requires, since a
mismatch between two compilation units makes the client misbehave.

- Submodule: `dependencies/tracy`, pinned to **v0.14.1**.
- Manual: `dependencies/tracy/manual/tracy.tex` (a built PDF ships with each release).

## Build contract

| Option | Default | Effect |
|---|---|---|
| `EMERAUDE_ENABLE_TRACY` | `Off` | Declares `TRACY_ENABLE` and builds the client as a shared library. |

`cmake/SetupTracy.cmake` translates it into Tracy's own options and attaches the result to
`emeraude_base_flags`, the shared compile-requirements target. That single link reaches both the
per-module `OBJECT` libraries and, through the umbrella's `PUBLIC` link, every consumer of
`emeraude::base`.

**Off** — `TRACY_STATIC=On`, `TRACY_ENABLE` undefined. `TracyClient.cpp` compiles to a handful of
stubs and every Tracy macro expands to nothing, but the headers stay on the include path. This is
what makes instrumentation unconditional: `ZoneScoped` and `FrameMark` need no `#if` guard, and
there is no shared library to ship.

**On** — `TRACY_STATIC=Off`, so the client becomes a shared library. This is mandatory here, not a
preference: `emeraude::base` is linked into `Emeraude` (shared), into `Kernel` (static, itself
linked into the app_system binaries) and into the executables. A static client would place a
distinct profiler instance in each of them, and each instance would announce itself and listen on
its own port. See the manual, *Setup for multi-DLL projects*.

`TRACY_ON_DEMAND=On` is forced with it. Without on-demand, the client buffers every event from
before `main()` until a profiler connects, and that memory is never released — for an application
that runs for hours it means gigabytes of resident memory and a single possible session. On-demand
makes each connection its own capture, at the cost of some bookkeeping per event.

Link-time optimization is refused (`CMAKE_INTERPROCEDURAL_OPTIMIZATION`) while Tracy is enabled:
Tracy silently switches its client to an `OBJECT` library when LTO is active, which reintroduces
the multiple-instance problem the shared library exists to avoid.

## What is instrumented here

`ThreadPool::worker()` names its threads (`tracy::SetThreadName`), so the pool is identifiable in
a capture instead of showing up as anonymous threads. Nothing else — zones are added where the
work being measured lives.

## Adding instrumentation

```cpp
#include <tracy/Tracy.hpp>

void
Something::compute ()
{
    ZoneScoped;               // named after the enclosing function
    ...
}
```

Tracy needs the *begin* and *end* of a zone to fall inside one connection, so a zone straddling a
disconnect is dropped. Its limits are worth knowing before instrumenting a hot loop: at most 65534
unique source locations, a recursive zone no deeper than 255 identical frames, and a session no
longer than 1.6 days.

## Related

- [`module-map.md`](module-map.md) — module → CMake target mapping.
- app_system's `docs/profiling.md` — how to actually profile the Lychee Slicer, whose CEF
  multi-process model makes one client per process.
