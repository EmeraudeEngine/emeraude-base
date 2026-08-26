---
id: observable-static-shared-rewrite
title: Rewrite the Observer/Observable pattern for static and shared objects
status: open
priority: unranked
scope: src/Observer*
opened: unknown
tags: [architecture, cpp]
---

# Rewrite the Observer/Observable pattern for static and shared objects

## Why

The historical engine `TODO.md` carried: *"Rewrite libs Observer/Observable pattern with the idea
of static and shared objects."* The pattern now lives in **emeraude-base**
(`src/ObservableTrait.*`, `src/ObserverTrait.*`), which is why the item is here.

## What remains

- [ ] Decide what "static and shared objects" must mean for the contract, then rewrite.

## ⚠️ Traps (contract in force, whatever the rewrite)

- **Never emit a notification while iterating a container or holding a mutex the handler may
  take** — defer it.
- `onNotification` returning `true` means **STOP** propagation.
- The whole cascade depends on this pattern; a change here is a cascade-wide change.

## ⚠️ Needs owner clarification

The original line predates the extraction into emeraude-base and gives no symptom. Confirm the
intent (lifetime safety? avoiding `shared_ptr` in the observer list? static registration of
observers?) before designing anything.
