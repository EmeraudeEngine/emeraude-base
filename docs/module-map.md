# Module Map

> **Status today:** only **`emeraude::base`** (umbrella — the whole library) and
> **`emeraude::base::platform`** (header-only) are defined and linkable. The per-module
> targets below (`math`, `io`, `pixel`, …) are a **planned** internal refactor — they do
> NOT exist yet. Until then, consumers link `emeraude::base`.

The plan: each `src/` subsystem becomes an independent CMake target, aliased
`emeraude::base::<name>`, and aggregated into the `emeraude::base` umbrella.

| Module | Target | Kind | External deps | Status |
|--------|--------|------|---------------|--------|
| platform | `emeraude::base::platform` | INTERFACE | — | DONE |
| math | `emeraude::base::math` | INTERFACE | — | TODO |
| algorithms | `emeraude::base::algorithms` | INTERFACE | — | TODO |
| animation | `emeraude::base::animation` | INTERFACE | — | TODO |
| core | `emeraude::base::core` | OBJECT | — | TODO |
| hash | `emeraude::base::hash` | OBJECT | — | TODO |
| gametools | `emeraude::base::gametools` | OBJECT | — | TODO |
| time | `emeraude::base::time` | OBJECT | — | TODO |
| debug | `emeraude::base::debug` | OBJECT | — | TODO |
| compression | `emeraude::base::compression` | OBJECT | ZLIB, LZMA | TODO |
| io | `emeraude::base::io` | OBJECT | libzip | TODO |
| network | `emeraude::base::network` | OBJECT | ASIO | TODO |
| pixel | `emeraude::base::pixel` | OBJECT | PNG, JPEG, Freetype | TODO |
| vertex | `emeraude::base::vertex` | OBJECT | — | TODO |
| wave | `emeraude::base::wave` | OBJECT | sndfile, samplerate, TinySoundFont | TODO |

**core** = the flat `src/` root files (ThreadPool, FastJSON, KVParser, String,
ObservableTrait, ObserverTrait, SourceCodeParser, TokenFormatter, Variant,
FileTimestamps). `emeraude_platform.hpp` and the future `emeraude_config.hpp` are
root config headers, not part of a module (included without prefix).