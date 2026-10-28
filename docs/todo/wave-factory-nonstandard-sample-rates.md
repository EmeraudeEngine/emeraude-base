---
id: wave-factory-nonstandard-sample-rates
title: A sound file at a sample rate outside the Frequency enum (22255 Hz…) fails to load
status: open
priority: high
scope: WaveFactory (Types.hpp Frequency, FileFormatSNDFile::readStream(), toFrequency())
opened: 2026-10-28
tags: [audio, defect, loaders]
---

# A sound file at a sample rate outside the Frequency enum (22255 Hz…) fails to load

## Why

`FileFormatSNDFile::readStream()` turns libsndfile's sample rate into `WaveFactory::Frequency`, an enum of ten
standard rates (4000 to 192000 Hz); any other rate gives `Frequency::Invalid` and the load is refused
("readStream(), invalid frequency !"), the resource failing in turn. Found 2026-10-28 by a downstream demo whose car
sounds would not load: in a downstream data store, **13 of 879 sounds** are refused — 9 at 22255 Hz (a classic Mac
rate: a car door slam, an engine loop, impacts), and one each at 22222, 22254, 16007 and 12500 Hz. libsndfile reads
them and OpenAL plays any rate (`alBufferData` takes the frequency as an integer): only the enum stands in the way.

## What remains (an owner decision first)

- Choose the contract:
  1. Accept any rate within a sane range (say 1 kHz to 384 kHz): `Frequency` becomes a strong integer type (or the
     wave carries the rate as an integer), the enum's standard values stay as named constants.
  2. Resample on load to the nearest standard rate (the processor's resampler), keeping the enum closed.
  3. Refuse, but name the rate in the error and list the standard ones (the data must be converted offline).
- Then the failing-then-passing unit test (a WAV written at 22255 Hz, loaded), Release + ASan/UBSan.

## References

- `src/WaveFactory/Types.hpp` (`Frequency`), `src/WaveFactory/FileFormatSNDFile.cpp:227`.
