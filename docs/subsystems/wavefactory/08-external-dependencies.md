## External Dependencies

These are **always available at compile time** (provided by ext-deps-generator / the vendored
submodule) — there are **no `*_ENABLED` compile guards**. The former `LIBSNDFILE_ENABLED`,
`SAMPLERATE_ENABLED` and `TINYSOUNDFONT_ENABLED` macros were removed: code includes and uses these
dependencies unconditionally.

- **libsndfile**: Audio format loading (WAV, FLAC, OGG, etc.). Real static lib, linked into `emeraude::base`.
- **libsamplerate**: High-quality resampling (SRC_SINC_BEST_QUALITY). Real static lib, linked into `emeraude::base`.
- **TinySoundFont**: SoundFont 2 (SF2) sample-based MIDI rendering (header-only, MIT license).
  - **Implementation ownership**: header-only libraries need `#define TSF_IMPLEMENTATION` in exactly
    **one** translation unit. The `emeraude_base` **library deliberately does NOT compile it** — the
    host application owns the single instance (emeraude-engine: `Audio/SoundfontResource.cpp`).
    Defining it in the library too would yield duplicate `tsf_*` symbols at the host's final link.
  - For base's own unit tests (which instantiate `FileFormatMIDI`, whose `renderToWave()` odr-uses
    the SF2 path), the implementation is compiled into the **test binary only** via
    `src/Testing/TinySoundFontImpl.cpp` (added to `EMERAUDE_BASE_TEST_SOURCES`).
