## 6b. PixelFactory — supported image formats

| Format | Read | Write | Backend |
|--------|------|-------|---------|
| JPEG (`.jpg`, `.jpeg`) | ✅ | ✅ | libjpeg-turbo |
| PNG (`.png`) | ✅ | ✅ | libpng |
| Targa (`.tga`) | ✅ | ✅ | in-house |
| HDR / RGBE (`.hdr`) | ✅ | ✅ | in-house — the only floating-point citizen |
| **TIFF (`.tif`, `.tiff`)** | ✅ | ❌ | **libtiff (added 2026-08-09)** |

> [!WARNING]
> **An in-house reader MUST bound the declared dimensions against the stream payload BEFORE
> `Pixmap::initialize()`.** The library is built `-fno-exceptions`, so `initialize()`'s
> `m_data.resize()` cannot *fail*: a throwing `operator new` is `std::terminate`. Its `bool` return
> can never report an allocation failure — the header check is the only line of defence. Both
> in-house readers shipped without it and both were caught: Targa by `fuzz_targa` (~17 GB from an
> 18-byte header), HDR by the unit suite on a smaller-RAM host (**51.5 GB** from a 40-byte stream,
> 2026-08-28). The pattern to copy, the max-expansion factor and its trade-off are in
> [`docs/caution-points.md`](../caution-points.md) § PixelFactory. ⚠️ A `…DoesNotOOM` test going
> green proves nothing on a big-RAM host — verify peak RSS and re-run under `ulimit -v`.

**Why TIFF was added, breaking the "Ave robustus!" freeze on purpose (owner decision,
2026-08-09):** Intel's Jungle Ruins — the engine's gold-goal scene — stores the base colour and
translucency of its **entire vegetation** as 16-bit TIFF. Ten files, all under
`textures/Plants/`. Without a decoder those materials fall back to a flat colour and every plant
renders lit, detailed and pure WHITE, however much work goes into instancing. Pre-converting the
asset was excluded by a standing owner rule: a load failure is an engine defect to fix, never an
asset to pre-chew.

> [!IMPORTANT]
> **The codecs backed by a third-party library are COMPILED, not header-only** (since 2026-08):
> `FileFormatPNG.cpp`, `FileFormatJpeg.cpp`, `FileFormatTIFF.cpp` and `Font.cpp` hold the bodies and
> the explicit instantiations (`< uint8_t, uint32_t >`; `Font< uint8_t >`), and their headers include
> neither `png.h`, `jpeglib.h`, `tiffio.h` nor FreeType. Targa and HDR stay header-only — they are
> in-house, with nothing to leak. Adding a pixel type therefore means **adding an instantiation
> line** in the matching `.cpp`; forget it and the failure is a LINK error naming the exact symbol,
> never a silent fallback. The reason this is not negotiable is in § 3b.

> [!WARNING]
> **TIFF is read-only here, and deliberately.** It is an interchange format — DCC tools emit it,
> the engine consumes it. PNG covers lossless output, HDR covers floating point.

> [!CAUTION]
> **A 16-bit TIFF is DOWN-CONVERTED to 8 bits per channel.** `FileFormatTIFF` decodes through
> `TIFFReadRGBAImageOriented()` rather than the strip/tile API, because TIFF is a container
> rather than a format: any bit depth, any photometric interpretation, a dozen compressions,
> strips or tiles, planar or interleaved. That entry point collapses all of it to 8-bit RGBA,
> which is what a texture becomes anyway. Full precision would be a separate reader, not a flag.

> [!CAUTION]
> `ORIENTATION_TOPLEFT` is what makes the output canonical. The plain `TIFFReadRGBAImage()`
> returns the image **bottom-up**, which renders as a vertically mirrored texture — right shape,
> wrong content, nothing in the log.

> [!CAUTION]
> **12-bit JPEG must be disabled TWICE, and the second one is the trap.** `jpeg12: false` only
> guards the "separate 12-bit libjpeg" branch. The other branch probes `jpeglib.h` with
> `check_symbol_exists()`, and **libjpeg-turbo 3.x DECLARES `jpeg12_read_scanlines` while the
> archive we build does not EXPORT it** — libtiff then believes it has dual mode, compiles
> `tif_jpeg_12.c`, and the link dies on `jpeg12_*`. The cache entry
> `HAVE_JPEGTURBO_DUAL_MODE_12: false` is what stops the probe.
>
> ⚠️⚠️ **This did NOT break the cascade build** — the shared library never pulled that object in.
> It surfaced only when linking `EmeraudeBaseUnitTests`. That is precisely why verification is
> done in two steps (cascade, then unit suite): step 1 passing is not evidence that step 2 will.

> [!CAUTION]
> **The WebP and JBIG codecs are OFF in the libtiff build** (`libraries/libtiff.yaml`). libtiff
> enables them whenever it finds the libraries, and the engine then fails to link on
> `WebPGetFeaturesInternal` and `jbg_dec_in`. Deflate, LZMA, Zstd and JPEG ride on libraries the
> cascade already links and stay enabled.

> [!IMPORTANT]
> **A format this table does not list must be rejected BEFORE the resource manager sees it.**
> An `ImageResource` that fails to load takes its texture and then its material down with it, so
> the object stops rendering ENTIRELY — strictly worse than a missing file, which merely falls
> back to a flat colour. `Scenes::Loaders::USDLoader` checks the extension first for that reason.
