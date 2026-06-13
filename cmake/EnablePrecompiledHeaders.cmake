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
# headers on top (e.g. app_kernel + Eigen) appends them itself with a second
# target_precompile_headers() call — base stays agnostic of consumer-specific dependencies.
#
# Design notes:
#   - Gated by EMERAUDE_ENABLE_PCH (declared in emeraude-base's CMakeLists). When OFF the
#     helper is a no-op, so call sites can stay unconditional.
#   - Applied PRIVATE: the PCH speeds up the target's OWN translation units and never
#     propagates to consumers (no INTERFACE_PRECOMPILE_HEADERS leak onto app_system, …).
#   - Restricted to CXX via $<COMPILE_LANGUAGE:CXX> so it is never attached to the C / ASM
#     translation units a target may also compile.
#   - Each target compiles its OWN PCH binary from the shared header list (NO REUSE_FROM):
#     targets with divergent compile definitions/options never clash, and a consumer is free
#     to layer its own heavy third-party headers (Eigen, Vulkan, …) on top in its own PCH.

include_guard(GLOBAL)

function(emeraude_base_target_enable_pch target)
	if ( NOT EMERAUDE_ENABLE_PCH )
		return ()
	endif ()

	if ( NOT EMERAUDE_BASE_PCH_FILE )
		message(FATAL_ERROR "[EmeraudeBase] EMERAUDE_BASE_PCH_FILE is not set. Make sure emeraude-base has been add_subdirectory'd before calling emeraude_base_target_enable_pch(${target}).")
	endif ()

	target_precompile_headers(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${EMERAUDE_BASE_PCH_FILE}>")
endfunction()
