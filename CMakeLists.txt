#######################################################################
## CMake configuration
cmake_minimum_required(VERSION 3.25.1)
cmake_policy(SET CMP0042 NEW)
cmake_policy(SET CMP0091 NEW)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

#######################################################################
## Declare project
# Foundation library for Emeraude-Engine: base utilities + platform/config headers,
# plus resolution of the ext-deps-generator output. Consumable whole (emeraude::base)
# or per-module (emeraude::base::math, …) by tools that don't need the full engine.
project(EmeraudeBase
	VERSION 1.1.5
	DESCRIPTION "Foundation library for Emeraude-Engine."
	HOMEPAGE_URL "https://github.com/EmeraudeEngine/emeraude-base"
	LANGUAGES C CXX
)

set_property(GLOBAL PROPERTY USE_FOLDERS On)

#######################################################################
## Options
set(EMERAUDE_BASE_LIBRARY_TYPE "STATIC" CACHE STRING "Umbrella library type (STATIC or SHARED).")
set_property(CACHE EMERAUDE_BASE_LIBRARY_TYPE PROPERTY STRINGS STATIC SHARED)

# Language standards — emeraude-base is an incentive toward modern C++: strict and
# forward-looking by default, but a consumer can raise them. Lowering below the floor
# is refused (C++20 / C17 are the minimums the codebase relies on).
set(EMERAUDE_CXX_VERSION "20" CACHE STRING "C++ standard to compile with (minimum 20).")
set(EMERAUDE_C_VERSION   "17" CACHE STRING "C standard to compile with (minimum 17).")

if ( EMERAUDE_CXX_VERSION LESS 20 )
	message(FATAL_ERROR "[EmeraudeBase] EMERAUDE_CXX_VERSION must be >= 20 (got ${EMERAUDE_CXX_VERSION}).")
endif ()
if ( EMERAUDE_C_VERSION LESS 17 )
	message(FATAL_ERROR "[EmeraudeBase] EMERAUDE_C_VERSION must be >= 17 (got ${EMERAUDE_C_VERSION}).")
endif ()

# Idealist by default, relaxable: warnings-as-errors ON unless explicitly disabled.
option(EMERAUDE_DISABLE_EXCEPTIONS "Disable C++ exceptions." On)
option(EMERAUDE_DISABLE_RTTI "Disable RTTI." Off)
option(EMERAUDE_DISABLE_PARANOID_COMPILATION "Disable warnings-as-errors." Off)
option(EMERAUDE_ENABLE_FP_OPTIMIZATION "Enable fast (non-strict) floating-point." Off)
option(EMERAUDE_ENABLE_PCH "Enable the precompiled headers." Off)
option(EMERAUDE_ENABLE_TESTS "Build the GoogleTest suite(s)." Off)
option(EMERAUDE_ENABLE_SANITIZERS "Build with AddressSanitizer + UndefinedBehaviorSanitizer (Ave robustus! A.1)." Off)
option(EMERAUDE_ENABLE_BENCHMARKS "Build the Google Benchmark suite(s) (Ave robustus! A.5)." Off)
# Windows MSVC runtime selection (/MT vs /MD). Drives the runtime, the ext-deps dirname
# (windows.x86_64-*-MT/MD) and gtest's CRT. When consumed by emeraude-engine the value is
# inherited (cache); declared here so a standalone Windows build of base works too.
option(EMERAUDE_USE_STATIC_RUNTIME "Build with the static MSVC runtime (Windows only)." Off)
option(EMERAUDE_ENABLE_OPENMP "Enable OpenMP (used by VertexFactory)." On)

# Consumed by emeraude_base_config.hpp.in
set(EMERAUDE_INTERNET_CHECK_DOMAIN "example.com" CACHE STRING "Domain used to check internet connectivity.")
option(EMERAUDE_DEBUG_OBSERVER_PATTERN "Debug output for the observer pattern." Off)
option(EMERAUDE_DEBUG_PIXEL_FACTORY "Debug output for PixelFactory." Off)
option(EMERAUDE_DEBUG_VERTEX_FACTORY "Debug output for VertexFactory." Off)
option(EMERAUDE_DEBUG_WAVE_FACTORY "Debug output for WaveFactory." Off)
option(EMERAUDE_DEBUG_THREAD_POOL "Debug output for ThreadPool." Off)
set(EMERAUDE_DEBUG_THREAD_POOL_N "1" CACHE STRING "Forced thread count when debugging ThreadPool.")
option(EMERAUDE_DISABLE_PARALLEL_COLLISION "Disable parallel collision detection." Off)

#######################################################################
## Build type
set(CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo MinSizeRel)

if ( MSVC )
	if ( MSVC_BUILD_TYPE )
		set(CMAKE_BUILD_TYPE ${MSVC_BUILD_TYPE})
	elseif ( NOT CMAKE_BUILD_TYPE )
		message(FATAL_ERROR "[EmeraudeBase] Set -DMSVC_BUILD_TYPE=Debug|Release|RelWithDebInfo|MinSizeRel on Windows.")
	endif ()
elseif ( NOT CMAKE_BUILD_TYPE )
	set(CMAKE_BUILD_TYPE Release)
endif ()

message("[EmeraudeBase] Build type : ${CMAKE_BUILD_TYPE}")

if ( PROJECT_IS_TOP_LEVEL )
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
	set(CMAKE_EXPORT_COMPILE_COMMANDS On)
endif ()

#######################################################################
## External dependencies (ext-deps-generator output)
# Single source of truth: consumers read EMERAUDE_EXT_LIBS_PATH (+ _INCLUDE_DIR /
# _LIB_DIR) after add_subdirectory(); they never recompute the path themselves.
if ( NOT DEFINED APP_ARCH )
	set(APP_ARCH "x86_64")
endif ()

# ext-deps-generator only ships two configurations (Release / Debug).
# Map CMake's optimized build types to "Release" so RelWithDebInfo and
# MinSizeRel reuse the Release binaries. Must stay in sync with the same
# mapping in cmake/InstallDependencies.cmake.
if ( CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo|MinSizeRel" )
	set(EMERAUDE_EXT_LIBS_BUILD_TYPE Release)
else ()
	set(EMERAUDE_EXT_LIBS_BUILD_TYPE Debug)
endif ()

if ( UNIX AND NOT APPLE )
	set(EMERAUDE_EXT_LIBS_DIRNAME "linux.${APP_ARCH}-${EMERAUDE_EXT_LIBS_BUILD_TYPE}")
elseif ( APPLE )
	if ( CMAKE_OSX_ARCHITECTURES )
		set(APP_ARCH ${CMAKE_OSX_ARCHITECTURES})
	endif ()
	set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
	set(EMERAUDE_EXT_LIBS_DIRNAME "mac.${APP_ARCH}-${EMERAUDE_EXT_LIBS_BUILD_TYPE}")
elseif ( MSVC )
	if ( EMERAUDE_USE_STATIC_RUNTIME )
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
		set(EMERAUDE_EXT_LIBS_DIRNAME "windows.${APP_ARCH}-${EMERAUDE_EXT_LIBS_BUILD_TYPE}-MT")
	else ()
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
		set(EMERAUDE_EXT_LIBS_DIRNAME "windows.${APP_ARCH}-${EMERAUDE_EXT_LIBS_BUILD_TYPE}-MD")
	endif ()
else ()
	message(FATAL_ERROR "[EmeraudeBase] Unsupported platform.")
endif ()

set(EMERAUDE_EXT_LIBS_PATH        "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/${EMERAUDE_EXT_LIBS_DIRNAME}" CACHE PATH "ext-deps-generator output root." FORCE)
set(EMERAUDE_EXT_LIBS_INCLUDE_DIR "${EMERAUDE_EXT_LIBS_PATH}/include" CACHE PATH "External dependencies include dir." FORCE)
set(EMERAUDE_EXT_LIBS_LIB_DIR     "${EMERAUDE_EXT_LIBS_PATH}/lib"     CACHE PATH "External dependencies library dir." FORCE)

# emeraude-base owns ALL the Setup*.cmake for ext-deps-generator libraries (even those it
# does not use itself, e.g. ufbx/fastgltf/taglib). Consumers add this dir to their
# CMAKE_MODULE_PATH and include(Setup<Lib>) from here — single source of truth, reusable
# by the engine and any other project.
set(EMERAUDE_BASE_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake" CACHE PATH "emeraude-base CMake helpers (Setup*.cmake for external deps)." FORCE)

# emeraude-base owns the project-wide precompiled header: a STL-only hot-set
# (STLPrecompiledHeaders.hpp) + the EnablePrecompiledHeaders helper. It is applied across the whole
# cascade — base's own compiled modules + umbrella, the engine SHARED library, and the leaf
# executables (projet-alpha, app_kernel's Kernel, app_system's binaries) — by calling
# emeraude_base_target_enable_pch(<target>) right after each target is declared. STL only — never
# project headers — so editing project code never invalidates it.
#
# WINDOWS CAVEAT (CONFIRMED): the engine is a SHARED library; with WINDOWS_EXPORT_ALL_SYMBOLS its
# auto-generated export .def scans every input .obj. With the PCH enabled, the PCH object's marker
# symbols (__@@_PchSym_@00@…) leak into exports.def and the link fails (LNK2001 on a bogus '__').
# This reproduces on MSVC. The fix is the explicit export API (EMERAUDE_API, dropping
# WINDOWS_EXPORT_ALL_SYMBOLS), gated by the engine's EMERAUDE_USE_EXPLICIT_EXPORTS option — NOT
# reverting the PCH. Migration guide: emeraude-engine/docs/windows-export-api.md.
set(EMERAUDE_BASE_PCH_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/STLPrecompiledHeaders.hpp" CACHE FILEPATH "emeraude-base shared precompiled header (STL hot-set)." FORCE)
include(EnablePrecompiledHeaders)

# Download + extract the prebuilt archive when EMERAUDE_EXT_LIBS_PATH is absent.
# Skipped automatically when it is a symlink (developer pointing at a local build).
include(InstallDependencies)

if ( NOT EXISTS "${EMERAUDE_EXT_LIBS_PATH}" )
	message(FATAL_ERROR "[EmeraudeBase] Missing external dependencies for this config: ${EMERAUDE_EXT_LIBS_PATH}")
endif ()

#######################################################################
## Compile configuration
# emeraude-base owns the project-wide compile definitions/options, published as cache
# variables. Consumers (emeraude-engine, …) apply EMERAUDE_COMPILE_DEFINITIONS /
# EMERAUDE_COMPILE_OPTIONS to their own targets; to diverge, override them in CMake.

if ( MSVC )
	set(EMERAUDE_COMPILE_DEFINITIONS
		$<$<CONFIG:Debug>:DEBUG>											# Declares DEBUG macro
		$<$<CONFIG:Release>:NDEBUG>											# Declares NDEBUG macro
		_WIN32_WINNT=0x0A00													# Target Windows 10/11 (0x0601=Win7, 0x0602=Win8, 0x0603=Win8.1)
		NOMINMAX															# Prevent <windows.h> from defining min/max macros (breaks Math/Vector.h and every std::min/std::max)
		WIN32_LEAN_AND_MEAN													# Trim rarely-used headers pulled by <windows.h> (faster compilation, fewer macro conflicts)
		CACHE STRING "Project-wide compile definitions (Windows)." FORCE
	)
	set(EMERAUDE_COMPILE_OPTIONS
		/utf-8																# Treat source files as UTF-8 and emit UTF-8 narrow execution strings (required for u8"..." literals containing non-ASCII to round-trip correctly; without this MSVC interprets sources via the system codepage)
		$<IF:$<BOOL:${EMERAUDE_DISABLE_RTTI}>,/GR-,/GR>						# Run-time type information
		$<$<BOOL:${EMERAUDE_DISABLE_EXCEPTIONS}>:/EHs->						# Enable exceptions
		$<$<BOOL:${EMERAUDE_DISABLE_EXCEPTIONS}>:/EHc->						# Enable exceptions
		$<$<NOT:$<BOOL:${EMERAUDE_DISABLE_EXCEPTIONS}>>:/EHsc>				# Enable exceptions
		$<$<CONFIG:Debug>:/RTC1>											# Enable basic run-time checks
		$<$<CONFIG:Debug>:/Od>												# Disable optimizations
		$<$<CONFIG:Debug>:/Zi>												# Generate debug information (PDB)
		$<$<CONFIG:Release>:/O2>											# Optimize for maximum speed
		$<$<CONFIG:Release>:$<IF:$<BOOL:${EMERAUDE_ENABLE_FP_OPTIMIZATION}>,/fp:fast,/fp:precise>> # Enable fast floating-point optimizations
		$<$<CONFIG:Release>:/Ob3>											# Inline any suitable function
		$<$<CONFIG:Release>:/GF>											# Enable string pooling
		/MP 																# Multi-processor compilation
		/Gy 																# Enable function-level linking
		/W4																	# Warning level 4 (high)
		$<$<NOT:$<BOOL:${EMERAUDE_DISABLE_PARANOID_COMPILATION}>>:/WX>		# Treat warnings as errors
		/wd4100 															# Ignore "unreferenced formal parameter"
		/wd4127 															# Ignore "conditional expression is constant"
		/wd4324 															# Ignore "structure was padded due to alignment specifier"
		/wd4481 															# Ignore "nonstandard extension used: override"
		/wd4512																# Ignore "assignment operator could not be generated"
		$<$<BOOL:${EMERAUDE_DISABLE_EXCEPTIONS}>:/wd4530>					# Ignore "C++ exception handler used, but unwind semantics are not enabled" when exceptions are disabled
		/wd4702 															# Ignore "unreachable code"
		/wd4996																# Ignore "function or variable may be unsafe"
		/bigobj																# Remove the COFF object format limitation
		CACHE STRING "Project-wide compile options (Windows)." FORCE
	)
elseif ( APPLE )
	set(EMERAUDE_COMPILE_DEFINITIONS
		$<$<CONFIG:Debug>:DEBUG>													# Declares DEBUG macro
		$<$<CONFIG:Release>:NDEBUG>													# Declares NDEBUG macro
		CACHE STRING "Project-wide compile definitions (macOS)." FORCE
	)
	set(EMERAUDE_COMPILE_OPTIONS
		$<IF:$<BOOL:${EMERAUDE_DISABLE_RTTI}>,-fno-rtti,-frtti>						# Run-time type information
		$<IF:$<BOOL:${EMERAUDE_DISABLE_EXCEPTIONS}>,-fno-exceptions,-fexceptions>	# C++ exception handling
		$<$<CONFIG:Debug>:-O0>														# No optimization
		$<$<CONFIG:Debug>:-g>														# Generate debug information
		$<$<CONFIG:Debug>:-Wall>													# Enable all common warnings
		$<$<CONFIG:Debug>:-Wextra>													# Enable extra warnings
		$<$<CONFIG:Debug>:-Wpedantic>												# Enforce strict ISO C++ compliance
		$<$<CONFIG:Debug>:-fno-omit-frame-pointer>									# Keep frame pointer for better stack traces
		$<$<CONFIG:Release>:-O2>													# Optimize for speed
		$<$<CONFIG:Release>:$<IF:$<BOOL:${EMERAUDE_ENABLE_FP_OPTIMIZATION}>,-ffast-math,-fno-fast-math>> # Enable fast floating-point optimizations # FIXME: This flag makes unit tests fail.
		$<$<CONFIG:Release>:-fdata-sections>										# Place each data item in its own section
		$<$<CONFIG:Release>:-ffunction-sections>									# Place each function in its own section
		$<$<CONFIG:Release>:-fstack-protector-strong>								# Stack buffer overflow protection
		$<$<NOT:$<BOOL:${EMERAUDE_DISABLE_PARANOID_COMPILATION}>>:-Werror>			# Treat warnings as errors
		-Wformat 																	# Check format string arguments
		-Wformat-security															# Warn about format string vulnerabilities
		-Wno-unknown-pragmas														# Ignore unknown pragma directives
		-Wno-unused-parameter														# Ignore unused function parameters
		-Wno-unused-private-field													# Ignore unused private fields
		-Wno-nullability-extension													# Ignore nullability extension warnings
		-Wno-nullability-completeness												# Ignore incomplete nullability annotations
		-Wno-gnu-zero-variadic-macro-arguments										# Ignore GNU variadic macro extension
		$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-fsanitize=address>
		$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-fsanitize=undefined>
		$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-fno-sanitize-recover=all>
		$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-fno-omit-frame-pointer>
		$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-fstack-protector-strong>
		$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-O1>
		CACHE STRING "Project-wide compile options (macOS)." FORCE
	)
elseif ( UNIX )
	set(EMERAUDE_COMPILE_DEFINITIONS
		$<$<CONFIG:Debug>:DEBUG>													# Declares DEBUG macro
		$<$<CONFIG:Release>:NDEBUG>													# Declares NDEBUG macro
		$<$<CONFIG:Release>:_FORTIFY_SOURCE=2>										# Runtime buffer overflow detection
		CACHE STRING "Project-wide compile definitions (Linux)." FORCE
	)
	set(EMERAUDE_COMPILE_OPTIONS
		$<IF:$<BOOL:${EMERAUDE_DISABLE_RTTI}>,-fno-rtti,-frtti> 					# Run-time type information
		$<IF:$<BOOL:${EMERAUDE_DISABLE_EXCEPTIONS}>,-fno-exceptions,-fexceptions> 	# C++ exception handling
		$<$<CONFIG:Debug>:-O0> 														# No optimization
		$<$<CONFIG:Debug>:-g> 														# Generate debug information
		$<$<CONFIG:Debug>:-Wall> 													# Enable all common warnings
		$<$<CONFIG:Debug>:-Wextra> 													# Enable extra warnings
		$<$<CONFIG:Debug>:-Wpedantic> 												# Enforce strict ISO C++ compliance
		$<$<CONFIG:Debug>:-fno-omit-frame-pointer> 									# Keep frame pointer for better stack traces
		$<$<CONFIG:Release>:-O2> 													# Optimize for speed
		$<$<CONFIG:Release>:$<IF:$<BOOL:${EMERAUDE_ENABLE_FP_OPTIMIZATION}>,-ffast-math,-fno-fast-math>> # Enable fast floating-point optimizations # FIXME: This flag makes unit tests fail.
		$<$<CONFIG:Release>:-fdata-sections> 										# Place each data item in its own section
		$<$<CONFIG:Release>:-ffunction-sections> 									# Place each function in its own section
		$<$<CONFIG:Release>:-fstack-protector-strong> 								# Stack buffer overflow protection
		$<$<NOT:$<BOOL:${EMERAUDE_DISABLE_PARANOID_COMPILATION}>>:-Werror> 			# Treat warnings as errors
		-Wformat 																	# Check format string arguments
		-Wformat-security 															# Warn about format string vulnerabilities
		-Wno-unknown-pragmas 														# Ignore unknown pragma directives
		-Wno-unused-parameter 														# Ignore unused function parameters
		$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-fsanitize=address>
		$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-fsanitize=undefined>
		$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-fno-sanitize-recover=all>
		$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-fno-omit-frame-pointer>
		$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-fstack-protector-strong>
		$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-O1>
		CACHE STRING "Project-wide compile options (Linux)." FORCE
	)
endif ()

#######################################################################
## Modules
# Per-module targets ("Ave robustus!" gap #1): every src/ subsystem is its own CMake target,
# aliased emeraude::base::<name> — header-only modules are INTERFACE libraries, compiled modules
# are OBJECT libraries aggregated into the emeraude::base umbrella. Consumers link the whole
# umbrella OR an individual module. See docs/module-map.md.
# platform (header-only, also usable standalone)
add_library(emeraude_base_platform INTERFACE)
target_include_directories(emeraude_base_platform INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
	$<INSTALL_INTERFACE:include>
)
add_library(emeraude::base::platform ALIAS emeraude_base_platform)

# Shared compile requirements for every base module AND the umbrella: how to compile a base
# translation unit — own headers, the generated config header, the ext-deps system includes,
# the C++ standard, the compile definitions/options, the ext lib link directory and the
# sanitizer link options. Per-module OBJECT libraries link this to compile; the umbrella links
# it PUBLIC so consumers inherit the include/definition/option usage requirements.
add_library(emeraude_base_flags INTERFACE)
target_include_directories(emeraude_base_flags INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
	$<INSTALL_INTERFACE:include>
)
target_include_directories(emeraude_base_flags SYSTEM INTERFACE
	${EMERAUDE_EXT_LIBS_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/dependencies/asio/include
	${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tinysoundfont
)
target_link_directories(emeraude_base_flags INTERFACE ${EMERAUDE_EXT_LIBS_LIB_DIR})
target_compile_features(emeraude_base_flags INTERFACE cxx_std_${EMERAUDE_CXX_VERSION})
target_compile_definitions(emeraude_base_flags INTERFACE ${EMERAUDE_COMPILE_DEFINITIONS})
target_compile_options(emeraude_base_flags INTERFACE ${EMERAUDE_COMPILE_OPTIONS})
target_link_options(emeraude_base_flags INTERFACE
	$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-fsanitize=address>
	$<$<BOOL:${EMERAUDE_ENABLE_SANITIZERS}>:-fsanitize=undefined>
)
add_library(emeraude::base::flags ALIAS emeraude_base_flags)

if ( EMERAUDE_ENABLE_SANITIZERS )
	message(STATUS "[EmeraudeBase] Sanitizers ENABLED: ASan + UBSan (-O1, halt-on-error). Incompatible with _FORTIFY_SOURCE (omitted).")
endif ()

# Per-module sources (no globbing); see cmake/PrepareBaseSourceFiles.cmake
include(PrepareBaseSourceFiles)

# A compiled module: an OBJECT library built with the shared flags, PIC (so it can be aggregated
# into a SHARED umbrella), aliased emeraude::base::<name>. Inter-module symbol references resolve
# when the objects are aggregated into the umbrella below; linking a single module OBJECT target
# standalone may also require its sibling modules.
function(emeraude_base_object_module name)
	add_library(emeraude_base_${name} OBJECT ${ARGN})
	target_link_libraries(emeraude_base_${name} PUBLIC emeraude_base_flags)
	set_target_properties(emeraude_base_${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
	emeraude_base_target_enable_pch(emeraude_base_${name})
	add_library(emeraude::base::${name} ALIAS emeraude_base_${name})
endfunction()

emeraude_base_object_module(core ${EMERAUDE_BASE_CORE_SOURCES})
emeraude_base_object_module(hash ${EMERAUDE_BASE_HASH_SOURCES})
emeraude_base_object_module(gametools ${EMERAUDE_BASE_GAMETOOLS_SOURCES})
emeraude_base_object_module(time ${EMERAUDE_BASE_TIME_SOURCES})
emeraude_base_object_module(debug ${EMERAUDE_BASE_DEBUG_SOURCES})
emeraude_base_object_module(compression ${EMERAUDE_BASE_COMPRESSION_SOURCES})
emeraude_base_object_module(io ${EMERAUDE_BASE_IO_SOURCES})
emeraude_base_object_module(network ${EMERAUDE_BASE_NETWORK_SOURCES})
emeraude_base_object_module(vertex ${EMERAUDE_BASE_VERTEX_SOURCES})
emeraude_base_object_module(wave ${EMERAUDE_BASE_WAVE_SOURCES})

# A header-only module: an INTERFACE library inheriting the shared include paths + standard.
# (pixel's PNG/JPEG/Freetype headers come from the same ext-deps system include carried by flags.)
function(emeraude_base_interface_module name)
	add_library(emeraude_base_${name} INTERFACE)
	target_link_libraries(emeraude_base_${name} INTERFACE emeraude_base_flags)
	add_library(emeraude::base::${name} ALIAS emeraude_base_${name})
endfunction()

emeraude_base_interface_module(math)
emeraude_base_interface_module(algorithms)
emeraude_base_interface_module(animation)
emeraude_base_interface_module(pixel)

# umbrella — aggregates every module's compiled objects (consumers may also link any
# emeraude::base::<module> directly). flags + platform are PUBLIC so consumers inherit the
# include/definition/option usage requirements; the ext libraries are linked below (Setup scripts).
add_library(emeraude_base ${EMERAUDE_BASE_LIBRARY_TYPE} ${EMERAUDE_BASE_SOURCES}
	$<TARGET_OBJECTS:emeraude_base_core>
	$<TARGET_OBJECTS:emeraude_base_hash>
	$<TARGET_OBJECTS:emeraude_base_gametools>
	$<TARGET_OBJECTS:emeraude_base_time>
	$<TARGET_OBJECTS:emeraude_base_debug>
	$<TARGET_OBJECTS:emeraude_base_compression>
	$<TARGET_OBJECTS:emeraude_base_io>
	$<TARGET_OBJECTS:emeraude_base_network>
	$<TARGET_OBJECTS:emeraude_base_vertex>
	$<TARGET_OBJECTS:emeraude_base_wave>
)
add_library(emeraude::base ALIAS emeraude_base)

set_target_properties(emeraude_base PROPERTIES
	OUTPUT_NAME EmeraudeBase
	C_STANDARD ${EMERAUDE_C_VERSION} C_STANDARD_REQUIRED On C_EXTENSIONS Off
	CXX_STANDARD ${EMERAUDE_CXX_VERSION} CXX_STANDARD_REQUIRED On CXX_EXTENSIONS Off
	POSITION_INDEPENDENT_CODE ON
	VERSION ${PROJECT_VERSION}
)

# Umbrella's own loose sources (${EMERAUDE_BASE_SOURCES}) get the PCH too.
emeraude_base_target_enable_pch(emeraude_base)

target_link_libraries(emeraude_base PUBLIC emeraude_base_flags emeraude::base::platform)

# External dependencies — reuse the engine Setup scripts, retargeted to emeraude_base.
set(TARGET_BINARY_FOR_SETUP emeraude_base)

include(SetupJsonCpp)
include(SetupFreetype)
include(SetupZIP)
include(SetupPNG)
include(SetupJPEG)
include(SetupZLIB)
include(SetupBZIP2)
include(SetupLZMA)
include(SetupZSTD)
include(SetupBrotli)
include(SetupHarfbuzz)
include(SetupSndFile)
include(SetupSampleRate)
include(SetupASIO)
include(SetupThreads)
if ( EMERAUDE_ENABLE_OPENMP AND NOT APPLE )
	include(SetupOpenMP)
endif ()

unset(TARGET_BINARY_FOR_SETUP)

# The Setup*.cmake scripts attach each dependency's compile definitions (LZMA_API_STATIC,
# ASIO_STANDALONE, JSON_USE_EXCEPTION, …) to the umbrella as PUBLIC usage requirements. But every
# subsystem is compiled in its own OBJECT library that inherits compile flags from
# emeraude_base_flags only (see emeraude_base_object_module above) — the umbrella's PUBLIC
# definitions reach consumers that LINK it, never the objects aggregated via $<TARGET_OBJECTS:>.
# Mirror those definitions onto the shared flags interface so each module is compiled with them.
# LZMA_API_STATIC in particular MUST be present when compiling against liblzma's headers: without
# it lzma.h declares the API __declspec(dllimport), producing __imp_lzma_* symbols that fail to
# link against the static lzma.lib.
get_target_property(_EMERAUDE_BASE_DEP_DEFS emeraude_base INTERFACE_COMPILE_DEFINITIONS)
if ( _EMERAUDE_BASE_DEP_DEFS )
	target_compile_definitions(emeraude_base_flags INTERFACE ${_EMERAUDE_BASE_DEP_DEFS})
endif ()
unset(_EMERAUDE_BASE_DEP_DEFS)

# emeraude_base_config.hpp — generated after SetupSndFile so LIBSNDFILE_ENABLED is resolved.
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/src/emeraude_base_config.hpp.in
	${CMAKE_CURRENT_BINARY_DIR}/include/emeraude_base_config.hpp
)

message(STATUS "[EmeraudeBase] ${CMAKE_BUILD_TYPE} | ${EMERAUDE_BASE_LIBRARY_TYPE} | ext-libs: ${EMERAUDE_EXT_LIBS_PATH}")

#######################################################################
## Tests
# base owns the GoogleTest infrastructure and its own suite (EmeraudeBaseUnitTests).
# Consumers reuse the fetched gtest for their own suites; `ctest` aggregates them all.
if ( EMERAUDE_ENABLE_TESTS )
	message(STATUS "[EmeraudeBase] GoogleTest suite enabled (EmeraudeBaseUnitTests).")

	include(FetchContent)
	include(GoogleTest)

	FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.17.0)

	if ( NOT EMERAUDE_USE_STATIC_RUNTIME )
		set(gtest_force_shared_crt On CACHE BOOL "" FORCE)
	endif ()

	FetchContent_MakeAvailable(googletest)

	add_executable(EmeraudeBaseUnitTests ${EMERAUDE_BASE_TEST_SOURCES})
	emeraude_base_target_enable_pch(EmeraudeBaseUnitTests)

	set_target_properties(EmeraudeBaseUnitTests PROPERTIES
		C_STANDARD ${EMERAUDE_C_VERSION} C_STANDARD_REQUIRED On C_EXTENSIONS Off
		CXX_STANDARD ${EMERAUDE_CXX_VERSION} CXX_STANDARD_REQUIRED On CXX_EXTENSIONS Off
	)

	target_compile_definitions(EmeraudeBaseUnitTests PRIVATE ${EMERAUDE_COMPILE_DEFINITIONS})
	target_compile_options(EmeraudeBaseUnitTests PRIVATE ${EMERAUDE_COMPILE_OPTIONS})
	target_link_libraries(EmeraudeBaseUnitTests PRIVATE emeraude::base gtest_main)

	# Heavy PixelFactory tests can run against a much larger source image (8160x6144
	# instead of the default 1200x800). Off by default to keep the suite fast; the
	# switch (path + expected dimensions) lives in src/Testing/Constants.hpp.
	option(EMERAUDE_TESTS_USE_EXTRA_LARGE_RGB "Use the 8160x6144 image for heavy PixelFactory tests (default 1200x800)." Off)

	if ( EMERAUDE_TESTS_USE_EXTRA_LARGE_RGB )
		target_compile_definitions(EmeraudeBaseUnitTests PRIVATE EMERAUDE_TESTS_USE_EXTRA_LARGE_RGB)
	endif ()

	enable_testing()
	# Tests load fixtures via "./assets/…" — run them from resources/ where those live.
	gtest_discover_tests(EmeraudeBaseUnitTests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources)
endif ()

#######################################################################
## Benchmarks
# "Ave robustus!" A.5 — performance is benchmark-gated: no optimization lands without a
# before/after measurement proving the gain. Google Benchmark is a test-only dependency
# (like gtest), built into a separate EmeraudeBaseBenchmarks target. OFF by default and
# NEVER part of the ctest correctness pass (benchmarks are slow and run on demand).
# Build it in Release for trustworthy numbers (a Debug build of the benchmark lib warns).
if ( EMERAUDE_ENABLE_BENCHMARKS )
	message(STATUS "[EmeraudeBase] Google Benchmark suite enabled (EmeraudeBaseBenchmarks).")

	include(FetchContent)

	# Pull only the library — skip its own tests, install rules and gtest dependency.
	set(BENCHMARK_ENABLE_TESTING Off CACHE BOOL "" FORCE)
	set(BENCHMARK_ENABLE_GTEST_TESTS Off CACHE BOOL "" FORCE)
	set(BENCHMARK_ENABLE_INSTALL Off CACHE BOOL "" FORCE)
	set(BENCHMARK_INSTALL_DOCS Off CACHE BOOL "" FORCE)

	FetchContent_Declare(googlebenchmark GIT_REPOSITORY https://github.com/google/benchmark.git GIT_TAG v1.9.1)
	FetchContent_MakeAvailable(googlebenchmark)

	add_executable(EmeraudeBaseBenchmarks ${EMERAUDE_BASE_BENCHMARK_SOURCES})
	emeraude_base_target_enable_pch(EmeraudeBaseBenchmarks)

	set_target_properties(EmeraudeBaseBenchmarks PROPERTIES
		C_STANDARD ${EMERAUDE_C_VERSION} C_STANDARD_REQUIRED On C_EXTENSIONS Off
		CXX_STANDARD ${EMERAUDE_CXX_VERSION} CXX_STANDARD_REQUIRED On CXX_EXTENSIONS Off
	)

	target_compile_definitions(EmeraudeBaseBenchmarks PRIVATE ${EMERAUDE_COMPILE_DEFINITIONS})
	target_compile_options(EmeraudeBaseBenchmarks PRIVATE ${EMERAUDE_COMPILE_OPTIONS})
	target_link_libraries(EmeraudeBaseBenchmarks PRIVATE emeraude::base benchmark::benchmark benchmark::benchmark_main)
endif ()
