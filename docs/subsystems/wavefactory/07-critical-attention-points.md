## Critical Attention Points

- **Synthesizer is mono-only**: Do not add channel loops to Synthesizer methods
- **Multi-channel via SFXScript**: Use JSON tracks for stereo/multi-channel
- **Region system**: Use `setRegion()`/`resetRegion()` for sub-buffer operations
- **Type conversion**: Use `dataConversion<>()` for int16_t <-> float conversion
- **Performance**: Synthesizer methods are optimized for single-channel processing
- **MIDI End of Track**: Always handle meta event 0x2F to properly terminate parsing. See: `FileFormatMIDI.hpp:parseTrack()`
- **MIDI Tempo Map**: Use `ticksToSamplesWithTempoMap()` for accurate timing with tempo changes. See: `FileFormatMIDI.hpp:ticksToSamplesWithTempoMap()`
- **MIDI EOF Safety**: `readVariableLength()` must check for EOF to prevent infinite loops on truncated files. See: `FileFormatMIDI.hpp:readVariableLength()`
- **Wave Invalid Frequency**: Always check frequency validity before computing durations. `seconds()`/`milliseconds()` return 0.0F for invalid frequency.
