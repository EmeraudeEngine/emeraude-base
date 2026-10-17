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
- **Synthesizer / SFX scripts — the parameter domains, the saturated output (2026-10-17, the coverage pass; owner
  decision: REFUSE a value outside its domain)**: SFX scripts (data) drive the generators and effects with whatever
  finite number they hold. A clang UBSan probe of every effect with reachable extremes found: `toSampleFormat()`
  converting an amplitude above 1 to `int16_t` out of range (UB), `applyADSR()` / `applyFadeOut()` turning a negative
  time into a `size_t` (UB), `applyVibrato()` extrapolating its interpolation to -7e8, a negative-frequency
  triangle / sawtooth and a 0 Hz high-pass leaving [-1, 1]. Now every generator and effect checks each parameter
  against its documented domain (`Synthesizer::inDomain()`: NaN refused; times within the 30-minute SFX ceiling;
  frequencies in (0, Nyquist]; the ranges of each `@param`) and refuses with an error naming it — SFXScript then
  refuses the script; an amplitude above 1 stays allowed and `toSampleFormat()` saturates (clipping, NaN gives
  silence). Also found: every effect READ a float wave divided by `numeric_limits< float >::max()` (a float wave came
  out ~1e-39 after an envelope, `normalize()` did nothing) — `fromSampleFormat()`, the mirror of `toSampleFormat()`, now.
  Tests `test_WaveFactorySynthesizer.cpp` (5 of 6 failed on the former code: two UBSan aborts, the crushed float, the
  accepted script) `fuzz_json_sfx` (instrumented, seeded with valid scripts) 10 minutes after the fixes: 22 M
  executions, 0 crash.
- **SFXScript — a parameter of the wrong type (2026-10-17; owner decision: REFUSE it, WARN on an unknown key)**: every
  optional parameter was read with `getValue(...).value_or(default)`, so a value present with the wrong type was read
  as ABSENT — `"frequency": "880"` played at 440 Hz, `"channels": "1"` became 1, `"offset": "5"` became 0 — and an
  optional array of the wrong type (`"instructions": 5`) was skipped. Now `param< T >()` refuses the instruction (and
  the script) naming the key, `isPresentButNotAnArray()` refuses such an array, and `warnUnknownKeys()` warns about a
  key no instruction / script / track / region reads (`"bitDepth"` for `"bits"`). NOTE: a fractional value for an
  integer (`"bits": 6.5`) is NOT a wrong type — FastJSON truncates it toward zero, its documented contract. The 12 SFX
  scripts of the projet-alpha data stores decode with 0 error and 0 warning under these rules and the effect domains.
  Tests `WaveFactorySFXScript.*` (every instruction type, regions, final instructions, stereo, the structural refusals).
- **WaveFactory::Processor (2026-10-17)**: `convertBitDepth(32)` computed its level count as `1 << 31` in an `int`
  (INT_MIN): a negative step and reversed dither bounds (libstdc++ asserts `a <= b` in `uniform_real_distribution`) —
  computed in 64 bits now. `test_WaveFactoryProcessor.cpp` covers every operation (the engine's mix-down / resample
  path of every sound and music load included).
- **JSON SFX — duration cap** (`SFXScript::processScript`): `sampleCount = sampleRate * durationMs
  / 1000` sizes one Synthesizer buffer per track. `durationMs` is capped at 30 min (`MaxDurationMs`)
  so an unbounded JSON duration cannot request a multi-hundred-GiB allocation.
- **Diagnostics**: all `std::cerr` sites in the three parsers (+ `Processor::resample`) migrated to
  the `EmEn::Base::Logging` hook (no raw `cerr` in this module's parsers).
