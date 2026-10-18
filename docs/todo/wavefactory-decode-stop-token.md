---
id: wavefactory-decode-stop-token
title: The WaveFactory decoders and resampler cannot be stopped
status: open
priority: high
scope: src/WaveFactory/FileIO.hpp, src/WaveFactory/FileFormatSNDFile.cpp, src/WaveFactory/Processor.hpp
opened: 2026-10-18
tags: [wavefactory, robustness, ave-robustus-ii, defect]
---

# The WaveFactory decoders and resampler cannot be stopped

## Why
Windows peer, 2026-10-18 (engine 39cec761): a `Core.shutdown()` while a playlist MP3 loaded (`Kyrandia/Medley`, 328 s:
decode, then the 44 100 -> 48 000 Hz resampling) took 6.58 s instead of ~0.8 s —
`stopBackgroundWork(): the 'Music manager' tasks took 4278 ms to stop`. `WaveFactory::FileIO::read()` (libsndfile /
mpg123 decode) and `Processor::resample()` (libsamplerate) run to the end: nothing takes a `std::stop_token`. Ave
Robustus II: a long computation takes a stop token and checks it with a bounded latency in EVERY stage.

## What remains
- A `std::stop_token` parameter (defaulted) on `FileIO::read()` and the long `Processor` stages (resample, the MIDI
  renderers' paths through them), checked per decoded block / per resampler chunk; a stopped call returns false.
- A test: a long decode stopped from another thread returns within a bounded time (the latency measured).
- Then the engine passes its job's token (engine item `music-load-honours-stop-token`).

## References
- engine `Resources::Manager::stopBackgroundWork()`; base `docs/caution-points.md` § the decimator's stop token (the
  same pattern, `ShapeDecimator::decimate(std::stop_token)`).
