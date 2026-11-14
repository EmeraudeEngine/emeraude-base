## PixelFactory: FileFormatPFM — a float pixmap written value for value (2026-11-14)

`src/PixelFactory/FileFormatPFM.hpp` (header-only) writes a Portable FloatMap (`.pfm`, the format of Paul Debevec's
HDR Shop): a text header, then raw 32-bit IEEE 754 floats. `FileIO::write()` reaches it by the `.pfm` extension.

- **Why it exists**: the Radiance `.hdr` writer keeps about 1 % of relative precision (RGBE) and has no value for a
  NaN or an infinity. A depth read back from the GPU needs its exact values (0.990 and 0.995 must differ): the
  engine's render-target dump writes its shadow maps with this (owner decision 2026-11-14).
- **The file**: `Pf` (one channel) or `PF` (three), `<width> <height>`, `-1.0` (a negative scale states
  little-endian floats), each on its line; then the rows from the BOTTOM one (the pixmap's origin is its top-left
  corner; `WriteOptions::invertYAxis` writes the rows in the pixmap's own order). Each float is written byte by byte
  from its bit pattern: little-endian whatever the host, and a signed zero, a subnormal, an infinity or a NaN go
  through untouched.
- **Refused, never converted**: a pixmap which is not made of `float`; a channel mode other than `Grayscale` or
  `RGB` (the format has no alpha channel, and one is never dropped in silence — the Radiance writer does drop it).
  One error line each, nothing written.
- **No reader**: `readStream()` refuses and says so. A reader is a parser of untrusted input: to add with its fuzz
  target the day something in the cascade reads a PFM.

**Tests** (`test_PixelFactoryFileFormats.cpp`, written first — the header did not exist):
`pfmWritesAGrayscalePixmapExactly` and `pfmWritesAnRGBPixmapBottomRowFirst` compare the bytes written with the bytes
the format prescribes (an infinity, a signed zero, a subnormal, a NaN; the row order, and its inversion),
`pfmRefusesWhatItCannotHold` (an RGBA pixmap, an 8-bit pixmap, a read: three error lines, nothing written),
`pfmThroughFileIO` (the `.pfm` extension).
