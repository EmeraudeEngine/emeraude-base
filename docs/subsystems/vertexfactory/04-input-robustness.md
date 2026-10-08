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
- **OBJ**: a face index that references a non-existent vertex is bounds-checked **before** the
  access (was `std::vector::at` → `out_of_range` → terminate). `resolveIndex()` widens to `int64_t`
  so a list larger than `INT_MAX` cannot wrap (the former `int32_t` cast was UB).
- **Diagnostics**: all `std::cerr` in the parsers, FileIO dispatch and StreamIO migrated to the
  `EmEn::Base::Logging` hook (no raw `cerr` in this module).
