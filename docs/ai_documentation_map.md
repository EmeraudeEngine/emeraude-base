# AI Documentation Map

Index of the AI-facing documentation for **emeraude-base**.

## Entry points

| Document | Purpose |
|----------|---------|
| [`/CLAUDE.md`](../CLAUDE.md) | Redirect to `AGENTS.md` (standard). |
| [`/AGENTS.md`](../AGENTS.md) | Root context: identity, position, CMake architecture, axioms, conventions. |
| [`/README.md`](../README.md) | Public-facing: what emeraude-base is and how to consume it standalone. |

## docs/

| Document | Purpose |
|----------|---------|
| [`module-map.md`](module-map.md) | Module → CMake target mapping, external deps, migration status. |
| [`integration.md`](integration.md) | How to link emeraude-base into a third-party project. |
| `cpp-conventions.md` | *(planned, migrate from engine)* Code style shared across the project. |
| `coordinate-system.md` | *(planned, migrate from engine)* Math conventions (right-handed, Y-down). |

## Per-module context

- [`/src/AGENTS.md`](../src/AGENTS.md) — the foundation modules reference (math, factories,
  I/O, hashing, threading, traits, …). Migrated from the engine's former `src/Libs/AGENTS.md`.
- [`/src/WaveFactory/AGENTS.md`](../src/WaveFactory/AGENTS.md) — audio factory details.