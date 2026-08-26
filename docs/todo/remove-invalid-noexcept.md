---
id: remove-invalid-noexcept
title: Remove every invalid noexcept keyword
status: in-progress
priority: unranked
scope: cascade-wide (emeraude-base first)
opened: unknown
tags: [cpp, hygiene]
---

# Remove every invalid noexcept keyword

## What remains

- [ ] Sweep emeraude-base for `noexcept` on functions that can in fact terminate — allocation,
  container growth, anything reaching a call that is not itself `noexcept`. A wrong `noexcept` is
  not a hint, it is a promise the compiler enforces with `std::terminate`.
- [ ] Then the engine, then projet-alpha.

⚠️ The historical entry was marked WIP with no date and no record of how far the sweep got:
re-measure the state before calling any part of it done.

## Where it starts, and why the item lives here

**Owner decision (2026-08-26): emeraude-base owns this sweep.** It is cascade-wide work, and the
foundation is where the convention is set — the doctrine of [`docs/plans/ave-robustus.md`](../plans/ave-robustus.md)
applies verbatim: *"emeraude-base first. The engine and projet-alpha inherit the hardening later."*
The item was inherited from the engine's historical root `TODO.md`, written before `EmEn::Libs`
was extracted into this repository.

It is consolidation, not a feature, so it does not conflict with the freeze in effect until
"Ave robustus!" is formally closed ([`ave-robustus-formal-closure.md`](ave-robustus-formal-closure.md)).
