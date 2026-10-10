---
id: external-dependencies-hwloc-3-archive
title: Move EXTERNAL_DEPENDENCIES_VERSION to the first archive carrying hwloc 3.0
status: blocked
priority: high
scope: cmake/InstallDependencies.cmake
opened: 2026-10-10
blocked-by: ext-deps-generator docs/todo/hwloc-3-rollout-and-release.md
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

## To do

- Once the generator publishes the release with hwloc 3.0 on every configuration, set
  `EXTERNAL_DEPENDENCIES_VERSION` to it (`cmake/InstallDependencies.cmake`), in the same push as the engine change.
- Build the cascade and run `EmeraudeBaseUnitTests` on the three OS against the downloaded archive.