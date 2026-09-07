## 3d. Tracy profiler client

Base owns the [Tracy](https://github.com/wolfpld/tracy) client (submodule `dependencies/tracy`,
v0.14.1) for the same reason it owns the Setup scripts and the PCH: it is the only place reached
by every consumer, and Tracy demands one consistent set of feature macros across the whole build.

- **`EMERAUDE_ENABLE_TRACY`** (default `Off`) — `cmake/SetupTracy.cmake` attaches Tracy to
  `emeraude_base_flags`, so the per-module `OBJECT` libraries and every consumer of
  `emeraude::base` (engine, app_kernel, app_system) get `<tracy/Tracy.hpp>` from one hook.
- **Off = headers only.** `TRACY_ENABLE` is undefined, every macro expands to nothing, the client
  is a static stub. Instrumentation therefore needs **no `#if` guard** — write `ZoneScoped`
  unconditionally.
- **On = shared client, mandatory.** `emeraude::base` ends up in `Emeraude` (shared), in `Kernel`
  (static) and in the executables; a static client would put a separate profiler instance in each,
  every one announcing itself on its own port. `TRACY_ON_DEMAND` comes with it, and LTO is refused.

Full contract, limits and how to add a zone: [`docs/tracy-profiler.md`](../tracy-profiler.md).
