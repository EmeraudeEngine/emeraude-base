## Unified ByteStream I/O Architecture

All three factories (PixelFactory, WaveFactory, VertexFactory) share a common I/O architecture via `IO/`.

### Core Abstraction: ByteStream

```cpp
// IO::ByteStream — abstract interface for all I/O
class ByteStream {
    virtual bool read(void * buffer, size_t size) noexcept = 0;
    virtual bool write(const void * buffer, size_t size) noexcept = 0;
    virtual bool seek(int64_t offset, SeekOrigin origin) noexcept = 0;
    virtual int64_t tell() const noexcept = 0;
    virtual int64_t size() const noexcept = 0;
    virtual bool isOpen() const noexcept = 0;
    // ...
};
```

Two concrete implementations:
- **FileStream** — wraps `std::ifstream`/`std::ofstream` with `Mode::Read`/`Mode::Write`
- **MemoryStream** — operates on `std::vector<std::byte>` (write) or `const std::byte *` (read), with position-based random access

### Factory I/O Pattern

Each factory exposes two namespaces:

| Namespace | Purpose | Stream type |
|-----------|---------|-------------|
| `FileIO::read/write` | File-backed I/O (path-based) | `IO::FileStream` |
| `StreamIO::read/write` | Memory buffer I/O | `IO::MemoryStream` |

Both delegate to `FileFormatInterface::readStream(ByteStream &, ...)` / `writeStream(ByteStream &, ...)`.

### Format Handler Strategies

| Strategy | Used by | How it works |
|----------|---------|--------------|
| **Direct ByteStream** | Native (ee3d), STL binary write | Binary read/write directly on the stream |
| **sf_open_virtual** | libsndfile (WAV/FLAC/OGG) | Virtual I/O callbacks delegate to ByteStream |
| **istringstream adapter** | OBJ, STL, MDx, JSON, MIDI | Read full stream to memory, wrap in `std::istringstream` for text-based parsing |
| **ostringstream adapter** | OBJ write | Build text output in `std::ostringstream`, write string to ByteStream |

### Whole files and byte ranges (`IO/IO.hpp`)

`IO::fileGetContents()` reads a whole file; **`IO::fileGetRange(path, offset, length, bytes)`** (2026-10-04) reads
`length` bytes from `offset` — the engine re-reads an image embedded in a model file (a `.glb`'s BIN chunk, an
external `.bin`, an uncompressed USDZ entry) without reading the model. A range reaching past the end of the file
(the file changed, or a wrong range), an empty range or a 64-bit overflow is REFUSED, never shortened. Pinned by
`IOFileUtils.getRangeReadsExactlyTheRange` and `.getRangeRefusesEveryRangeOutOfTheFile`.

### Code References

| File | Description |
|------|-------------|
| `IO/ByteStream.hpp` | Abstract polymorphic stream interface |
| `IO/FileStream.hpp` | File-backed implementation |
| `IO/MemoryStream.hpp` | Memory-backed implementation |
| `PixelFactory/FileIO.hpp` | Image file dispatcher |
| `PixelFactory/StreamIO.hpp` | Image memory buffer I/O |
| `WaveFactory/FileIO.hpp` | Audio file dispatcher |
| `WaveFactory/StreamIO.hpp` | Audio memory buffer I/O (libsndfile only) |
| `VertexFactory/FileIO.hpp` | Geometry file dispatcher (Native, OBJ, STL, MDx) |
| `VertexFactory/StreamIO.hpp` | Geometry memory buffer I/O (Native ee3d only) |
