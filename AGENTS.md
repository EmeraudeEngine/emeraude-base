# emeraude-base — AI router

The foundation library of the Emeraude project (`EmEn::Base`, C++20, tabs, Linux / macOS / Windows): math,
image / audio / mesh factories, hashing, compression, threading, parsing, I/O — an "STL++" for multimedia
applications. A standalone repository with its own lifecycle: the engine consumes a pinned base, it does not
drive it. Base is also the single source of truth for the external dependencies and the project-wide compile
policy.

> ⚠️ **This file is a ROUTER (2026-09-27).** Everything it used to say was moved VERBATIM to `docs/agents/`
> and every subsystem `AGENTS.md` routes to `docs/subsystems/<area>/`, one file per topic. **Read only the
> file(s) your task needs.** New knowledge goes into the matching `docs/` file — never back into an
> `AGENTS.md`, which only grows by one table row when a docs file is added.

## Non-negotiable rules (detail in the linked file)

- **Agnostic foundation**: nothing high-level (engine subsystem) is ever included here; external libraries come
  through ext-deps-generator only (`EMERAUDE_EXT_LIBS_PATH`); everything depends on base, so test exhaustively.
  → [`docs/agents/07-4-core-axioms.md`](docs/agents/07-4-core-axioms.md)
- **Conventions**: tabs, braces always, UPPERCASE acronyms, no public data members, booleans last, the include
  layout, the library's own types. → [`docs/agents/08-5-conventions.md`](docs/agents/08-5-conventions.md)
- **Third-party symbols never leave the binary that links them** (ELF):
  [`docs/agents/05-3b-third-party-symbols-must-never-leave-the-binary-that-link.md`](docs/agents/05-3b-third-party-symbols-must-never-leave-the-binary-that-link.md).
  **`LC_NUMERIC` is `"C"`** across the cascade: [`docs/agents/06-3c-the-c-numeric-locale-is-a-cascade-invariant.md`](docs/agents/06-3c-the-c-numeric-locale-is-a-cascade-invariant.md).
- **Documentation in the same session, signalled to the user**: [`docs/agents/09-6-documentation-directive.md`](docs/agents/09-6-documentation-directive.md).
  **Open work** in `docs/todo/`, one file per idea: [`docs/agents/11-6c-open-work-docs-todo-one-file-per-idea.md`](docs/agents/11-6c-open-work-docs-todo-one-file-per-idea.md).

## Where to read — by task

| Task touches | Router / file |
|---|---|
| Which CMake target to link, the module map | [`docs/agents/03-3-cmake-architecture-which-target-to-link.md`](docs/agents/03-3-cmake-architecture-which-target-to-link.md) |
| Shared precompiled header | [`docs/agents/04-3a-shared-precompiled-header.md`](docs/agents/04-3a-shared-precompiled-header.md) |
| Source tree, per-module rules and patterns | [`src/AGENTS.md`](src/AGENTS.md) |
| Meshes, parametric geometry | [`src/VertexFactory/AGENTS.md`](src/VertexFactory/AGENTS.md) |
| Audio data | [`src/WaveFactory/AGENTS.md`](src/WaveFactory/AGENTS.md) |
| Unit tests (`EmeraudeBaseUnitTests`) | [`src/Testing/AGENTS.md`](src/Testing/AGENTS.md) |
| Tracy profiler client | [`docs/agents/06b-3d-tracy-profiler-client.md`](docs/agents/06b-3d-tracy-profiler-client.md) |
| Supported image formats (PixelFactory) | [`docs/agents/10-6b-pixelfactory-supported-image-formats.md`](docs/agents/10-6b-pixelfactory-supported-image-formats.md) |
| Pitfalls | [`docs/caution-points.md`](docs/caution-points.md) (search it, do not read it whole) |

## The former content of this file (`docs/agents/`)

| Section | File |
|---|---|
| Identity | [`01-1-identity.md`](docs/agents/01-1-identity.md) |
| Position in the project (dependency chain) | [`02-2-position-in-the-project.md`](docs/agents/02-2-position-in-the-project.md) |
| CMake architecture | [`03-3-cmake-architecture-which-target-to-link.md`](docs/agents/03-3-cmake-architecture-which-target-to-link.md) |
| Shared precompiled header | [`04-3a-shared-precompiled-header.md`](docs/agents/04-3a-shared-precompiled-header.md) |
| Third-party symbol hiding | [`05-3b-third-party-symbols-must-never-leave-the-binary-that-link.md`](docs/agents/05-3b-third-party-symbols-must-never-leave-the-binary-that-link.md) |
| Numeric locale invariant | [`06-3c-the-c-numeric-locale-is-a-cascade-invariant.md`](docs/agents/06-3c-the-c-numeric-locale-is-a-cascade-invariant.md) |
| Tracy profiler client | [`06b-3d-tracy-profiler-client.md`](docs/agents/06b-3d-tracy-profiler-client.md) |
| Core axioms | [`07-4-core-axioms.md`](docs/agents/07-4-core-axioms.md) |
| Conventions | [`08-5-conventions.md`](docs/agents/08-5-conventions.md) |
| Documentation directive | [`09-6-documentation-directive.md`](docs/agents/09-6-documentation-directive.md) |
| PixelFactory image formats | [`10-6b-pixelfactory-supported-image-formats.md`](docs/agents/10-6b-pixelfactory-supported-image-formats.md) |
| Open work rule | [`11-6c-open-work-docs-todo-one-file-per-idea.md`](docs/agents/11-6c-open-work-docs-todo-one-file-per-idea.md) |
| Status | [`12-7-status.md`](docs/agents/12-7-status.md) |
