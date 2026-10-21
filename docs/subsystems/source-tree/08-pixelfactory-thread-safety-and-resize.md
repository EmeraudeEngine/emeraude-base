## PixelFactory: Thread Safety and Resize

### TextProcessor and Pixmap During Resize

During window resize, Pixmap dimensions can change between frames. `TextProcessor` must be resilient:

**Implemented protection:**
1. `TextProcessor::setPixmap()` calls `updateMetrics()` to recalculate `maxRows`; the row width is read from the
   rectangle at each `write()` (the layout is in pixels since 2026-10-18, there is no column count)
2. `blitCharacter()` uses `blendFreePixel()` (ignores out-of-bounds pixels) instead of `blendPixel()` (assert)
3. Notifier checks `pixmap.width() == 0 || pixmap.height() == 0` before rendering

**Code references:**
- `PixelFactory/TextProcessor.hpp:setPixmap()` - Calls `updateMetrics()` after pixmap change
- `PixelFactory/TextProcessor.hpp:blitCharacter()` - Uses `blendFreePixel()` for bounds-safety
- `PixelFactory/Pixmap.hpp:blendPixel()` - Assert on coordinates (development)
- `PixelFactory/Pixmap.hpp:blendFreePixel()` - Silently ignores out-of-bounds (production)

### Resampling a TEXTURE is not resizing an image (2026-10-20)

`Processor::resize()` (Nearest / Linear / Cubic) maps the CORNERS — destination x reads the source at
`x · (w − 1) / W` — and reads Black past the border (`safePixel()`). That suits a display resize, not a texture:
on a tiled texture it shifts the content by up to half a source texel and darkens every tile edge.

`Processor::resampleTexture(source, width, height, destination, wrap, pool)` is the texture resample: texel centres
aligned (`(x + 0.5) · w / W − 0.5`), the taps past an edge WRAPPING (repeat addressing, `wrap = true`) or clamped,
separable Catmull-Rom. Found by the engine's texture packing (a 512² opacity mask upscaled to its 1024² albedo).
Tests: `PixelFactoryProcessor.resampleTexture*` (a constant stays constant up to its edges, centres aligned, wrap
vs clamp, refusals).

### Colour under transparent pixels: `Processor::fillTransparentColour()` (2026-10-21)

`fillTransparentColour(pixmap, alphaThreshold)` dilates the colour of an RGBA pixmap under its holes (alpha below the
threshold): pull-push hole filling (Gortler, Grzeszczuk, Szeliski, Cohen, *The Lumigraph*, SIGGRAPH 1996) — the visible
colours averaged down a pyramid, pushed back into the holes. The alpha and the visible pixels are never touched; a
pixmap with nothing to fill or nothing to fill it with is left as it was; a zero threshold or a non-RGBA pixmap is
refused. Why: a texture keeps black under its transparent pixels and every filter (a box-filtered mip, a bilinear tap)
mixed it into the visible edge — the engine's BC7 chain dilates first (its graphics § 39 § Baked assets: Sponza's cypress
leaves kept 56 % of their green at mip 8). Tests: `PixelFactoryProcessor.fillTransparentColour*` (8: holes take the visible
colour, a 1 × 1 box level keeps it where the raw one halves it, visible pixels kept, nearest side wins, the threshold
both ways on an odd size, full / empty masks, refusals).

### Channel packing: `Pixmap::copyChannel()` (2026-10-20)

`copyChannel(channel, source, sourceChannel)` copies one channel of another pixmap of the SAME size into a channel of
this one, values exact (no conversion); a grey-scale source has one channel, named red; another size or a channel
either pixmap lacks is refused, nothing written. Not `fillChannel(channel, pattern, …)`: that one converts the
pattern to a luminance, and its `SingleChannel` mode cannot read the alpha. Tests:
`PixelFactoryPixmap.copyChannel*`.
