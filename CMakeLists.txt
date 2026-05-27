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
	VERSION 1.0.0
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
option(EMERAUDE_ENABLE_PCH "Enable precompiled headers." Off)
option(EMERAUDE_ENABLE_TESTS "Build the GoogleTest suite(s)." Off)
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
set(CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo)

if ( NOT CMAKE_BUILD_TYPE )
	if ( MSVC AND MSVC_BUILD_TYPE )
		set(CMAKE_BUILD_TYPE ${MSVC_BUILD_TYPE})
	elseif ( MSVC )
		message(FATAL_ERROR "[EmeraudeBase] Set -DMSVC_BUILD_TYPE=Debug|Release on Windows.")
	else ()
		set(CMAKE_BUILD_TYPE Release)
	endif ()
endif ()

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

if ( UNIX AND NOT APPLE )
	set(EMERAUDE_EXT_LIBS_DIRNAME "linux.${APP_ARCH}-${CMAKE_BUILD_TYPE}")
elseif ( APPLE )
	if ( CMAKE_OSX_ARCHITECTURES )
		set(APP_ARCH ${CMAKE_OSX_ARCHITECTURES})
	endif ()
	set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
	set(EMERAUDE_EXT_LIBS_DIRNAME "mac.${APP_ARCH}-${CMAKE_BUILD_TYPE}")
elseif ( MSVC )
	if ( EMERAUDE_USE_STATIC_RUNTIME )
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
		set(EMERAUDE_EXT_LIBS_DIRNAME "windows.${APP_ARCH}-${CMAKE_BUILD_TYPE}-MT")
	else ()
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
		set(EMERAUDE_EXT_LIBS_DIRNAME "windows.${APP_ARCH}-${CMAKE_BUILD_TYPE}-MD")
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

# Download + extract the prebuilt archive when EMERAUDE_EXT_LIBS_PATH is absent.
# Skipped automatically when it is a symlink (developer pointing at a local build).
option(EMERAUDE_DL_DEPENDENCIES "Download the prebuilt external dependencies if absent." On)
if ( EMERAUDE_DL_DEPENDENCIES )
	include(InstallDependencies)
endif ()

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
		$<$<CONFIG:Debug>:DEBUG>
		$<$<CONFIG:Release>:NDEBUG>
		_WIN32_WINNT=0x0A00
		CACHE STRING "Project-wide compile definitions (Windows)." FORCE
	)
	set(EMERAUDE_COMPILE_OPTIONS
		$<IF:$<BOOL:${EMERAUDE_DISABLE_RTTI}>,/GR-,/GR>
		$<$<BOOL:${EMERAUDE_DISABLE_EXCEPTIONS}>:/EHs->
		$<$<BOOL:${EMERAUDE_DISABLE_EXCEPTIONS}>:/EHc->
		$<$<NOT:$<BOOL:${EMERAUDE_DISABLE_EXCEPTIONS}>>:/EHsc>
		$<$<CONFIG:Debug>:/RTC1>
		$<$<CONFIG:Debug>:/Od>
		$<$<CONFIG:Debug>:/Zi>
		$<$<CONFIG:Release>:/O2>
		$<$<CONFIG:Release>:$<IF:$<BOOL:${EMERAUDE_ENABLE_FP_OPTIMIZATION}>,/fp:fast,/fp:precise>>
		$<$<CONFIG:Release>:/Ob3>
		$<$<CONFIG:Release>:/GF>
		/MP /Gy /W4
		$<$<NOT:$<BOOL:${EMERAUDE_DISABLE_PARANOID_COMPILATION}>>:/WX>
		/wd4100 /wd4127 /wd4324 /wd4481 /wd4512
		$<$<BOOL:${EMERAUDE_DISABLE_EXCEPTIONS}>:/wd4530>
		/wd4702 /wd4996
		CACHE STRING "Project-wide compile options (Windows)." FORCE
	)
elseif ( APPLE )
	set(EMERAUDE_COMPILE_DEFINITIONS
		$<$<CONFIG:Debug>:DEBUG>
		$<$<CONFIG:Release>:NDEBUG>
		CACHE STRING "Project-wide compile definitions (macOS)." FORCE
	)
	set(EMERAUDE_COMPILE_OPTIONS
		$<IF:$<BOOL:${EMERAUDE_DISABLE_RTTI}>,-fno-rtti,-frtti>
		$<IF:$<BOOL:${EMERAUDE_DISABLE_EXCEPTIONS}>,-fno-exceptions,-fexceptions>
		$<$<CONFIG:Debug>:-O0>
		$<$<CONFIG:Debug>:-g>
		$<$<CONFIG:Debug>:-Wall>
		$<$<CONFIG:Debug>:-Wextra>
		$<$<CONFIG:Debug>:-Wpedantic>
		$<$<CONFIG:Debug>:-fno-omit-frame-pointer>
		$<$<CONFIG:Release>:-O2>
		$<$<CONFIG:Release>:$<IF:$<BOOL:${EMERAUDE_ENABLE_FP_OPTIMIZATION}>,-ffast-math,-fno-fast-math>>
		$<$<CONFIG:Release>:-fdata-sections>
		$<$<CONFIG:Release>:-ffunction-sections>
		$<$<CONFIG:Release>:-fstack-protector-strong>
		$<$<NOT:$<BOOL:${EMERAUDE_DISABLE_PARANOID_COMPILATION}>>:-Werror>
		-Wformat -Wformat-security
		-Wno-unknown-pragmas -Wno-unused-parameter -Wno-unused-private-field
		-Wno-nullability-extension -Wno-nullability-completeness
		-Wno-gnu-zero-variadic-macro-arguments
		CACHE STRING "Project-wide compile options (macOS)." FORCE
	)
elseif ( UNIX )
	set(EMERAUDE_COMPILE_DEFINITIONS
		$<$<CONFIG:Debug>:DEBUG>
		$<$<CONFIG:Release>:NDEBUG>
		$<$<CONFIG:Release>:_FORTIFY_SOURCE=2>
		CACHE STRING "Project-wide compile definitions (Linux)." FORCE
	)
	set(EMERAUDE_COMPILE_OPTIONS
		$<IF:$<BOOL:${EMERAUDE_DISABLE_RTTI}>,-fno-rtti,-frtti>
		$<IF:$<BOOL:${EMERAUDE_DISABLE_EXCEPTIONS}>,-fno-exceptions,-fexceptions>
		$<$<CONFIG:Debug>:-O0>
		$<$<CONFIG:Debug>:-g>
		$<$<CONFIG:Debug>:-Wall>
		$<$<CONFIG:Debug>:-Wextra>
		$<$<CONFIG:Debug>:-Wpedantic>
		$<$<CONFIG:Debug>:-fno-omit-frame-pointer>
		$<$<CONFIG:Release>:-O2>
		$<$<CONFIG:Release>:$<IF:$<BOOL:${EMERAUDE_ENABLE_FP_OPTIMIZATION}>,-ffast-math,-fno-fast-math>>
		$<$<CONFIG:Release>:-fdata-sections>
		$<$<CONFIG:Release>:-ffunction-sections>
		$<$<CONFIG:Release>:-fstack-protector-strong>
		$<$<NOT:$<BOOL:${EMERAUDE_DISABLE_PARANOID_COMPILATION}>>:-Werror>
		-Wformat -Wformat-security
		-Wno-unknown-pragmas -Wno-unused-parameter
		CACHE STRING "Project-wide compile options (Linux)." FORCE
	)
endif ()

#######################################################################
## Modules
# TRANSITION: a single umbrella target (emeraude::base) compiling all of src/.
# The per-module split (emeraude::base::math INTERFACE, ::io/::pixel/::wave OBJECT, …)
# is a planned internal refactor — see docs/module-map.md.
# platform (header-only, also usable standalone)
add_library(emeraude_base_platform INTERFACE)
target_include_directories(emeraude_base_platform INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
	$<INSTALL_INTERFACE:include>
)
add_library(emeraude::base::platform ALIAS emeraude_base_platform)

# umbrella — sources listed explicitly (no globbing); see cmake/PrepareBaseSourceFiles.cmake
include(PrepareBaseSourceFiles)

add_library(emeraude_base ${EMERAUDE_BASE_LIBRARY_TYPE} ${EMERAUDE_BASE_SOURCES})
add_library(emeraude::base ALIAS emeraude_base)

set_target_properties(emeraude_base PROPERTIES
	OUTPUT_NAME EmeraudeBase
	C_STANDARD ${EMERAUDE_C_VERSION} C_STANDARD_REQUIRED On C_EXTENSIONS Off
	CXX_STANDARD ${EMERAUDE_CXX_VERSION} CXX_STANDARD_REQUIRED On CXX_EXTENSIONS Off
	POSITION_INDEPENDENT_CODE ON
	VERSION ${PROJECT_VERSION}
)

target_include_directories(emeraude_base PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
	$<INSTALL_INTERFACE:include>
)
target_include_directories(emeraude_base SYSTEM PUBLIC
	${EMERAUDE_EXT_LIBS_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/dependencies/asio/include
	${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tinysoundfont
)
target_link_directories(emeraude_base PUBLIC ${EMERAUDE_EXT_LIBS_LIB_DIR})

target_compile_definitions(emeraude_base PUBLIC ${EMERAUDE_COMPILE_DEFINITIONS})
target_compile_options(emeraude_base PUBLIC ${EMERAUDE_COMPILE_OPTIONS})

target_link_libraries(emeraude_base PUBLIC emeraude::base::platform)

# External dependencies — reuse the engine Setup scripts, retargeted to emeraude_base.
set(TARGET_BINARY_FOR_SETUP emeraude_base)
set(LOCAL_LIB_DIR ${EMERAUDE_EXT_LIBS_PATH})
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

	set_target_properties(EmeraudeBaseUnitTests PROPERTIES
		C_STANDARD ${EMERAUDE_C_VERSION} C_STANDARD_REQUIRED On C_EXTENSIONS Off
		CXX_STANDARD ${EMERAUDE_CXX_VERSION} CXX_STANDARD_REQUIRED On CXX_EXTENSIONS Off
	)

	target_compile_definitions(EmeraudeBaseUnitTests PRIVATE ${EMERAUDE_COMPILE_DEFINITIONS})
	target_compile_options(EmeraudeBaseUnitTests PRIVATE ${EMERAUDE_COMPILE_OPTIONS})
	target_link_libraries(EmeraudeBaseUnitTests PRIVATE emeraude::base gtest_main)

	enable_testing()
	# Tests load fixtures via "./assets/…" — run them from resources/ where those live.
	gtest_discover_tests(EmeraudeBaseUnitTests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources)
endif ()
