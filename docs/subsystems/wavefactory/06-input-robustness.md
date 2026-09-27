## Input Robustness (Ave robustus! — A.2)

The three file-format parsers consume **untrusted input** (WAV/FLAC/OGG via libsndfile, the
hand-rolled MIDI parser, the JSON SFX interpreter). Owner directive: *malformed input must never
crash the engine — cancel the load (`return false`), nothing fancier.* Hardening landed in the
A.2 WaveFactory pass, each fix covered by `Testing/test_WaveFactoryFileFormats.cpp` and run green
in Release **and** under ASan/UBSan via `ctest`.

- **MIDI — zero time division** (`FileFormatMIDI::parseHeader`): `division == 0` is now rejected.
  It is the divisor in the tick→sample tempo conversion; unchecked it produces `+inf` and the cast
  to a `uint32_t` sample count is UB.
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
