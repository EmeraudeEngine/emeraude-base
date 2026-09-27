## Architecture (Separation of Concerns)

### Core Classes

**Wave<T>** - Audio data container (template)
- Stores samples in `std::vector<precision_t>`
- Supports any arithmetic type (int16_t default, float for processing)
- File I/O via libsndfile (int16_t specialization)
- See: `Wave.hpp`

**Synthesizer<T>** - Sound generation (creates new sounds)
- Takes a `Wave<T>&` reference, generates directly into it
- **Mono-only**: Works exclusively with mono audio for performance
- **Region system**: `setRegion(offset, length)` / `resetRegion()` for targeting specific portions
- Noise: `whiteNoise()`, `pinkNoise()`, `brownNoise()`, `blueNoise()`
- Waveforms: `sineWave()`, `squareWave()`, `triangleWave()`, `sawtoothWave()`
- Special: `pitchSweep()`, `noiseBurst()`
- Envelopes: `applyADSR()`, `applyFadeIn/Out()`
- Modulation: `applyVibrato()`, `applyTremolo()`, `applyRingModulation()`
- Filters: `applyLowPass()`, `applyHighPass()`
- Guitar effects: `applyDistortion()`, `applyOverdrive()`, `applyFuzz()`
- Modulation FX: `applyChorus()`, `applyFlanger()`, `applyPhaser()`
- Delay FX: `applyDelay()`, `applyReverb()`
- Dynamic FX: `applyWahWah()`, `applyAutoWah()`, `applyCompressor()`, `applyNoiseGate()`
- Lo-fi FX: `applyBitCrush()`, `applyPitchShift()`, `applySampleRateReduce()`
- Utilities: `mix()`, `reverse()`, `normalize()`
- See: `Synthesizer.hpp`

**SFXScript<T>** - JSON-based procedural sound effects
- Parses JSON definitions for multi-track audio generation
- Creates one mono `Synthesizer` per track, then interleaves for stereo output
- **Processing order per track**: `preInstructions` → `regions` → `instructions`
- `preInstructions`: Applied first on full track (generators like noise, waveforms)
- `regions`: Applied to specific portions (modifiers, localized effects)
- `instructions`: Applied last on full track (post-processing)
- `finalInstructions`: Applied to all tracks uniformly before interleaving
- Multi-channel via track interleaving, not per-sample channel loops
- See: `SFXScript.hpp:processTrack()`

**Processor** - Sound transformation (modifies existing sounds)
- Works internally in float precision via `dataConversion()`
- Structural: `trim()`, `crop()`, `pad()`, `concat()`, `split()`
- Channels: `mixDown()`, `toStereo()`, `extractChannel()`, `swapChannels()`
- Resampling: `resample()` (via libsamplerate, SRC_SINC_BEST_QUALITY)
- Analysis: `getPeakLevel()`, `getRMSLevel()`, `getDuration()`, `detectSilence()`
- Quality: `normalize()`, `convertBitDepth()`
- See: `Processor.hpp`, `Processor.cpp`

**dataConversion<In,Out>()** - Type conversion helper
- Converts Wave between precision types (int16_t <-> float)
- Handles normalization automatically
- See: `Wave.hpp:dataConversion()`

### File I/O Classes (Unified ByteStream Architecture)

All format handlers operate on `IO::ByteStream &` (polymorphic: file or memory).

**FileIO** - File-backed dispatcher by extension
- Creates `IO::FileStream`, delegates to format handler `readStream`/`writeStream`
- `read(path, wave, ReadOptions)` — single overload with options struct
- `write(wave, path, overwrite, WriteOptions)` — infers AudioFormat from extension
- See: `FileIO.hpp`

**StreamIO** - Memory buffer I/O
- `read(vector<byte>, wave, ReadOptions)` — via `IO::MemoryStream` → libsndfile only
- `write(wave, vector<byte>, WriteOptions)` — via `IO::MemoryStream` → libsndfile only
- See: `StreamIO.hpp`

**FileFormatInterface** - Abstract base for audio formats
- `readStream(IO::ByteStream &, Wave &, ReadOptions)` / `writeStream(IO::ByteStream &, Wave &, WriteOptions)`
- See: `FileFormatInterface.hpp`

**FileFormatSNDFile** - libsndfile wrapper via `sf_open_virtual`
- Virtual I/O callbacks (get_filelen/seek/read/write/tell) delegate to `IO::ByteStream`
- Supports WAV, FLAC, OGG (read/write)
- WriteOptions::AudioFormat selects output format
- **Lossy decode path** (int16_t specialization): reads via `sf_readf_float` then converts manually
  with explicit clamp to ±1.0 before scaling to int16. This is mandatory for Vorbis/MP3/FLAC-24:
  - Lossy codecs produce float samples that can overshoot ±1.0 on transients (drum hits, sibilants).
    Direct `sf_readf_short` would wrap the int16 instead of clipping → audible clicks at peaks.
    `SFC_SET_CLIPPING` is unreliable across codec paths — manual clamp is the only safe path.
  - `sf_info.frames` is documented as an *estimate* for VBR formats. The reader trims the wave
    to the actual frame count returned, never treats a short read as failure.
  - Silent failure now logs `sf_strerror(file)` so any future decoder issue is diagnosable.
- See: `FileFormatSNDFile.hpp`

**FileFormatJSON** - Procedural audio via SFXScript
- Reads ByteStream to string, passes to `SFXScript::generateFromString()`
- Write not supported (returns false)
- See: `FileFormatJSON.hpp`

**FileFormatMIDI** - MIDI file parser with synthesized audio output
- Reads ByteStream to memory, creates `std::istringstream`, processes via `processIStream()`
- ReadOptions provides `synthesisFrequency` and optional `soundfont` (tsf*)
- Write not supported (returns false)
- **RMID unwrap**: detects the Microsoft RIFF/RMID/data 20-byte container that wraps a standard
  SMF (Tyrian and other mid-1990s game soundtracks). Header signature is `RIFF...RMID...data`;
  when matched, `istringstream::seekg(20)` skips to the inner SMF without reallocating the buffer.
  Files without the prefix are parsed as-is. Optional embedded DLS sound bank is currently
  ignored — rendering uses the application-level SF2 configured via `Core/Audio/MusicSoundfont`.
- See: `FileFormatMIDI.hpp`

**ReadOptions / WriteOptions** — Defined in `Types.hpp`
- `ReadOptions`: `synthesisFrequency` (for MIDI), `soundfont` (tsf* for SF2)
- `WriteOptions`: `AudioFormat` enum (WAV, FLAC, OGG)
