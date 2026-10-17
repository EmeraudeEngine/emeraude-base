## Input Robustness (Ave robustus! — A.2)

The three file-format parsers consume **untrusted input** (WAV/FLAC/OGG via libsndfile, the
hand-rolled MIDI parser, the JSON SFX interpreter). Owner directive: *malformed input must never
crash the engine — cancel the load (`return false`), nothing fancier.* Hardening landed in the
A.2 WaveFactory pass, each fix covered by `Testing/test_WaveFactoryFileFormats.cpp` and run green
in Release **and** under ASan/UBSan via `ctest`.

- **MIDI — zero time division** (`FileFormatMIDI::parseHeader`): `division == 0` is now rejected.
  It is the divisor in the tick→sample tempo conversion; unchecked it produces `+inf` and the cast
  to a `uint32_t` sample count is UB.
- **MIDI — the rendered duration (2026-10-17, the coverage pass; owner decision: 30 minutes for both renderers)**: a
  4-byte delta time is 2^28 ticks, so a 36-byte file can declare any duration. The additive renderer (no SoundFont) had
  no ceiling: ~750 KB of RAM per declared second at 48 kHz, 2 hours = 6 GB then `std::bad_alloc` -> abort; beyond
  ~24.8 hours both renderers converted the sample count to `uint32_t` out of range (undefined behaviour, the SoundFont
  path BEFORE its own 30-minute clamp). Now `renderedSampleCount()` bounds both renderers to `MaxDurationSeconds`
  (truncated, a warning — the SoundFont path's former behaviour; worst case of a hostile file: ~1.35 GB / 3.4 s at
  48 kHz), and every floating-point tick / sample conversion goes through `saturatedUInt32()`. The header and the track
  chunk refuse a short read (`!stream.good()`), and the big-endian readers never read an indeterminate byte. Tests:
  `WaveFactoryMIDI.declaredDurationBeyondTheLimitIsTruncated` (both renderers) and the coverage tests beside it.
  `fuzz_midi` (10 min, `float-cast-overflow` on): 0 crash / OOM / timeout. ⚠️ The CPU cost is NOT bounded yet (~3 s
  per note held 30 minutes, no stop token): `docs/todo/midi-additive-render-cost.md`.
- **MIDI — the additive renderer's resonant filter diverged (2026-10-17, found by the same tests under GCC UBSan with
  `float-cast-overflow`)**: Paul Kellett's filter (musicdsp.org Filters #29) defines its resonance q between 0 and 1;
  `Synthesizer::filterResonanceToFeedback()` mapped CC#71 up to 3.8 and the feedback used `(1 - f/2)` instead of the
  published `(1 - f)`. From CC#71 ~ 40 with the filter engaged the output grew to infinity: one such note ruined the
  whole track (the normalization) and its conversion to an integer was undefined behaviour. Owner decisions: the
  published form (q at most 0.99, `fb = q + q/(1 - f)`; bounded over the whole CC grid, 4.15 peak for a 0.25 input),
  CC#71 / CC#74 kept ABSOLUTE (GS/XG read them relative to 64 — not adopted). The renderer calls
  `Synthesizer::applyResonantFilterSample()` (its inline copy removed), drops a non-finite sample and clips each
  contribution before its integer conversion. Test `WaveFactoryMIDI.resonantFilterStaysBoundedOverTheWholeControllerRange`.
- **MIDI — speculative reserve clamp** (`processIStream`): the per-track `reserve()` hints derive
  from the untrusted 16-bit `trackCount`. They are clamped (`min(trackCount, 256)`) so a forged
  header cannot trigger a large transient allocation before the per-track parse fails fast.
- **libsndfile — decode buffer bound** (`FileFormatSNDFile::readStream`): `soundFileInfos.frames`
  is an *estimate* for VBR formats and is attacker-influenced. The float decode buffer size is now
  bounded by `MaxDecodedSamples` (512 M samples / 2 GiB), division-first to stay overflow-safe;
  out-of-range frame counts cancel the load instead of OOM-ing under `-fno-exceptions`.
- **libsndfile — short-write detection** (`writeStream`): `sf_writef_short` return value is now
  checked against the requested frame count; a partial write returns `false` (was silently ignored).
- **JSON SFX — duration cap** (`SFXScript::processScript`): `sampleCount = sampleRate * durationMs
  / 1000` sizes one Synthesizer buffer per track. `durationMs` is capped at 30 min (`MaxDurationMs`)
  so an unbounded JSON duration cannot request a multi-hundred-GiB allocation.
- **Diagnostics**: all `std::cerr` sites in the three parsers (+ `Processor::resample`) migrated to
  the `EmEn::Base::Logging` hook (no raw `cerr` in this module's parsers).
