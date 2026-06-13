/*
 * emeraude-base — shared precompiled header (project-wide STL hot-set).
 *
 * PURPOSE
 *   Pre-parse the heavy, ubiquitous standard-library headers once per target instead of in
 *   every translation unit. emeraude-base owns this header as the single source of truth:
 *   its own compiled modules use it, and downstream consumers (emeraude-engine, and any
 *   other project) attach it to their targets through emeraude_base_target_enable_pch().
 *
 * STRICT RULE — what may live here:
 *   ONLY the C++ standard library. Stable, rarely-changing, included almost everywhere.
 *
 *   NEVER add a project header — neither emeraude-base's own headers (Math/*, IO/*, …) nor
 *   a consumer's headers (engine, app_kernel). Editing a header listed here invalidates the
 *   precompiled header and forces a FULL rebuild of every translation unit of every target
 *   that uses it — the exact opposite of the intended speed-up. Project code changes daily;
 *   the STL does not.
 *
 *   Likewise, NEVER add a third-party header here: it would tie this shared, STL-only PCH to
 *   one consumer's dependency (e.g. Eigen for app_kernel, Vulkan/GLFW for the engine). Such
 *   consumer-specific headers belong in that consumer's own PCH, layered on top of this one.
 *
 * MAINTENANCE
 *   The list below is the audited union of every STL/C-standard header actually included across
 *   the whole cascade (projet-alpha + emeraude-engine + emeraude-base), MINUS rarely-used heavy
 *   headers (<regex>, <format>) that would only bloat the per-target PCH binary for a handful of
 *   call sites. Regenerate the audit with collect_stl_headers.py at the projet-alpha root.
 *
 * SCOPE
 *   Wired by cmake/EnablePrecompiledHeaders.cmake (emeraude_base_target_enable_pch), applied
 *   PRIVATE and restricted to the CXX language ($<COMPILE_LANGUAGE:CXX>) so it never lands on
 *   the C / ASM translation units a target may also compile, and never propagates to
 *   consumers. Toggle the whole feature with -DEMERAUDE_ENABLE_PCH=ON/OFF.
 */

#pragma once

/* MSVC only exposes M_PI, M_E, … from <cmath> when _USE_MATH_DEFINES is defined BEFORE it is
 * included. This PCH is force-included (/FI) ahead of every translation unit, so it parses
 * <cmath> before any TU can set the macro itself — a TU's own "#define _USE_MATH_DEFINES" would
 * arrive too late (the include guard already swallowed <cmath>). Define it here, once, for the
 * whole project. Ignored / harmless on POSIX. Without this, TUs relying on M_PI (e.g. app_kernel's
 * InfillGyroid) fail with "M_PI: undeclared identifier". */
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

/* C base headers. */
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

/* Containers. */
#include <array>
#include <bitset>
#include <deque>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <span>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/* Memory, utility, generic programming. */
#include <any>
#include <compare>
#include <functional>
#include <initializer_list>
#include <memory>
#include <new>
#include <optional>
#include <source_location>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>

/* Algorithms & numerics. */
#include <algorithm>
#include <bit>
#include <concepts>
#include <iterator>
#include <limits>
#include <numbers>
#include <numeric>
#include <random>
#include <ranges>
#include <ratio>

/* Diagnostics. */
#include <stdexcept>
#include <system_error>

/* Streams & string building. */
#include <charconv>
#include <fstream>
#include <iomanip>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>

/* Threading & time. */
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <thread>

/* Filesystem. */
#include <filesystem>
