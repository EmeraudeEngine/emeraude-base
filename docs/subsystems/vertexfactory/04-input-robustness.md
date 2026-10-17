## Input Robustness (Ave robustus! — A.2 / A.3)

The four file-format parsers consume **untrusted input**. Owner directive (shared with
WaveFactory): *malformed input must never crash the engine — cancel the load (`return false`),
nothing fancier.* Hardening landed in the A.2 characterization pass and the A.3 fuzzing pass
(`src/Fuzzing/fuzz_{native,stl,mdx,obj}`), each fix covered by
`Testing/test_VertexFactoryFileFormats.cpp`, green in Release **and** under ASan/UBSan via `ctest`.

- **The shared Tier-1 vuln**: an untrusted count read from a header fed to `resize`/`reserve`/
  `vector(n)` without validation → `std::length_error`/`std::terminate`/OOM under `-fno-exceptions`.
  Native (ee3d) and STL now validate every count against the **remaining stream bytes**
  (overflow-safe, division-first) before allocating.
- **MDx** (read-only legacy MDL/MD2/MD3/MD5): a uniform `exceedsStream()` guard at every alloc
  site, plus the fuzzing fixes — MD2/MDL empty-frame null-deref + unchecked triangle vertex/st/
  normal indices, MD3 OOB index + 64 GB `reserveData` (triangle total bounded vs stream) + offset
  signed-overflow (`int64_t`), MD5 null-deref (derive `jointCount` from `joints.size()`, validate
  weight→joint / vertex→weight / triangle→vertex cross-refs before building).
  2026-10-08: an MDL / MD2 skin width or height that is not positive is refused (it divides every texture
  coordinate: inf / NaN in the geometry), and an MD5 block holding fewer lines than its declared count is refused
  (it used to read the closing brace and the next blocks as entries).
  2026-10-17 (the coverage pass, files that LOAD checked value by value; owner decision for the groups): an MDL skin
  GROUP and frame GROUP (Quake's modelgen.h: a count, intervals, several images / frames) were read as one skin / one
  simple frame and the rest of the file shifted — now read per the format (the skins skipped, a frame group stands for
  its first frame; a group count must be positive and within the stream); a truncated MDL used to load zeros — refused.
  The MD3 normal bytes were SWAPPED: Quake III decodes the short as `lat = byte 1`, `lng = byte 0`, 256 steps per turn
  (ioquake3 `tr_surface.c`, `LerpMeshVertexes`); measured on `fembotbig.md3` through the engine: mean |cos| between the
  vertex and face normals 0.513 before (random), 0.833 after; MDL / MD2 unchanged (0.696 / 0.702). Tests
  `VertexFactoryMDx.mdlLoadsItsVerticesNormalsAndCoordinates`, `mdlTruncatedOrBadGroupIsRefused`, `md3NormalsFollowQuake3`
  (all three failed on the former reader).
- **OBJ**: a face index that references a non-existent vertex is bounds-checked **before** the
  access (was `std::vector::at` → `out_of_range` → terminate). `resolveIndex()` widens to `int64_t`
  so a list larger than `INT_MAX` cannot wrap (the former `int32_t` cast was UB).
- **Diagnostics**: all `std::cerr` in the parsers, FileIO dispatch and StreamIO migrated to the
  `EmEn::Base::Logging` hook (no raw `cerr` in this module).
