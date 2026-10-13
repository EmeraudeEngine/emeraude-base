---
id: rtti-free-observer-payload
title: RTTI-free foundation — replace the std::any Observer payload, arm ASIO_NO_TYPEID
status: blocked
priority: low
scope: src/Observer*, cmake/SetupASIO.cmake, compile policy (EMERAUDE_DISABLE_RTTI)
opened: 2026-09-08
blocked-by: [rtti-removal, actor-trait-queries-without-rtti]
tags: [architecture, cpp, rtti, cascade-wide]
---

# RTTI-free foundation — replace the std::any Observer payload, arm ASIO_NO_TYPEID

## Why

**Owner goal (2026-09-08):** build the whole cascade without RTTI, the way it already builds
without exceptions. The knob exists (`EMERAUDE_DISABLE_RTTI`, `CMakeLists.txt:53`, default
`Off` ⇒ `-frtti` / `/GR`) but flipping it does not compile the whole cascade yet.

History: emeraude-base was the first layer to fix, because its Observer contract carried a `std::any` payload (which
MSVC refuses under `/GR-`, STL1003) and asio used `typeid` without `ASIO_NO_TYPEID`. Both are fixed (below); the
decision and its traps live in `docs/error-handling.md` § The Observer payload is a `Base::Any`.

## What remains

⚠️ **Blocked, and the blockers are in other repositories:** engine `rtti-removal` and projet-alpha
`actor-trait-queries-without-rtti`. Both remaining tasks below wait on them. The payload part is done (see below).

**Owner decision (2026-10-07): option A.** Done the same day on Linux: `src/Any.hpp` (`Base::Any`, `typeHashOf< T >()`,
`anyCast< T >()`), `AnyValue.hpp` / `ObservableTrait` / `ObserverTrait` / `test_ObserverPattern.cpp` migrated, new
`test_Any.cpp`, `ASIO_NO_TYPEID` in `cmake/SetupASIO.cmake`; the engine (~38 files) and projet-alpha (11) migrated in
the same change (signatures only: every read already went through `anyValue< T >()`). Now
`ObservableTrait::notify()` and `ObserverTrait::onNotification()` (`src/ObserverTrait.hpp:140`) take a `const Any &`.
Doctrine and traps (cv/ref normalisation, compiler-family-local hash, collision risk): `docs/error-handling.md`
§ The Observer payload is a `Base::Any`. (Option B, not chosen: typed payloads per subject — a redesign of every
observable and observer; keeping `std::any` on GCC/Clang only was rejected, MSVC cannot compile it under `/GR-`. Not
yet in `docs/error-handling.md`: move this sentence there when the item closes.)

Base alone without RTTI ACCEPTED on three OS (2026-10-07, base `92ab1fd`): Linux `-fno-rtti` + `ASIO_NO_TYPEID`, macOS
142/142 steps, Windows MSVC `/GR-` 145/145 steps with no STL1003 and nothing from `Any.hpp` under `/W4 /WX` — ctest
2435 run / 2432 passed / 3 skipped on each.
- [ ] Flip the default of `EMERAUDE_DISABLE_RTTI` to `On` **only after** the engine and projet-alpha items are closed
      (`rtti-removal` in emeraude-engine, `actor-trait-queries-without-rtti` in projet-alpha); document the flip in
      `docs/integration.md` and `docs/error-handling.md`.
- [ ] Docs before deletion: `docs/error-handling.md` gets the RTTI counterpart of its exceptions section (what is
      forbidden: `dynamic_cast`, `typeid`, `std::any`, `std::type_index`, `std::function::target()`; what replaces
      each).

## References

- Engine item: `emeraude-engine/docs/todo/rtti-removal.md` (consumer of this payload type).
- projet-alpha item: `projet-alpha/docs/todo/actor-trait-queries-without-rtti.md`.
- asio typeid guards: `dependencies/asio/asio/include/asio/executor.hpp:298`,
  `asio/detail/handler_work.hpp:213`.
- EnTT `type_hash` (MIT) — https://github.com/skypjack/entt — reference for the compile-time
  type-name hash; cite it at the point of use if the technique is reproduced.
