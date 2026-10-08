---
id: httpserver-tests-share-fixed-temp-files
title: Two HTTPServer tests share fixed temp file names, so parallel runs of the suite collide
status: open
priority: high
scope: src/Testing/test_NetworkHTTPServer.cpp
opened: 2026-10-08
tags: [tests, network, defect]
---

# Two HTTPServer tests share fixed temp file names, so parallel runs of the suite collide

## Why
Found by the macOS peer on base `26128bc` (8 copies of the test binary in parallel, 20 repeats each):
`NetworkHTTPServer.ServesFilesWithRanges` failed 115 times and `CleartextDownloadFromAPrivatePeer` 94 times out of 160.
Both use FIXED paths in the shared temp directory — `patternFile()` writes
`temp_directory_path() / "emeraude-httpserver-test-<size>.bin"` (truncating it), and the download test uses
`temp_directory_path() / "emeraude-httpserver-test-download.bin"` — and each removes its file at the end, so concurrent
processes truncate or delete each other's file (`expected.size() 0`, `file_size … 18446744073709551615`). `ctest -j`
never runs the same test twice at once, so the normal gate stays green. Paths since `a534436` (2026-10-04).

## What remains
- A per-process unique name (pid, or a random suffix) for every temp file / directory of the suite; grep the other test
  files for the same pattern (`temp_directory_path() /` with a fixed leaf).
- Proof: the 8-copy run green on macOS.

## References
- macOS peer report, 2026-10-08.
