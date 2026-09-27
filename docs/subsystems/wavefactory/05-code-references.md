## Code References

| File | Description |
|------|-------------|
| `Wave.hpp` | Audio data container |
| `Synthesizer.hpp` | All generation/effects (header-only template) |
| `SFXScript.hpp` | JSON parser for procedural sound effects |
| `Processor.hpp/.cpp` | Transformation operations |
| `Types.hpp` | Channels, Frequency, AudioFormat enums + ReadOptions/WriteOptions structs |
| `FileIO.hpp` | File-backed format dispatcher (IO::FileStream) |
| `StreamIO.hpp` | Memory buffer I/O (IO::MemoryStream, libsndfile only) |
| `FileFormatInterface.hpp` | Abstract base: `readStream(ByteStream &)` / `writeStream(ByteStream &)` |
| `FileFormatSNDFile.hpp` | libsndfile via `sf_open_virtual` (ByteStream callbacks) |
| `FileFormatJSON.hpp` | JSON procedural audio (ByteStream → string → SFXScript) |
| `FileFormatMIDI.hpp` | MIDI file parser with SF2 support (ByteStream → istringstream) |
