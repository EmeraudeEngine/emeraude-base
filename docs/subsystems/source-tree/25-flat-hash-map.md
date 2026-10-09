## FlatHashMap — a hash map in one flat array, deterministic on every platform (2026-10-08)

`src/FlatHashMap.hpp` (header-only, `EmEn::Base::FlatHashMap`, `PortableHash`, `mixHash()`). Owner decision
(2026-10-08, Ave Robustus II D7): a reusable base container, created for the mesh decimator.

### Why
A node-based `std::unordered_map` / `std::unordered_set` allocates one node per entry: a million-entry table costs a
million allocations to fill and as many frees to release. That release alone held a CANCELLED decimation of a
2.24 M-triangle mesh for 0.3 to 1.5 s (Linux / Windows) — the stop bound D7 is 50 ms. And `std::hash` is
implementation-defined (the identity under libstdc++, FNV-1a under MSVC): iterating such a table visits its entries in
a different order on each standard library, so any floating-point sum or tie-break taken in that order differs between
the three OS.

### What it is
- Open addressing with linear probing (Knuth, TAOCP vol. 3, § 6.4, Algorithm L), a power-of-two slot count kept at most
  half full; ONE `std::vector` of slots; no erase (the uses build a table, read it, drop it).
- `FlatHashMap(expectedCount)` / `reserve()` allocate once: a reserved map never grows. Growth past it rehashes
  (doubling), so an unknown count still works.
- `tryEmplace(key, value)` → `{value *, inserted}` (the first value is kept), `operator[]`, `find()` → pointer or
  nullptr, `contains()`, `forEach(function)` in slot order, `size()`, `slotCount()`, `clear()` (keeps the slots).
- `PortableHash< key_t >` (the default, integral and enumeration keys) is `mixHash()`, the splitmix64 finalizer
  (Steele, Lea, Flood, OOPSLA 2014; S. Vigna's public-domain constants): the same layout and `forEach()` order on every
  platform. Golden values in `test_FlatHashMap.cpp`.
- `FlatHashMap(expectedCount, hash, equal)` takes functors WITH state: the decimator keys its table by source vertex
  INDEX and hashes / compares the quantized positions those indices point to, so a slot is 12 bytes instead of 32.

### Traps
- Pointers and references to values are invalidated by a growth (an insertion past the reserved count) and by
  `clear()`.
- Not thread-safe.
- A slot holds `key_t`, `value_t` and a `bool`: a large key multiplies the table's size by its slot count (twice the
  entries at least) — key by index when the key is large.

### Users
- `VertexFactory::ShapeDecimator::buildWorkMesh()` (position deduplication of the work mesh).
