## Critical Attention Points

- **Synthesizer is mono-only**: Do not add channel loops to Synthesizer methods
- **Multi-channel via SFXScript**: Use JSON tracks for stereo/multi-channel
- **Region system**: Use `setRegion()`/`resetRegion()` for sub-buffer operations
- **Type conversion**: Use `dataConversion<>()` for int16_t <-> float conversion
- **Performance**: Synthesizer methods are optimized for single-channel processing
- **MIDI End of Track**: Always handle meta event 0x2F to properly terminate parsing. See: `FileFormatMIDI.hpp:parseTrack()`
- **MIDI Tempo Map**: Use `ticksToSamplesWithTempoMap()` for accurate timing with tempo changes. See: `FileFormatMIDI.hpp:ticksToSamplesWithTempoMap()`
- **MIDI EOF Safety**: `readVariableLength()` must check for EOF to prevent infinite loops on truncated files. See: `FileFormatMIDI.hpp:readVariableLength()`
- **Long decodes and resamples are STOPPABLE (2026-10-27)**: `ReadOptions::stopToken` reaches every format's
  `readStream()` — libsndfile decodes in blocks of `DecodeBlockFrames` (16 384) with the stop checked between two (a
  stopped read returns false) — and `Processor::resample(frequency, stopToken)` feeds libsamplerate in chunks of
  `ResampleChunkFrames` (16 384) through its streaming API (`src_new` / `src_process`, the tail drained with
  `end_of_input`): the SAME samples as the former one-shot `src_simple()` (test `theStreamedResampleGivesTheOneShotSamples`,
  largest difference 0.0); stopped, the wave is left as it was. Measured (Linux, 328 s stereo 44.1 → 48 kHz, stop
  100 ms in): 0.35-2.2 ms (`DISABLED_ResampleStopLatency`). Why: a 328 s MP3 held a shutdown 4.3 s (Windows peer,
  2026-10-18). The engine's music / sound loads pass their job's token (engine item `music-load-honours-stop-token`).
- **Wave Invalid Frequency**: Always check frequency validity before computing durations. `seconds()`/`milliseconds()` return 0.0F for invalid frequency.
