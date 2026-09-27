## Detailed Documentation

In this repository:
- [`VertexFactory/AGENTS.md`](../../../src/VertexFactory/AGENTS.md) — geometry subsystem context
- [`WaveFactory/AGENTS.md`](../../../src/WaveFactory/AGENTS.md) — audio subsystem context
- [`Testing/AGENTS.md`](../../../src/Testing/AGENTS.md) — unit-test conventions and gates
- [`../docs/plans/ave-robustus.md`](../../plans/ave-robustus.md) — robustness plan and per-fix history

Downstream consumers (their AGENTS networks live in their own repositories):
- **emeraude-engine** — Scenes (CartesianFrame, Animation types via GLTF), Physics (Vector/Matrix,
  collision), Graphics (Math transforms), Audio (3D positioning, WaveFactory), Animations
  (runtime evaluation of `Animation/` data types)
- **Standalone tools** — any process linking `emeraude::base` without the engine runtime
