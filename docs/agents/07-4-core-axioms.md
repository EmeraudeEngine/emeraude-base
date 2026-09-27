## 4. Core Axioms

1. **Agnostic foundation.** Base depends on NOTHING high-level. No engine subsystem
   (Graphics, Scenes, Physics, Audio runtime, …) may ever be `#include`-d here. The
   dependency arrow only points *into* Base, never out.
2. **External deps via ext-deps-generator only.** Never hardcode or vendor an external
   library path; resolve through `EMERAUDE_EXT_LIBS_PATH`.
3. **Stability matters.** Everything depends on Base — a bug here ripples everywhere.
   Exhaustive tests, careful API.
