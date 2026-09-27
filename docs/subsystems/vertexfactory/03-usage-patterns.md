## Usage Patterns

### Load a mesh from a file (automatic dispatch)

```cpp
VertexFactory::ShapeLoadResult<float, uint32_t> result;
VertexFactory::ReadOptions options;
options.requestNormal = true;
options.requestTextureCoordinates = true;
VertexFactory::FileIO::read("model.obj", result, options);
const auto & shape = result.shape;
```

### Memory buffer I/O (StreamIO) — explicit format

```cpp
// A memory buffer carries no extension, so the format is selected explicitly.
std::vector<std::byte> bytes = /* ... from an archive / network / asset pack ... */;
VertexFactory::ShapeLoadResult<float, uint32_t> result;
VertexFactory::StreamIO::read(bytes, VertexFactory::FileFormatType::OBJ, result);

// Write (Native/OBJ/STL writable; MDx read-only).
std::vector<std::byte> output;
VertexFactory::StreamIO::write(result.shape, VertexFactory::FileFormatType::Native, output);
```
