## Module Overview

**Geometry foundation** - Provides mesh data containers, procedural generation, file loading
(OBJ/STL/MDx/native), processing (decimation, hole-filling, splitting) and analysis. All
CPU-side geometry in the engine flows through VertexFactory `Shape`. glTF/FBX are **not** here
— those are engine-level `Loaders` (fastgltf/ufbx); VertexFactory is the format-agnostic
in-memory representation they feed into.
