---
id: windows-long-paths
title: File IO fails on Windows for paths longer than MAX_PATH (260 characters)
status: in-progress
priority: high
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

## Owner decision (2026-10-07)

BOTH: `\\?\`-prefixed absolute normalized paths inside the base `IO::` wrappers on Windows, AND a `longPathAware` manifest in projet-alpha.

## What remains

Done on Linux (2026-10-07): `IO::systemPath()` in every `IO::` wrapper (the `\\?\` form from 248 characters),
`IO::windowsExtendedLengthPath()` (pure, tested on every OS), `IO::renameFile()` (used by the engine's shader-binary
and pipeline caches), projet-alpha's `long-path-aware.manifest`. Linux: the cascade builds, 2379 / 2379 Release and
ASan + UBSan; `IOSystemPath.shortPathsAreUnchangedAndLongOnesStayUsable` writes, reads, renames and erases a
300+-character path (proves nothing about Windows: Linux has no MAX_PATH).

- [ ] **Windows proof**, without `LongPathsEnabled` first: `EmeraudeBaseUnitTests --gtest_filter=IO*` green (the
      long-path test goes through `\\?\` there), then projet-alpha with a 300-character `--cache-directory`: the shader
      binaries and the pipeline cache are written (0 "Unable to write the shader binary"). Check the manifest is in the
      executable (`mt.exe -inputresource:projet-alpha.exe;#1 -out:con`). Then delete this item.

## References

- `src/IO/IO.cpp` (`filePutContents()` and its siblings).
- Microsoft: "Maximum Path Length Limitation" (the `\\?\` prefix and the `longPathAware` manifest element).
