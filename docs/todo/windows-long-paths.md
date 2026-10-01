---
id: windows-long-paths
title: File IO fails on Windows for paths longer than MAX_PATH (260 characters)
status: open
priority: unranked
scope: IO (filePutContents and the other IO wrappers), the applications' Windows manifest
opened: 2026-10-01
tags: [io, windows, paths]
---

# File IO fails on Windows for paths longer than MAX_PATH (260 characters)

## Why

Found by the Windows peer during the triad 8b / 8c validation (2026-10-01). With a deep `--cache-directory` (its
scratchpad, ~142 characters), 118 shader binaries could not be written:

- `[Error][IO] filePutContents: cannot open …\shader-binaries\RenderableInstanceDirectionalLightPassVertexShader_…bin.tmp for writing`
- then `[Error][ShaderManagerService] Unable to write the shader binary code to file …`

Every failing path was 273 to 282 characters long, every successful one shorter than 260. With short directories
there were 0 errors. Neither the `\\?\` prefix nor a `longPathAware` application manifest is used.

The failure is handled (traced, the shader still runs), and the default `%LOCALAPPDATA%\…\shader-binaries\` paths
(the longest name is ~101 characters) stay under the limit. A long user profile path or a deep cache / data
directory would hit it in any IO (caches, captures, settings).

## What remains

- [ ] Choose the support (an owner decision):
  - a `longPathAware` manifest in the applications (projet-alpha), which also needs the system's `LongPathsEnabled`;
  - or `\\?\`-prefixed absolute paths inside the base `IO::` wrappers on Windows (works without the system setting,
    but needs an absolute, normalized path);
  - or both.
- [ ] Re-test with a 300-character cache path on Windows: the binaries are written.

## References

- `src/IO/IO.cpp` (`filePutContents()` and its siblings).
- Microsoft: "Maximum Path Length Limitation" (the `\\?\` prefix and the `longPathAware` manifest element).
