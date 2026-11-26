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
| **Windowed line reader** (`IO::StreamLineReader`) | OBJ | A stream that seeks: walked line by line through a 1 MiB window, never held whole; fallback `IO::LineReader` over the stream read whole |
| **Windowed istream** (`IO::ByteStreamBuffer`) | STL, MDx | A stream that seeks: a `std::istream` on a `ByteStreamBuffer` reads it through a 1 MiB window (seekg / tellg included); fallback `IO::MemoryStreamBuffer` over the stream read whole |
| **In-place istream** (`IO::MemoryStreamBuffer`) | MIDI | Read full stream to memory (the RMID wrapper and its DLS bank read the bytes directly), a `std::istream` reads it in place |
| **String** | JSON (WaveFactory) | Read full stream to a string, hand it to `SFXScript::generateFromString()` |
| **ostringstream adapter** | OBJ write | Build text output in `std::ostringstream`, write string to ByteStream |

### Reading a buffer in place: `IO::LineReader` and `IO::MemoryStreamBuffer` (2026-11-26)

The readers above used to wrap the buffer they had just read in a `std::istringstream`. ⚠️ **That constructor COPIES
its string**: the file was held twice for the whole parse. Measured on a 312 MB OBJ (3.9 M vertices, 7.8 M triangles,
Release, Linux): peak 1461 MiB, of which 2 × 298 MiB was text; **1163 MiB** without the copy. Two header-only
utilities replace it, and nothing else in the readers changed:

- **`IO::LineReader`** (`IO/LineReader.hpp`) walks a text held in memory line by line, every line a `std::string_view`
  into it, with exactly `std::getline()`'s cut ('\n' dropped, a '\r' before it kept, a last line without '\n'
  returned, nothing after a final '\n'); `rewind()` starts a second pass. It also saves the `std::string` each line was
  copied into: the OBJ read above took **−16 %** of its time (1129 → 944 ms). Used by OBJ, whose two passes are pure
  line work.
- **`IO::MemoryStreamBuffer`** (`IO/MemoryStreamBuffer.hpp`) is a read-only `std::streambuf` over memory it does not
  own: `std::istream input{&buffer}` reads, `seekg()`s and `tellg()`s in place, a position outside the block being
  refused (failbit, position unchanged, no overflow of the sum). Used by STL, MDx and MIDI, whose binary halves need a
  real stream. Same memory gain; no speed gain (a 2 % change on the same OBJ through it). A 390 MB binary STL
  (7.8 M triangles): peak 3274 → **2902 MiB**, the file's size.

Both require the memory to outlive them. Pinned by `IOLineReader.*` (the cut compared with `std::getline()` on 14
texts, lines as views into the text, rewind) and `IOMemoryStreamBuffer.*` (reads in place, reads and seeks as a
`std::istringstream`, refuses a seek outside the block, read past the end, empty block).

**Then the window (2026-11-26, owner decision: a window for every source, a fallback for a stream that cannot seek).**
`IO::ByteStreamBuffer` (`IO/ByteStreamBuffer.hpp`) is a read-only `std::streambuf` over a `ByteStream`: a window of 1 MiB
refilled by `ByteStream::read()`, a seek inside the window moving within it and outside it asking `ByteStream::seek()`.
`IO::StreamLineReader` (`IO/StreamLineReader.hpp`) is `IO::LineReader`'s cut over a `ByteStream` through such a window,
`rewind()` seeking back for the second pass; ⚠️ its line is valid until the next call only (the window is refilled and
doubles when a line outgrows it), where `IO::LineReader`'s lives as long as its text — two classes on purpose, so each
contract stays simple. Both see the stream from its position at construction to its end, and are usable only when the
stream tells its position and seeks to it (`isUsable()`; `FileStream` and `MemoryStream` do): the readers fall back to
the stream read whole otherwise. A `MemoryStream` goes through the window too: reading its bytes in place would take a
`const_cast` (`std::streambuf::setg()` takes `char *`, `ByteStream::data()` gives `const std::byte *`), refused by the
static-analysis gate. Measured: the 312 MB OBJ **822 MiB** (with the OBJ's V mode writing its vertices directly, no position
table: −45 MiB) and 897 ms; the 390 MB STL **2532 MiB**. Pinned by `IOByteStreamBuffer.*`, `IOStreamLineReader.*` (window
sizes down to one byte, a line longer than the window, the stream position, rewind, a stream that cannot seek, a failed
read), and the readers' fallback tests (`VertexFactoryOBJ.aStreamThatCannotSeekIsReadWhole`,
`VertexFactorySTL.aStreamThatCannotSeekIsReadWhole`, `VertexFactoryMDx.md3NormalsFollowQuake3`) through
`Testing/StreamDoubles.hpp`.

What still dominates the peak once the file is no longer held: the read buffer itself (one × the file), the reader's staging
tables, and the `Shape` (`ShapeVertex` 92 bytes, `ShapeTriangle` 64 bytes — 818 MiB of the 1163 on that OBJ; on the
STL above, whose reader does not weld the corners, 23.4 M vertices: about 2 GiB of `ShapeVertex`).

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
| `IO/MemoryStreamBuffer.hpp` | Read-only `std::streambuf` over memory: a `std::istream` reading a buffer in place |
| `IO/LineReader.hpp` | Line-by-line walk of a text in memory, `std::getline()`'s cut, lines as `std::string_view` |
| `IO/ByteStreamBuffer.hpp` | Read-only `std::streambuf` over a `ByteStream` through a refilled window (seekable streams) |
| `IO/StreamLineReader.hpp` | `LineReader`'s cut over a `ByteStream` through a refilled window; a line lives until the next call |
| `PixelFactory/FileIO.hpp` | Image file dispatcher |
| `PixelFactory/StreamIO.hpp` | Image memory buffer I/O |
| `WaveFactory/FileIO.hpp` | Audio file dispatcher |
| `WaveFactory/StreamIO.hpp` | Audio memory buffer I/O (libsndfile only) |
| `VertexFactory/FileIO.hpp` | Geometry file dispatcher (Native, OBJ, STL, MDx) |
| `VertexFactory/StreamIO.hpp` | Geometry memory buffer I/O (Native, OBJ, STL, MDx: the format is given, a buffer has no extension) |
