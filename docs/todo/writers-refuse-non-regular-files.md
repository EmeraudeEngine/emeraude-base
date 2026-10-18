---
id: writers-refuse-non-regular-files
title: The writers open a FIFO or a device path without checking it, and block
status: open
priority: high
scope: src/IO/IO.cpp, src/IO/IO.hpp, src/IO/FileStream.hpp, src/IO/ZipReader.cpp, src/IO/ZipWriter.cpp, src/INIParser.cpp, src/Network/HTTPSClient.cpp
opened: 2026-10-18
tags: [io, robustness, defect]
---

# The writers open a FIFO or a device path without checking it, and block

## Why
Opening a FIFO for writing without a reader blocks `std::ofstream::open()` forever, as the read side did (fixed on
2026-10-18: every reader goes through `IO::openInputFile()`, base `caution-points.md` § the readers refuse what is not
a regular file). The owner's decision covered the readers; the writers have the same defect. A write target is usually
chosen by the application (the captures directory), less often by data: the risk is lower, not nil.

## What remains
- An `IO::openOutputFile()` beside `openInputFile()`: refuse an EXISTING path that is not a regular file (a missing
  path is created as now), then open.
- Route: `FileStream` (write mode), `IO::createFile()`, `IO::filePutContents()` (both overloads),
  `ZipReader::extract()`, `INIParser::write()`, `HTTPSClient`'s download file; `ZipWriter::openArchive()` (libzip
  opens the path itself: the check alone).
- Tests: the FIFO harness of `test_IO.cpp` (`FifoForOneTest`) works for a writer too (open the READ end
  non-blocking to release a blocked writer).

## References
- `src/Testing/test_IO.cpp` `IOFileUtils.readersRefuseAFifoWithoutBlocking`.
