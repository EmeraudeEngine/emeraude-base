---
id: rmid-embedded-dls-soundbank
title: RMID embedded DLS sound bank
status: open
priority: low
scope: src/WaveFactory/FileFormatMIDI.hpp (base); engine Audio/MusicResource (the consumer)
opened: unknown
tags: [audio, midi, deferred]
---

# RMID embedded DLS sound bank

## Owner decision (2026-10-27)

**Implement it**, consumer or not (owner, closing every emeraude-base item).

## Why

An RMID container can carry an embedded DLS sound bank alongside the SMF data. The current unwrap
(`src/WaveFactory/FileFormatMIDI.hpp`, ~99-114; it came with engine commit `ba2d4d2e`) reads the SMF and ignores any
trailing DLS chunk. Rendering uses the application-level soundfont that engine `Audio/MusicResource.cpp` (~363) takes
from `Core/Audio/MusicSoundfont`.

**Low priority**: it only matters if a future title relies on per-track DLS banks instead of the global SF2. Tyrian
and similar mid-1990s soundtracks render correctly with a general-purpose SF2.

Moved here from projet-alpha on 2026-10-11, because the parsing belongs to this repository. The engine part (the
per-resource override) follows once the base exposes the bank.

## What remains, if it ever matters

- [ ] Base: parse the optional `DLS ` RIFF subchunk after the `data` chunk in `FileFormatMIDI`, with a unit test.
- [ ] Base: have `FileFormatMIDI` prefer the embedded bank over the global SF2 when present.
- [ ] Engine: expose it as a per-resource `SoundfontResource` override on `MusicResource`.
