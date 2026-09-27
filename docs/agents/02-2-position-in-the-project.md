## 2. Position in the project

```
ext-deps-generator   → builds external deps (zlib, sndfile, freetype, jsoncpp, …)
                       as per-config static libs under output/<config>/{include,lib}
        ↓ (symlinked into dependencies/)
emeraude-base        → THIS repo. Base utilities + platform/config headers.
                       Single source of truth for resolving the external deps
                       (exposes EMERAUDE_EXT_LIBS_PATH / _INCLUDE_DIR / _LIB_DIR).
        ↓ (add_subdirectory)
consumers            → emeraude-engine (full), standalone tools (per-module),
                       projet-alpha (through the engine).
```
