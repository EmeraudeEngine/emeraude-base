---
id: rtti-free-observer-payload
title: RTTI-free foundation — replace the std::any Observer payload, arm ASIO_NO_TYPEID
status: open
priority: unranked
scope: src/Observer*, cmake/SetupASIO.cmake, compile policy (EMERAUDE_DISABLE_RTTI)
opened: 2026-09-08
tags: [architecture, cpp, rtti, cascade-wide]
---

# RTTI-free foundation — replace the std::any Observer payload, arm ASIO_NO_TYPEID

## Why

**Owner goal (2026-09-08):** build the whole cascade without RTTI, the way it already builds
without exceptions. The knob exists (`EMERAUDE_DISABLE_RTTI`, `CMakeLists.txt:43`, default
`Off` ⇒ `-frtti` / `/GR`) but flipping it does not compile today. emeraude-base is the first
layer to fix because the Observer contract it owns carries a `std::any` payload that the whole
cascade consumes:

- `src/ObservableTrait.hpp:141` — `notify (int notificationCode, const std::any & data = {})`
- `src/ObserverTrait.hpp:140` — `onNotification (const ObservableTrait *, int, const std::any &)`
- `src/Testing/test_ObserverPattern.cpp:143` — the unit test of the contract

**`std::any` cannot survive `/GR-` on MSVC**: `/GR-` leaves `_CPPRTTI` undefined, the STL sets
`_HAS_STATIC_RTTI` to 0 and `<any>` and `<typeindex>` refuse to compile (STL1003). libstdc++ and
libc++ do compile `std::any` without RTTI, so a Linux-only build would hide the defect. The
payload type must be portable by construction, not by accident.

The second base-owned blocker is **asio**: it only auto-detects Boost's `BOOST_NO_TYPEID`
(`dependencies/asio/asio/include/asio/detail/config.hpp:1187`), never `__GXX_RTTI`. Without
`ASIO_NO_TYPEID`, `detail/service_registry`, `executor.hpp` and `execution/any_executor.hpp`
use `typeid` and the standalone build fails under `-fno-rtti`.

## What remains

- [ ] **Owner decision A/B** (see below) — nothing else in this item starts before it.
- [ ] Add `ASIO_NO_TYPEID` to the PUBLIC definitions of `cmake/SetupASIO.cmake:10`, next to
      `ASIO_STANDALONE ASIO_NO_EXCEPTIONS ASIO_DISABLE_CO_AWAIT`.
- [ ] Implement the chosen payload type and migrate the two trait headers, the `.cpp`, and
      `test_ObserverPattern.cpp` (a test proving cross-type `anyCast` returns null is mandatory,
      the "no fix without test" rule survives the *Ave robustus!* closure).
- [ ] Build emeraude-base alone with `-DEMERAUDE_DISABLE_RTTI=On`, `EmeraudeBaseUnitTests`
      green in Release, from the dedicated git-ignored build dir.
- [ ] Flip the default of `EMERAUDE_DISABLE_RTTI` to `On` **only after** the engine and
      projet-alpha items are closed (`rtti-removal` in emeraude-engine,
      `actor-trait-queries-without-rtti` in projet-alpha); document the flip in
      `docs/integration.md:64` and `docs/error-handling.md`.
- [ ] Docs before deletion: `docs/error-handling.md` gets the RTTI counterpart of its
      exceptions section (what is forbidden: `dynamic_cast`, `typeid`, `std::any`,
      `std::type_index`, `std::function::target()`; what replaces each).

## Owner decision pending — the payload type

- **Option A (recommended):** an RTTI-free `EmEn::Base::Any`, same value semantics as
  `std::any`, whose type identity is a **compile-time hash of the type name** (`__PRETTY_FUNCTION__`
  / `__FUNCSIG__` through `Base::Hash::FNV1a`, already `constexpr` in `src/Hash/FNV1a.hpp`).
  This is the EnTT `type_hash` / ctti technique. It is stable across the DLL/EXE boundary
  because it hashes a string, not an address (the engine is a `SHARED` library, emeraude-base is
  statically duplicated in the DLL and in the executables — a manager-function-pointer identity,
  the libstdc++ trick, breaks on Windows). Migration is mechanical: `std::any` → `Base::Any`,
  `std::any_cast< T >` → `Base::anyCast< T >`, about 50 files across the cascade.
- **Option B:** typed payloads per subject (template the Observer contract, or a `std::variant`
  per observable). Strongest typing, but every observable and observer of the cascade is
  redesigned, not just renamed.
- **Rejected:** keeping `std::any` on GCC/Clang only — Windows cannot compile it under `/GR-`.

## ⚠️ Traps

- **cv/ref normalisation of the requested type.** The engine already casts to
  `const std::shared_ptr< AVConsole::AbstractVirtualDevice >`
  (`emeraude-engine/src/Scenes/Scene.rendering.cpp:1918`): the type key must be computed on
  `std::remove_cvref_t< T >`, or that site silently returns null.
- **Type-name hashing is only stable within one compiler family**, which is the case here (one
  toolchain per build); never persist the hash to disk or send it over the wire.
- **A hash collision aliases two payload types in silence.** FNV-1a 64-bit on full type names is
  the accepted risk; add a debug-only assertion helper if a collision is ever suspected.
- The Observer contract in force does not change: never notify while iterating or holding a
  mutex the handler may take; `onNotification` returning `true` stops propagation. See the
  unrelated item `observable-static-shared-rewrite` before touching the same headers.

## References

- Engine item: `emeraude-engine/docs/todo/rtti-removal.md` (consumer of this payload type).
- projet-alpha item: `projet-alpha/docs/todo/actor-trait-queries-without-rtti.md`.
- asio typeid guards: `dependencies/asio/asio/include/asio/executor.hpp:298`,
  `asio/detail/handler_work.hpp:213`.
- EnTT `type_hash` (MIT) — https://github.com/skypjack/entt — reference for the compile-time
  type-name hash; cite it at the point of use if the technique is reproduced.
