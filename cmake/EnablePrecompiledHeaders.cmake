include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/STLPrecompiledHeaders.cmake)

# emeraude_base_target_enable_pch(<target> "<header-list>") — the list is ONE quoted argument.
#
# MSVC: a target using WINDOWS_EXPORT_ALL_SYMBOLS must NOT call this — the PCH object's marker
# symbols leak into the auto-generated exports.def and the link fails (LNK2001 on a bogus '__').
# That is the caller's business (only a SHARED library can be concerned), so the check lives at
# the call site — see emeraude-engine/CMakeLists.txt and docs/windows-export-api.md.
function(emeraude_base_target_enable_pch target headers)
	if ( NOT EMERAUDE_ENABLE_PCH )
		return ()
	endif ()

	target_precompile_headers(${target} PRIVATE ${headers})

	get_target_property(_emeraude_base_pch_target_sources ${target} SOURCES)

	foreach ( _emeraude_base_pch_source IN LISTS _emeraude_base_pch_target_sources )
		if ( _emeraude_base_pch_source MATCHES "\\.(m|mm|M)$" )
			set_source_files_properties(
				${_emeraude_base_pch_source}
				TARGET_DIRECTORY ${target}
				PROPERTIES SKIP_PRECOMPILE_HEADERS ON
			)
		endif ()
	endforeach ()
endfunction()
