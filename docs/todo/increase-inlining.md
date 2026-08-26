---
id: increase-inlining
title: Increase inlining where it pays
status: in-progress
priority: unranked
scope: cascade-wide (emeraude-base first)
opened: unknown
tags: [performance, cpp]
---

# Increase inlining where it pays

## What remains

- [ ] Continue moving small, hot accessors into the headers, and **measure**. The project rule is
  RUNTIME > READABILITY > COMPILE TIME, so this is legitimate work — but it is worth doing only
  where a profile says so, never as a blanket "inline everything".
- [ ] Then the engine, then projet-alpha.

⚠️ The historical entry was marked WIP with no record of what was already covered.

## Where it starts, and why the item lives here

**Owner decision (2026-08-26): emeraude-base owns this sweep.** It is cascade-wide work, and the
foundation is where the convention is set — the doctrine of [`docs/plans/ave-robustus.md`](../plans/ave-robustus.md)
applies verbatim: *"emeraude-base first. The engine and projet-alpha inherit the hardening later."*
The item was inherited from the engine's historical root `TODO.md`, written before `EmEn::Libs`
was extracted into this repository.

It is consolidation, not a feature, so it does not conflict with the freeze in effect until
"Ave robustus!" is formally closed ([`ave-robustus-formal-closure.md`](ave-robustus-formal-closure.md)).
