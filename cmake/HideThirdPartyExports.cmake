include_guard(GLOBAL)

include(CheckLinkerFlag)

# emeraude_base_target_hide_third_party_exports(<target> [EXCEPT <stem> ...])
#
# Keeps the vendored third-party static archives OUT of <target>'s dynamic export table.
#
# WHY THIS EXISTS (measured 2026-08-22)
# -------------------------------------
# On ELF the dynamic namespace is FLAT: a shared library that statically links a third-party
# archive re-exports that archive's symbols, and any object sitting LATER in the global search
# scope then binds to that copy instead of to its own dependency. libEmeraude.so exported 372
# png_* symbols coming from its static libpng 1.6.58, while the very same process also loaded the
# system libpng16.so.16 (1.6.48, pulled in by cairo and FreeType behind CEF) — so cairo, FreeType
# and gdk-pixbuf all ended up calling OUR libpng. Two measured consequences:
#   - libdecor compares what the global scope resolves "png_free" to against what its GTK3
#     plugin's own dependency chain resolves it to; the two addresses differed, so it printed
#     `Plugin "GTK3 plugin" uses conflicting symbol "png_free".` and fell back to its cairo
#     plugin — no native window decorations under Wayland.
#   - the silent half: the system libpng reaches its own exported entry points through the PLT
#     (107 JUMP_SLOT relocations, built without -Bsymbolic), so 1.6.48 code paths could execute
#     1.6.58 implementations on structures allocated with the 1.6.48 layout.
# The same reasoning covers every vendored C library that has a system twin: zlib, FreeType,
# libjpeg, harfbuzz, brotli, lzma, zstd, bz2, tiff, sndfile/FLAC/ogg/vorbis/opus, and LibreSSL
# — whose OpenSSL-compatible symbol names would interpose the system OpenSSL used by CEF and glib.
#
# WHAT IT DOES
# ------------
# Adds `--exclude-libs` for every archive found in EMERAUDE_EXT_LIBS_LIB_DIR, which turns their
# symbols LOCAL in the produced binary. Nothing changes for the target's own code — it keeps
# calling them normally — only the export table shrinks (engine, Release: 44127 -> 15518 exported
# symbols, every png_/jpeg_/FT_/sf_/TIFF_/deflate/ktx/meshopt_/ufbx_/SSL_/EVP_ symbol gone).
#
# Naming an archive the target does not link is a no-op, which is what makes globbing the
# ext-deps lib dir both complete and safe: adding a dependency never requires editing this file.
# The glob runs at configure time, so a change in that directory needs a reconfigure — the same
# contract as the source GLOBs.
#
# EXCEPT takes library STEMS (`EXCEPT jsoncpp` matches libjsoncpp.a and libjsoncppd.a). Use it
# ONLY for an archive whose symbols a consumer legitimately resolves FROM this binary. A C
# library that has a system twin must NEVER be excepted — that is the whole defect above.
#
# PLATFORMS
# ---------
# - MSVC: no-op. Nothing is auto-exported (EMERAUDE_USE_EXPLICIT_EXPORTS / EMEN_API drives the
#   generated .def), so an archive linked into the DLL never reaches its export table anyway.
# - Apple: no-op BY CONSTRUCTION, not by omission. dyld uses a TWO-LEVEL namespace — every
#   undefined symbol records which library must provide it, so a static copy inside a dylib
#   cannot interpose another library's symbols. ld64 has no --exclude-libs either (its
#   -load_hidden is a per-input flag that would have to be threaded through every Setup script).
# - ELF (Linux/BSD): the real target. GNU ld, gold, lld and mold accept the flag; it is probed
#   once, and skipped with a warning if the active linker rejects it.
function(emeraude_base_target_hide_third_party_exports target)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "EXCEPT")

	if ( MSVC OR APPLE )
		return ()
	endif ()

	check_linker_flag(CXX "LINKER:--exclude-libs,libnothing.a" EMERAUDE_BASE_LINKER_HAS_EXCLUDE_LIBS)

	if ( NOT EMERAUDE_BASE_LINKER_HAS_EXCLUDE_LIBS )
		message(WARNING "[EmeraudeBase] The active linker rejects --exclude-libs: the third-party archives linked into '${target}' will export their symbols and may interpose the system libraries of any plugin the process loads. See cmake/HideThirdPartyExports.cmake.")

		return ()
	endif ()

	file(GLOB _emeraude_base_third_party_archives "${EMERAUDE_EXT_LIBS_LIB_DIR}/*.a")

	if ( NOT _emeraude_base_third_party_archives )
		message(WARNING "[EmeraudeBase] No third-party archive found in '${EMERAUDE_EXT_LIBS_LIB_DIR}': nothing hidden for '${target}'.")

		return ()
	endif ()

	set(_emeraude_base_hidden_archives "")

	foreach ( _emeraude_base_archive IN LISTS _emeraude_base_third_party_archives )
		get_filename_component(_emeraude_base_archive_name "${_emeraude_base_archive}" NAME)

		set(_emeraude_base_archive_excepted Off)

		foreach ( _emeraude_base_stem IN LISTS ARG_EXCEPT )
			if ( _emeraude_base_archive_name MATCHES "^lib${_emeraude_base_stem}d?\\.a$" )
				set(_emeraude_base_archive_excepted On)

				break ()
			endif ()
		endforeach ()

		if ( NOT _emeraude_base_archive_excepted )
			list(APPEND _emeraude_base_hidden_archives "${_emeraude_base_archive_name}")
		endif ()
	endforeach ()

	list(SORT _emeraude_base_hidden_archives)
	list(LENGTH _emeraude_base_hidden_archives _emeraude_base_hidden_count)

	# GNU ld accepts both commas and colons as delimiters; colons keep the whole list inside a
	# single LINKER: argument, since CMake splits that one on commas.
	list(JOIN _emeraude_base_hidden_archives ":" _emeraude_base_hidden_list)

	target_link_options(${target} PRIVATE "LINKER:--exclude-libs,${_emeraude_base_hidden_list}")

	if ( ARG_EXCEPT )
		message(STATUS "[EmeraudeBase] '${target}': ${_emeraude_base_hidden_count} third-party archives hidden from the export table (kept exported: ${ARG_EXCEPT}).")
	else ()
		message(STATUS "[EmeraudeBase] '${target}': ${_emeraude_base_hidden_count} third-party archives hidden from the export table.")
	endif ()
endfunction()