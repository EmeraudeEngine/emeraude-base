## 3b. Third-party symbols must never leave the binary that links them

emeraude-base owns the vendored external dependencies, so it also owns the rule that keeps them
from leaking. **`cmake/HideThirdPartyExports.cmake`** exposes
`emeraude_base_target_hide_third_party_exports(<target> [EXCEPT <stem>…])`, which adds
`--exclude-libs` for every archive in `EMERAUDE_EXT_LIBS_LIB_DIR` so their symbols become **local**
in the produced binary.

> [!CAUTION]
> **The defect it closes, measured 2026-08-22.** On ELF the dynamic namespace is **flat**. The
> engine's shared library exported **372 `png_*` symbols** from its static libpng **1.6.58**, while
> the same process also loaded the system **libpng16.so.16 (1.6.48)** behind CEF (cairo and
> FreeType pull it in). libEmeraude sits earlier in the global search scope, so **cairo, FreeType
> and gdk-pixbuf all called OUR libpng**. Two consequences: libdecor compares what the global scope
> resolves `png_free` to against what its GTK3 plugin's own chain resolves it to, finds two
> different addresses, prints `Plugin "GTK3 plugin" uses conflicting symbol "png_free".` and falls
> back to cairo decorations; and — the silent half — the system libpng reaches its own entry points
> through the PLT (107 `JUMP_SLOT` relocations, no `-Bsymbolic`), so 1.6.48 code paths could execute
> 1.6.58 implementations on 1.6.48-allocated structures. Same exposure for zlib, FreeType, libjpeg,
> harfbuzz, brotli, lzma, zstd, bz2, tiff, sndfile/FLAC/ogg/vorbis/opus, and **LibreSSL**, whose
> OpenSSL-compatible names would interpose the system OpenSSL used by CEF and glib.
>
> Result on the engine: **44127 → 15518 exported symbols**, every `png_`/`jpeg_`/`FT_`/`sf_`/
> `TIFF`/`ktx`/`meshopt_`/`ufbx_`/`SSL_`/`EVP_` symbol gone, `EmEn::` and `glfw*` untouched.

**Two halves, both required.**

1. **Hide at link.** Every binary of the process calls the helper — not just the shared library. An
   ELF **executable** exports a symbol it defines as soon as a shared object of its link closure
   references it: `fontconfig → libfreetype.so.6 → libpng16` made projet-alpha export **250 `png_*`
   symbols** and kept libdecor refusing its plugin *after* the engine was clean.
2. **Never inline a third-party codec into a header.** A codec defined in a header is compiled into
   **every consumer**, which then defines those symbols in its own binary. `FileFormatPNG`,
   `FileFormatJpeg`, `FileFormatTIFF`, `Font::readTrueTypeFile` and `FileFormatSNDFile< int16_t >`
   are therefore **defined in `.cpp`** and explicitly instantiated there — which is what turned
   `pixel` into an OBJECT module. **A new third-party-backed codec follows the same rule.**

> [!IMPORTANT]
> `EXCEPT` is for an archive whose symbols a consumer legitimately resolves **from** the binary.
> Today there is exactly one: **jsoncpp** (measured — projet-alpha references `Json::Value` from
> libEmeraude), and it is safe precisely because jsoncpp has **no system twin** to interpose. A C
> library that exists on the system must never be excepted.

> [!NOTE]
> **Not needed on the other platforms, and for a stated reason.** MSVC: nothing is auto-exported
> (`EMERAUDE_USE_EXPLICIT_EXPORTS` / `EMEN_API` drives the `.def`). Apple: dyld's **two-level
> namespace** records which library must provide each symbol, so a static copy inside a dylib cannot
> interpose anything — and ld64 has no `--exclude-libs`. The helper is a no-op on both.

**How to check it stayed fixed** (any ELF binary of the cascade):

```bash
nm -D --defined-only libEmeraude.so.0 | grep -cE ' (png_|jpeg_|FT_|sf_|deflate|SSL_)'   # must print 0
nm -D --defined-only projet-alpha     | grep -cE ' (png_|jpeg_|FT_|deflate)'            # must print 0
```
