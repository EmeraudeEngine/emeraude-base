---
id: std-move-function-args
title: Use std::move on function arguments where it pays
status: in-progress
priority: unranked
scope: cascade-wide (emeraude-base first)
opened: unknown
tags: [performance, cpp]
---

# Use std::move on function arguments where it pays

## What remains

- [ ] Audit by-value parameters that are then copied into a member, and move them instead.
- [ ] Then the engine, then projet-alpha.

⚠️ The historical entry was marked WIP with no record of what was already covered.

## Where it starts, and why the item lives here

**Owner decision (2026-08-26): emeraude-base owns this sweep.** It is cascade-wide work, and the
foundation is where the convention is set — the doctrine of [`docs/plans/ave-robustus.md`](../plans/ave-robustus.md)
applies verbatim: *"emeraude-base first. The engine and projet-alpha inherit the hardening later."*
The item was inherited from the engine's historical root `TODO.md`, written before `EmEn::Libs`
was extracted into this repository.

It is consolidation, not a feature. (It was opened under the "Ave robustus!" feature freeze,
which the owner lifted on 2026-08-27 with the plan's closure — no constraint remains.)
