---
id: midi-rendering-cancelled-logs-an-error
title: A MIDI rendering given up on a stop request logs an error per track
status: open
priority: high
scope: src/WaveFactory/FileFormatMIDI.hpp (processIStream(), renderToWave())
opened: 2026-11-27
tags: [defect, logging, midi, shutdown]
---

# A MIDI rendering given up on a stop request logs an error per track

## Why

`FileFormatMIDI` takes a `std::stop_token` (`ReadOptions::stopToken`) and its rendering loops give up when a stop is
requested (`m_stopToken.stop_requested()`). `renderToWave()` then returns false and `processIStream()` reports it as
a failure: `[Error][WaveFactory::FileFormatMIDI] processIStream(), failed to render audio !` — one line per track
still rendering.

Measured 2026-11-27 (Linux, the engine's testbed, Release): a playlist of 74 MIDI tracks loaded, the application shut
down two seconds later — **31 such `[Error]` lines**, a clean exit otherwise. The same playlist left to load for 60 s:
0 line, and its tracks play. Also seen in the engine's track mixer stress run (29 lines on Linux, 12 on macOS): the
shutdown arrives while the playlist still renders — on macOS each line pairs with a `renderToWave(), stopped.` line,
and the engine reports "47 resource load(s) cancelled by the stop".

The consumer's contract says the opposite: the engine's `MusicResource::renderPendingMidi()` documents "the rendering
gives up once a stop is requested, without an error line". A requested cancellation is not an error; these lines
hide a real rendering failure among them and break the "0 `[Error]`" bar of a shutdown.

Priority: `high` by the default rule for a defect (a wrong report). It is a false line in the log, nothing is
corrupted: the owner may rank it lower.

## What remains

- Tell a cancellation from a failure in `renderToWave()` / `processIStream()`: no error line when the stop token is
  set (the caller still gets `false`), the error kept for a real failure. Check the other formats of `FileIO::read()`
  that take the token for the same shape.
- Proof: a unit test of the base that renders a MIDI with a stop already requested and captures the diagnostics
  (`Testing::captureDiagnostics()`): 0 error line, `false` returned; then the measurement above: 0 such line.

## References

- `src/WaveFactory/FileFormatMIDI.hpp` (`processIStream()`: "failed to render audio", `renderToWave()`, the
  `m_stopToken` checks).
