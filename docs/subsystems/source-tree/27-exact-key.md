## ExactKey — the exact identity of a cached object (2026-11-01)

`src/ExactKey.hpp` (header-only, `EmEn::Base::ExactKey` + its `std::hash` specialization). Owner decision
2026-10-31: the exact key of the engine's caches is a generic class of the base, not an engine-only type.

**Why.** A cache that SERVES what it finds (a shader program, a graphics pipeline) must never hand out one object for
another. A cache keyed by a bare `size_t` hash with no equality behind it does exactly that when two hashes meet, and
says nothing: the engine's program cache served a spot-shadow program for the sun's cascaded pass in 1 run in 6 to 12
(2026-10-31). A hash only PLACES a key; the equality decides.

**Contract.**
- The key is the byte sequence of every input, in the order they are `add()`ed: an integer, a boolean or an
  enumeration widened to 64 bits; a `float` / `double` by its bits (widened); a pointer (a Vulkan handle, an object's
  identity) as its address in 64 bits; a text with its length in front. Two different input lists never give the same
  bytes.
- `ExactKey{domain}` starts with the builder's name (a `ClassId`), so two builders never share a key.
- `add(const char *)` is DELETED: a C string would bind to `add(const void *)` and be keyed by its address. Pass a
  `std::string_view` or a `std::string`.
- `operator==` compares the bytes; `hash()` (and `std::hash< ExactKey >`) is `std::hash< std::string >` over them —
  implementation-defined, so a key is an in-process value: never persist it, never compare it across platforms (the
  bytes are in native endianness too).
- Floats compare by their bits: `-0.0` and `+0.0` are two keys, and so are two NaN payloads — what a creation would
  receive.
- Built when the cached object is created or looked up (a cold path): it allocates.
- A blob (an array of specialization-constant bytes) is added value by value, 8 bytes each; no `add(std::span< const
  std::byte >)` on purpose (owner, 2026-11-01: a raw block would not be told apart from a sequence of values, and the
  use is cold).

**Users.** The engine's `Saphir::Generator::ProgramKey` (an alias), `Vulkan::GraphicsPipeline::getKey()` and the
renderer's `m_programs` / `m_graphicsPipelines` (engine `docs/pipeline-caching-system.md`).

**Tests.** `src/Testing/test_ExactKey.cpp` (7 tests: same inputs same key, one different input, the text boundaries,
the fixed widths, the float bits, the pointer address, and a hash map whose hashes all meet still serving the right
object) and the template guard.
