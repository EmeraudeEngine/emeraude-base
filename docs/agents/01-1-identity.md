## 1. Identity

**emeraude-base** is the **foundation library** of the Emeraude-Engine project: the
agnostic, reusable core (math, I/O, hashing, factories, traits, threading, …) that
sits *below* the engine runtime.

> [!IMPORTANT]
> **Mission — a turnkey starting point for multimedia applications.** Think of it as a
> **"STL++"**: what the C++ standard library would offer if it covered the multimedia
> domain. It bundles the painful base layer every cross-platform multimedia/graphics
> project re-solves from scratch — vectors/matrices/quaternions & geometry, image / audio /
> mesh loading and processing, hashing, compression, threading, parsing — *plus* a managed,
> prebuilt set of external dependencies for Linux/macOS/Windows. A developer can start a
> real application on top of it instead of a blank `CMakeLists.txt`. The Emeraude-Engine is
> the most prominent consumer, but emeraude-base is meant to be a project starter in its
> own right.

- **Standalone repository.** Independent of `emeraude-engine`, NOT a git submodule.
- **Namespace: `EmEn::Base`.** A file at `src/Foo/Bar.hpp` is `#include "Foo/Bar.hpp"`
  (no prefix in the include path), namespace `EmEn::Base::Foo`. This is the engine's
  former `EmEn::Libs::*`, with `Libs` renamed to `Base`. The `EmEn` root is shared with
  `emeraude-engine`.
- **Language:** C++20. **Indentation:** tabs.
- **Platform:** cross-platform strict (Linux, macOS, Windows).

> [!IMPORTANT]
> **A real split, not a sub-folder.** emeraude-base and emeraude-engine are two separate
> repositories with **independent lifecycles**. emeraude-base **evolves on its own** — its
> own versioning and roadmap, new formats/capabilities added without waiting on the engine.
> The engine **builds on a pinned emeraude-base package**: it consumes the base, it does
> not drive its evolution. Using the engine implies using base, but base is equally
> consumable on its own — a tool, a baker, a converter may need only `EmEn::Base::Math` or
> only image handling, **without the full engine** (no Vulkan, no GLFW, no OpenAL).
