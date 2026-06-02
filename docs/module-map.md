# Module Map

> **Status today:** the per-module split is **DONE** (Ave robustus! gap #1, 2026-06-03). Every
> `src/` subsystem is its own CMake target, aliased `emeraude::base::<name>`: header-only modules
> are **INTERFACE** libraries, compiled modules are **OBJECT** libraries aggregated into the
> **`emeraude::base`** umbrella. Consumers link the whole umbrella **or** an individual module.

Mechanism: a shared `emeraude_base_flags` INTERFACE target carries the compile requirements (own
headers, generated config header, ext-deps system includes, C++ standard, definitions/options,
link dir, sanitizer opts). Each OBJECT module links it (PIC) and is pulled into the umbrella via
`$<TARGET_OBJECTS:…>`; the umbrella links the ext **libraries**. Granular-linking caveat: a single
OBJECT module references symbols in sibling modules (e.g. everything uses `core`'s Logging), so
linking one module target standalone may also require its siblings — the umbrella resolves them all.

| Module | Target | Kind | External deps | Status |
|--------|--------|------|---------------|--------|
| platform | `emeraude::base::platform` | INTERFACE | — | DONE |
| math | `emeraude::base::math` | INTERFACE | — | DONE |
| algorithms | `emeraude::base::algorithms` | INTERFACE | — | DONE |
| animation | `emeraude::base::animation` | INTERFACE | — | DONE |
| pixel | `emeraude::base::pixel` | INTERFACE | PNG, JPEG, Freetype | DONE (header-only — PixelFactory is all templates; corrected from OBJECT) |
| core | `emeraude::base::core` | OBJECT | — | DONE |
| hash | `emeraude::base::hash` | OBJECT | — | DONE |
| gametools | `emeraude::base::gametools` | OBJECT | — | DONE |
| time | `emeraude::base::time` | OBJECT | — | DONE |
| debug | `emeraude::base::debug` | OBJECT | — | DONE |
| compression | `emeraude::base::compression` | OBJECT | ZLIB, LZMA | DONE |
| io | `emeraude::base::io` | OBJECT | libzip | DONE |
| network | `emeraude::base::network` | OBJECT | ASIO | DONE |
| vertex | `emeraude::base::vertex` | OBJECT | — | DONE |
| wave | `emeraude::base::wave` | OBJECT | sndfile, samplerate, TinySoundFont | DONE |

**core** = the flat `src/` root files (ThreadPool, FastJSON, INIParser, String, ObservableTrait,
ObserverTrait, SourceCodeParser, TokenFormatter, Variant, FileTimestamps) + the `Logging` hook.
`emeraude_platform.hpp` and `emeraude_config.hpp` are root config headers, not part of a module
(included without prefix). Source lists per module live in `cmake/PrepareBaseSourceFiles.cmake`
(`EMERAUDE_BASE_<MODULE>_SOURCES`).