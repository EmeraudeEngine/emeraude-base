# emeraude-base — shared precompiled-header helper.
#
# emeraude-base owns the project-wide STL hot-set precompiled header (STLPrecompiledHeaders.hpp
# at the repo root — STL ONLY, see that file for the strict rule). This module exposes a
# single helper that attaches it to a target.
#
# Usage (consumer CMakeLists, after add_subdirectory(emeraude-base) has run so that
# EMERAUDE_BASE_PCH_FILE / EMERAUDE_BASE_CMAKE_DIR are defined):
#   list(APPEND CMAKE_MODULE_PATH ${EMERAUDE_BASE_CMAKE_DIR})   # the engine already does this
#   include(EnablePrecompiledHeaders)
#   emeraude_base_target_enable_pch(MyTarget)
#
# This helper attaches ONLY the shared STL hot-set. A consumer that needs heavy third-party
# headers on top (e.g. a consumer library + Eigen) appends them itself with a second
# target_precompile_headers() call — base stays agnostic of consumer-specific dependencies.
#
# Design notes:
#   - Gated by EMERAUDE_ENABLE_PCH (declared in emeraude-base's CMakeLists). When OFF the
#     helper is a no-op, so call sites can stay unconditional.
#   - Applied PRIVATE: the PCH speeds up the target's OWN translation units and never
#     propagates to consumers (no INTERFACE_PRECOMPILE_HEADERS leak onto a consumer, …).
#   - Restricted to CXX via $<COMPILE_LANGUAGE:CXX> so it is never attached to the C / ASM
#     translation units a target may also compile. Objective-C(++) sources (.m/.mm) need an
#     explicit SKIP_PRECOMPILE_HEADERS on top: without the OBJCXX language enabled CMake
#     classifies them as CXX (so the genex matches), but clang compiles them as Objective-C++
#     and rejects a pure-C++ PCH.
#   - Each target compiles its OWN PCH binary from the shared header list (NO REUSE_FROM):
#     targets with divergent compile definitions/options never clash, and a consumer is free
#     to layer its own heavy third-party headers (Eigen, Vulkan, …) on top in its own PCH.

include_guard(GLOBAL)

function(emeraude_base_target_enable_pch target)
	if ( NOT EMERAUDE_ENABLE_PCH )
		return ()
	endif ()

	# WINDOWS/MSVC GUARD (temporary): the engine DLL is exported via WINDOWS_EXPORT_ALL_SYMBOLS,
	# whose auto-generated exports.def scans every input .obj. A per-target PCH object carries
	# compiler marker symbols (__@@_PchSym_@00@…) that leak into the .def and break the link
	# (LNK2001 on a bogus '__'). Until the explicit-export migration is complete (the engine option
	# EMERAUDE_USE_EXPLICIT_EXPORTS, which drops WINDOWS_EXPORT_ALL_SYMBOLS), the PCH is a no-op on
	# MSVC so the cascade links. Linux/macOS (symbol-visibility export) are unaffected. Lift this
	# guard once EMERAUDE_USE_EXPLICIT_EXPORTS is the default. See
	# emeraude-engine/docs/windows-export-api.md.
	if ( MSVC AND NOT EMERAUDE_USE_EXPLICIT_EXPORTS )
		message(STATUS "[EmeraudeBase] PCH disabled on MSVC for '${target}' (WINDOWS_EXPORT_ALL_SYMBOLS + PCH break the DLL link; pending explicit-export migration).")
		return ()
	endif ()

	if ( NOT EMERAUDE_BASE_PCH_FILE )
		message(FATAL_ERROR "[EmeraudeBase] EMERAUDE_BASE_PCH_FILE is not set. Make sure emeraude-base has been add_subdirectory'd before calling emeraude_base_target_enable_pch(${target}).")
	endif ()

	target_precompile_headers(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${EMERAUDE_BASE_PCH_FILE}>")

	# Objective-C(++) sources: when the OBJCXX language is not enabled, CMake classifies .mm
	# files as CXX, so the $<COMPILE_LANGUAGE:CXX> guard above cannot exclude them — yet clang
	# still compiles them as Objective-C++ (driven by the file extension) and rejects the
	# pure-C++ PCH ("error: Objective-C was disabled in PCH file but is currently enabled").
	# Skip the PCH explicitly for those sources.
	get_target_property(_emeraude_base_pch_target_sources ${target} SOURCES)

	foreach ( _emeraude_base_pch_source IN LISTS _emeraude_base_pch_target_sources )
		if ( _emeraude_base_pch_source MATCHES "\\.(m|mm)$" )
			set_source_files_properties(
				${_emeraude_base_pch_source}
				TARGET_DIRECTORY ${target}
				PROPERTIES SKIP_PRECOMPILE_HEADERS ON
			)
		endif ()
	endforeach ()
endfunction()
