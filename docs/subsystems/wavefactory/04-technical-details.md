## Technical Details

### Noise Types (Spectral Characteristics)
- **White**: Flat spectrum, equal energy per frequency
- **Pink**: -3 dB/octave (Voss-McCartney algorithm), natural sounds
- **Brown**: -6 dB/octave (random walk), deep rumbles
- **Blue**: +3 dB/octave (high-pass filtered white), dithering

### Mono-Only Synthesizer Design

The `Synthesizer` class works exclusively with mono audio:

**Rationale:**
- Simpler, more performant code (no channel loops)
- Direct sample indexing (`data[sampleIndex]` vs `data[sampleIndex * channels + channel]`)
- Multi-channel handled at higher level by `SFXScript`

**Multi-channel workflow:**
1. JSON defines `"channels": 2` and multiple `"tracks"`
2. `SFXScript` creates one mono `Synthesizer` per track
3. Each track generates into its own mono buffer
4. Tracks are interleaved into final multi-channel output
5. `finalInstructions` applied to mixed result

### FileFormatMIDI Details

**Supported MIDI features:**
- Format 0 and 1 (single/multi-track)
- Note On/Off events with velocity
- Tempo changes (meta event 0x51)
- End of Track (meta event 0x2F) - proper parsing termination
- Variable-length delta times
- Running status compression
- **Pitch Bend (0xE0)**: Real-time pitch modification per channel
- **CC#1 (Modulation)**: Vibrato effect (LFO-based in TSF mode)
- **CC#7 (Volume)**: Channel volume control
- **CC#10 (Pan)**: Stereo positioning per MIDI channel (0=left, 64=center, 127=right)
- **CC#11 (Expression)**: Dynamic volume control
- **CC#64 (Sustain)**: Sustain pedal
- **CC#74 (Filter Cutoff)**: Brightness control
- **CC#92 (Tremolo)**: Amplitude modulation
- **Program Change (0xC0)**: Dynamic instrument selection per channel
- **Channel Pressure (0xD0)**: Aftertouch affecting whole channel
- **Polyphonic Key Pressure (0xA0)**: Per-note aftertouch (captured, limited TSF support)

**Advanced controllers (SF2 mode):**
- **CC#0, CC#32**: Bank Select MSB/LSB
- **CC#6, CC#38**: Data Entry MSB/LSB (for RPN)
- **CC#39, CC#42, CC#43**: Fine LSB controllers (Volume, Pan, Expression)
- **CC#98, CC#99**: NRPN LSB/MSB
- **CC#100, CC#101**: RPN LSB/MSB (Pitch Bend Range)
- **CC#120**: All Sound Off
- **CC#121**: Reset All Controllers
- **CC#123**: All Notes Off

**Output:**
- **Stereo output** with per-channel pan positioning
- Constant-power panning for natural sound

**Instrument families (waveform mapping):**
| Family | Programs | Waveform | ADSR |
|--------|----------|----------|------|
| Piano, Strings, Ensemble, Pads, Pipe | 0-7, 40-55, 72-79, 88-95 | Sine | Varied |
| Organ, Synth Lead, Chromatic | 8-23, 80-87 | Square | Sustained |
| Guitar, Bass | 24-39 | Sawtooth | Plucked |
| Brass, Reed | 56-71 | Triangle | Wind |
| Percussive, SFX | 112-127 | Noise burst | Short |
| **Channel 10** | Any | Noise burst | Percussion |

**SoundFont (SF2) Support:**
- Optional SF2 sample-based rendering via TinySoundFont
- `setSoundfont(tsf*)` to enable high-quality rendering
- Falls back to additive synthesis if no SF2 provided
- See: `Audio/SoundfontResource` for SF2 loading

**Dynamic Control Events (TSF mode):**
- Pitch Bend, Volume, Expression, Sustain, Pan update in real-time during playback
- Events processed via unified timeline (`TimelineEvent` struct)
- **Modulation/Vibrato**: LFO at 5.5Hz, ±50 cents max depth, scaled by CC#1 value
- **Adaptive chunked rendering**: 4096-sample chunks normally, 64-sample when vibrato active
- **Program Change**: Dynamic instrument switching during playback
- **Bank Select + RPN**: Full MIDI bank/preset selection support
- Pre-allocated 256 TSF voices for complex MIDI files
- See: `FileFormatMIDI.hpp:renderWithSoundfont()`

**Tempo Map:**
- Multiple tempo changes during playback properly handled
- Tempo events (`TempoEvent`) stored and sorted by tick
- Time-to-sample conversion via `ticksToSamplesWithTempoMap()`
- See: `FileFormatMIDI.hpp:TempoEvent`, `FileFormatMIDI.hpp:ticksToSamplesWithTempoMap()`

**Rendering Pipeline (SF2 mode):**
- Float accumulator buffer for lossless rendering before normalization
- Post-rendering normalization with 5% headroom (target peak = 0.95)
- Adaptive chunk sizes: 4096 samples normally, 64 samples when vibrato active
- Direct rendering into accumulator (no intermediate buffer allocation)
- Maximum duration limit: 30 minutes to prevent memory exhaustion
- See: `FileFormatMIDI.hpp:renderWithSoundfont()`

**Robustness:**
- **EOF protection**: `readVariableLength()` guards against truncated files to prevent infinite loops
- **NaN guards**: All output samples checked with `std::isfinite()` before writing
- **Invalid frequency handling**: `Wave::seconds()`/`milliseconds()` return 0.0F when frequency is Invalid
- See: `FileFormatMIDI.hpp:readVariableLength()`, `Wave.hpp:seconds()`

**Limitations:**
- No SMPTE time division
- No Reverb/Chorus effects (CC#91/93 - TSF limitation)
- Polyphonic Key Pressure captured but not applied (TSF limitation)
- No streaming mode (full pre-rendering to buffer)
- Truncated MIDI files may produce incomplete audio (graceful degradation)

**Note rendering:**
- Waveform selected by instrument family
- ADSR envelope adapted to instrument type
- Polyphony: additive mixing with saturation protection
- Stereo panning: constant-power pan law based on CC#10 per channel
- Auto-normalization to prevent clipping
- Note 69 (A4) = 440 Hz reference
