---
id: midi-additive-render-cost
title: Bound the CPU cost of the MIDI additive renderer, and make it stoppable
status: open
priority: high
scope: src/WaveFactory/FileFormatMIDI.hpp (renderToWave), src/WaveFactory/FileFormatInterface.hpp (if a stop token goes through it)
opened: 2026-10-17
tags: [audio, midi, ave-robustus-ii, trust-boundary]
---

# Bound the CPU cost of the MIDI additive renderer, and make it stoppable

## Why

The 2026-10-17 coverage pass bounded the rendered DURATION (30 minutes, both renderers — `MaxDurationSeconds`), hence
the memory (~1.35 GB at 48 kHz at worst). The additive renderer (no SoundFont) still costs time in proportion to the sum
of its notes' rendered samples: every note is synthesized sample by sample, with no polyphony limit. Measured on Linux
(Release, 48 kHz, one track of N notes held past the ceiling):

| Notes held 30 min | File | Render |
|---|---|---|
| 1 | 37 bytes | 3.45 s |
| 4 | 61 bytes | 12.2 s |
| 16 | 157 bytes | 47.4 s |

Linear, ~3 s per note: a 4 KB file can hold ~1 000 such notes, ~50 minutes of CPU on one loader thread. `fuzz_midi`
(10 min, 2026-10-17) reported 35 slow units of 81 bytes, each 8.4 s in Release. And `readStream()` takes no
`std::stop_token`: a shutdown during such a load waits for it (Ave Robustus II: a long computation checks a stop token
with a bounded latency in every stage).

The SoundFont renderer is bounded by TinySoundFont's voice limit (256, `tsf_set_max_voices()`), not by this item.

## Owner decision (2026-10-27)

**A polyphony limit in the additive renderer (256 voices, the oldest voice stolen, as TinySoundFont) AND a
`std::stop_token` through `ReadOptions`** (`ReadOptions::stopToken`, in place since 2026-10-27 for the decoders). No voice-second budget.

## What remains — owner decision first

Options (to present with trade-offs and a recommendation):
- A budget of rendered voice-seconds (the sum of the notes' rendered durations), the file refused or truncated beyond it;
- a polyphony limit in the additive renderer (the oldest voice stolen, like a hardware synthesizer / TSF);
- a `std::stop_token` reaching the renderer: through `ReadOptions`, or through `FileFormatInterface::readStream()` — a
  cross-cutting API change for every format.

## ⚠️ Traps

- The `fuzz_midi` campaign needs `-timeout` above the worst legitimate render under ASan (a 30-minute single note takes
  tens of seconds there), or every ceiling case is reported as a timeout.
- A legitimate long, dense piece is expensive by nature: the budget must not refuse real music (a 30-minute piece with
  16 voices on average is 8 voice-hours).

## References

- `docs/subsystems/wavefactory/06-input-robustness.md` § the rendered duration.
- projet-alpha `docs/plans/ave-robustus-ii.md` (coverage, gap 4).
