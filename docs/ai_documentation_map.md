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
| [`caution-points.md`](caution-points.md) | Cross-cutting compiler/platform pitfalls (GCC `-Wstringop-*` false positives on `std::string`, …) and their source-level fixes. |
| [`error-handling.md`](error-handling.md) | **Error-handling contract** (Ave robustus A.0): bool/optional propagation, abort policy, no-throw, untrusted-input bounds, the Logging hook. Normative. |
| [`plans/ave-robustus.md`](plans/ave-robustus.md) | **Long-term governance plan** "Ave robustus!" — robustness of software intent (completeness + execution hardening). Feature freeze + "no fix without a test" in effect. *(VALIDATED 2026-05-31)* |
| [`plans/ave-robustus-inventory.md`](plans/ave-robustus-inventory.md) | Phase 0 deliverable: full promised-vs-delivered inventory of all 15 modules (severity tiers, gaps A/B, correctness bugs, open decisions). |
| `cpp-conventions.md` | *(planned, migrate from engine)* Code style shared across the project. |
| `coordinate-system.md` | *(planned, migrate from engine)* Math conventions (right-handed, Y-up). |

## Per-module context

- [`/src/AGENTS.md`](../src/AGENTS.md) — the foundation modules reference (math, factories,
  I/O, hashing, threading, traits, …). Migrated from the engine's former `src/Libs/AGENTS.md`.
- [`/src/VertexFactory/AGENTS.md`](../src/VertexFactory/AGENTS.md) — geometry factory details.
- [`/src/WaveFactory/AGENTS.md`](../src/WaveFactory/AGENTS.md) — audio factory details.