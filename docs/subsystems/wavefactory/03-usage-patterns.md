## Usage Patterns

### Generate a Mono Sound

```cpp
Wave<int16_t> wave;
Synthesizer synth{wave, sampleCount, Frequency::F44100};  // Always mono
synth.sineWave(440.0F, 0.5F);
synth.applyADSR(0.01F, 0.1F, 0.7F, 0.2F);
```

### Multi-Track Stereo via JSON

```json
{
    "duration": 1000,
    "channels": 2,
    "tracks": [
        {
            "instructions": [
                { "type": "sineWave", "frequency": 440.0, "amplitude": 0.5 },
                { "type": "applyADSR", "attack": 0.01, "decay": 0.1, "sustain": 0.7, "release": 0.2 }
            ]
        },
        {
            "instructions": [
                { "type": "sineWave", "frequency": 554.37, "amplitude": 0.5 },
                { "type": "applyADSR", "attack": 0.01, "decay": 0.1, "sustain": 0.7, "release": 0.2 }
            ]
        }
    ],
    "finalInstructions": [
        { "type": "normalize" }
    ]
}
```

Creates one Synthesizer per track, interleaves tracks for stereo output.

### Process Existing Sound

```cpp
Wave<int16_t> loaded;
loaded.readFile("sound.wav");
Processor processor{loaded};
processor.mixDown();      // Stereo -> Mono
processor.resample(Frequency::F44100);
processor.toWave(loaded); // Write back
```

### Load MIDI File

```cpp
// Via FileIO (automatic dispatch) - additive synthesis
Wave<int16_t> wave;
WaveFactory::ReadOptions options;
options.synthesisFrequency = Frequency::PCM48000Hz;
WaveFactory::FileIO::read("music.mid", wave, options);
```

### Load MIDI with SoundFont (SF2)

```cpp
// High-quality rendering with SF2 sample bank
// Note: tsf* handle typically comes from Audio::SoundfontResource
Wave<int16_t> wave;
WaveFactory::ReadOptions options;
options.synthesisFrequency = Frequency::PCM48000Hz;
options.soundfont = tsfHandle;  // tsf* from SoundfontResource::handle()
WaveFactory::FileIO::read("music.mid", wave, options);
```

### Memory Buffer I/O (StreamIO)

```cpp
// Read from a memory buffer — the format is explicit (a buffer carries no file extension).
// SoundFileFormat::Audio = libsndfile (WAV/FLAC/OGG/…); MIDI and JSON are also reachable (read-only).
std::vector<std::byte> audioData = /* ... loaded from network, archive, etc. */;
Wave<int16_t> wave;
WaveFactory::StreamIO::read(audioData, WaveFactory::SoundFileFormat::Audio, wave);

// Write audio to memory buffer (libsndfile is the only writable format)
std::vector<std::byte> output;
WaveFactory::WriteOptions writeOpts;
writeOpts.format = WaveFactory::AudioFormat::FLAC;
WaveFactory::StreamIO::write(wave, output, writeOpts);
```
