---
id: external-dependencies-hwloc-3-archive
title: Move EXTERNAL_DEPENDENCIES_VERSION to the first archive carrying hwloc 3.0
status: in-progress
priority: high
scope: cmake/InstallDependencies.cmake
opened: 2026-10-10
tags: [hwloc, external-dependencies, release]
---

# Move EXTERNAL_DEPENDENCIES_VERSION to the first archive carrying hwloc 3.0

## Why

hwloc <= 2.15 hangs forever in `hwloc_topology_load()` on Apple silicon with three core types (story: the engine's
`docs/subsystems/platformspecific/13-cpu-hybrid-core-detection.md`). The ext-deps-generator now pins hwloc on
upstream master (3.0.0a1), and the engine's `SystemInfo.cpp` is adapted to the 3.0 API
(`hwloc_cpukinds_get_info()` takes a `struct hwloc_infos_s **`). That engine code **does not compile against v017**,
which still ships hwloc 2.14.

Only a host whose `dependencies/<config>` is a symlink to a local ext-deps-generator output already rebuilt with hwloc
3.0 builds today (macOS arm64, 2026-10-10). Every host that downloads v017 breaks on `SystemInfo.cpp`.

## Done

- `EXTERNAL_DEPENDENCIES_VERSION` is `v018` (2026-10-10), the first archive carrying hwloc 3.0 on every configuration,
  pushed with the engine change. v018 also moves tinyusdz to LightUSD v1.0.0-rc4 (upstream rebrand, no alias):
  `cmake/SetupTinyUSDZ.cmake` now finds package `lightusd` and links `lightusd::lightusd_static`, and the engine's
  USDLoader uses namespace `lightusd`. Verified on Linux (Debian 13, local symlinked archive): projet-alpha builds
  `-Werror`, starts (hwloc 3.0, no hang), and WorldLobby composes 2806 prims / 942 meshes / 155 materials / 348
  textures.

## To do

- ⚠️ **v018 has no `glibc2.35` Linux archive yet** — only `glibc2.41`. `InstallDependencies` tries the host's exact
  glibc tag, then the `glibc2.35` floor, so until the two 2.35 zips are added to v018 (owner decision 2026-10-10:
  built on the Ubuntu 22.04 machine, uploaded to the existing release) every Linux host whose glibc is not exactly
  2.41 fails to download. Nothing to change here once they are up.
- Build the cascade and run `EmeraudeBaseUnitTests` on the three OS against the downloaded archive.