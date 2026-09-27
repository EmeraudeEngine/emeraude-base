## 3. CMake architecture — which target to link

> [!IMPORTANT]
> **Targets available NOW (link these):**
> - **`emeraude::base`** — the whole foundation (umbrella, `STATIC` or `SHARED` per
>   `EMERAUDE_BASE_LIBRARY_TYPE`). **This is the one to use** for any consumer that wants
>   math + factories + I/O + threading, etc. The engine links this.
> - **`emeraude::base::platform`** — header-only `INTERFACE` target, just the
>   platform/arch/OS detection (`emeraude_platform.hpp`). Link it when you need *only* that.
>
> **Do NOT link `emeraude::base::math`, `::io`, `::pixel`, … yet — they do NOT exist.**
> The per-module split is **planned** (see §7 and [`docs/module-map.md`](../module-map.md)):
> until it lands, the single umbrella `emeraude::base` is the way to consume the library.

Planned design (internal refactor, no API break — `emeraude::base` keeps working):
- Header-only modules → `INTERFACE` targets; compiled modules → `OBJECT` libraries; each
  aliased `emeraude::base::<name>` and aggregated into the `emeraude::base` umbrella, so a
  consumer will be able to link only what it needs (e.g. `emeraude::base::math`).
