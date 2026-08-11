message("Installing external dependencies ...")

# Local-libs bypass: if the resolved target EMERAUDE_EXT_LIBS_PATH already exists
# as a symbolic link to a directory (POSIX) or a directory junction (Windows
# `mklink /J` / `mklink /D`), trust the local setup and skip the download +
# extraction entirely.
#
# This is exactly the ext-deps-generator build host itself: it produces the
# release archive, so it already has the libraries locally and points
# EMERAUDE_EXT_LIBS_PATH at its own ext-deps-generator output via a symlink —
# there is nothing to fetch. The check keys on EMERAUDE_EXT_LIBS_PATH, which
# carries the full resolved folder name (including the host glibc / sdk / CRT
# tag), so the bypass fires only when a symlink with the *correct* name for this
# exact configuration is present; a stale symlink from an older naming (e.g. the
# pre-v013 tag-less 'linux.x86_64-Release') is ignored and a real archive is
# fetched instead.
if ( IS_SYMLINK ${EMERAUDE_EXT_LIBS_PATH} AND IS_DIRECTORY ${EMERAUDE_EXT_LIBS_PATH} )
	message("External dependencies directory '${EMERAUDE_EXT_LIBS_PATH}' is a symbolic link to local libraries — bypassing download and extraction.")
	return ()
endif ()

# The folder grammar is the single source of truth: EMERAUDE_EXT_LIBS_DIRNAME
# (computed in CMakeLists.txt, a mirror of ext-deps-generator's
# BuildConfig.build_suffix) already carries the OS prefix, arch, build type and
# the per-OS ABI tag (Linux host glibc, macOS deployment target, Windows CRT).
#
# From v013 the release archive is exactly that folder name with a
# '.v<version>.zip' suffix (e.g. linux.x86_64-Release-glibc2.35.v013.zip,
# macos.arm64-Release-sdk12.0.v013.zip, windows.x86_64-Release-MD.v013.zip), so
# we derive the filename from the folder rather than recomputing an independent
# (and historically divergent) name here — this also removes the old per-distro
# lsb_release logic, superseded by the glibc tag.
if ( NOT DEFINED EMERAUDE_EXT_LIBS_DIRNAME )
	message(FATAL_ERROR "EMERAUDE_EXT_LIBS_DIRNAME is not set — CMakeLists.txt must compute the external dependencies folder name before include(InstallDependencies).")
endif ()

# Plain (non-cached) variable on purpose: this is a maintainer-owned code
# constant bumped by editing this line. A CACHE entry would let a stale value
# from a previously-configured build dir (e.g. "v012") silently win over a bump.
# The leading 'v' is part of the value: it is both the GitHub release tag and
# the archive filename token (e.g. ...-glibc2.35.v013.zip).
set(EXTERNAL_DEPENDENCIES_VERSION "v014")
set(EXTERNAL_DEPENDENCIES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")

# Candidate dirnames, in priority order: the exact host tag first, then the
# published floor fallback (Linux only — DIRNAME_FALLBACK is undefined on
# macOS/Windows, where a single ABI tag is published). A static lib linked
# against the floor glibc runs on any newer host, so falling back to it is safe
# when no exact-host archive is published.
set(_emeraude_ext_candidates "${EMERAUDE_EXT_LIBS_DIRNAME}")

if ( DEFINED EMERAUDE_EXT_LIBS_DIRNAME_FALLBACK AND NOT EMERAUDE_EXT_LIBS_DIRNAME_FALLBACK STREQUAL EMERAUDE_EXT_LIBS_DIRNAME )
	list(APPEND _emeraude_ext_candidates "${EMERAUDE_EXT_LIBS_DIRNAME_FALLBACK}")
endif ()

# Resolve the first candidate that is already extracted, already cached as a
# valid archive, or downloadable. A fresh download forces re-extraction below.
set(EXTERNAL_DEPENDENCIES_RESOLVED_DIRNAME "")
set(EXTERNAL_DEPENDENCIES_PATH "")
set(EXTERNAL_DEPENDENCIES_FRESH_DOWNLOAD FALSE)
set(_emeraude_ext_tried "")

file(MAKE_DIRECTORY ${EXTERNAL_DEPENDENCIES_DIR})

foreach ( _cand IN LISTS _emeraude_ext_candidates )
	set(_cand_filename "${_cand}.${EXTERNAL_DEPENDENCIES_VERSION}.zip")
	set(_cand_archive "${EXTERNAL_DEPENDENCIES_DIR}/${_cand_filename}")
	set(_cand_extracted "${EXTERNAL_DEPENDENCIES_DIR}/${_cand}")
	set(_cand_url "https://github.com/EmeraudeEngine/ext-deps-generator/releases/download/${EXTERNAL_DEPENDENCIES_VERSION}/${_cand_filename}")
	string(APPEND _emeraude_ext_tried "  ${_cand_url}\n")

	# Already extracted (real dir or local symlink): nothing to fetch.
	if ( EXISTS ${_cand_extracted} )
		message("External dependencies '${_cand}' already present — no download needed.")
		set(EXTERNAL_DEPENDENCIES_RESOLVED_DIRNAME "${_cand}")
		set(EXTERNAL_DEPENDENCIES_PATH "${_cand_archive}")
		break ()
	endif ()

	# Archive already cached from a previous run (kept only when it was a valid ZIP): reuse it.
	if ( EXISTS ${_cand_archive} )
		message("External dependencies archive '${_cand_filename}' is present !")
		set(EXTERNAL_DEPENDENCIES_RESOLVED_DIRNAME "${_cand}")
		set(EXTERNAL_DEPENDENCIES_PATH "${_cand_archive}")
		break ()
	endif ()

	message("Downloading external dependencies archive '${_cand_filename}' from ${_cand_url} ...")

	file(DOWNLOAD
		${_cand_url}
		${_cand_archive}
		SHOW_PROGRESS
		STATUS _cand_status
		LOG _cand_log
	)

	list(GET _cand_status 0 _cand_code)
	list(GET _cand_status 1 _cand_message)

	if ( NOT _cand_code EQUAL 0 )
		# Transport-level failure (offline, DNS, TLS): drop the partial file and try the next candidate.
		message("  download failed (transport): ${_cand_code} (${_cand_message}) — trying next candidate.")
		file(REMOVE ${_cand_archive})
		continue ()
	endif ()

	# file(DOWNLOAD) reports curl code 0 even for an HTTP 404: GitHub returns a
	# small HTML / 'Not Found' body, not the asset. Detect a real archive by its
	# ZIP local-file-header magic (PK\x03\x04 = 0x504b0304) instead of trusting
	# the status code, so a missing asset falls through to the next candidate
	# rather than being extracted as garbage.
	set(_cand_magic "")

	if ( EXISTS ${_cand_archive} )
		file(READ ${_cand_archive} _cand_magic LIMIT 4 HEX)
	endif ()

	if ( NOT _cand_magic MATCHES "^504b" )
		message("  downloaded payload is not a ZIP archive (likely a 404) — trying next candidate.")
		file(REMOVE ${_cand_archive})
		continue ()
	endif ()

	set(EXTERNAL_DEPENDENCIES_RESOLVED_DIRNAME "${_cand}")
	set(EXTERNAL_DEPENDENCIES_PATH "${_cand_archive}")
	set(EXTERNAL_DEPENDENCIES_FRESH_DOWNLOAD TRUE)
	break ()
endforeach ()

if ( EXTERNAL_DEPENDENCIES_RESOLVED_DIRNAME STREQUAL "" )
	message(FATAL_ERROR
		"Failed to obtain the external dependencies archive for this configuration.\n"
		"No candidate resolved (exact host tag, then the published floor fallback).\n"
		"URLs tried:\n${_emeraude_ext_tried}"
		"If this host is offline, drop the archive manually into ${EXTERNAL_DEPENDENCIES_DIR}, or point EMERAUDE_EXT_LIBS_PATH at a local build via a symlink."
	)
endif ()

# When the resolved candidate is not the primary host tag, repoint the cached
# path/include/lib variables (originally computed from the host tag in
# CMakeLists.txt) at the archive that actually resolved. The extraction step and
# every downstream Setup*.cmake read these, so they must name the real folder.
if ( NOT EXTERNAL_DEPENDENCIES_RESOLVED_DIRNAME STREQUAL EMERAUDE_EXT_LIBS_DIRNAME )
	message("External dependencies: no archive for '${EMERAUDE_EXT_LIBS_DIRNAME}', falling back to the published floor '${EXTERNAL_DEPENDENCIES_RESOLVED_DIRNAME}'.")

	set(EMERAUDE_EXT_LIBS_DIRNAME "${EXTERNAL_DEPENDENCIES_RESOLVED_DIRNAME}")
	set(EMERAUDE_EXT_LIBS_PATH        "${EXTERNAL_DEPENDENCIES_DIR}/${EXTERNAL_DEPENDENCIES_RESOLVED_DIRNAME}"         CACHE PATH "ext-deps-generator output root." FORCE)
	set(EMERAUDE_EXT_LIBS_INCLUDE_DIR "${EXTERNAL_DEPENDENCIES_DIR}/${EXTERNAL_DEPENDENCIES_RESOLVED_DIRNAME}/include" CACHE PATH "External dependencies include dir." FORCE)
	set(EMERAUDE_EXT_LIBS_LIB_DIR     "${EXTERNAL_DEPENDENCIES_DIR}/${EXTERNAL_DEPENDENCIES_RESOLVED_DIRNAME}/lib"     CACHE PATH "External dependencies library dir." FORCE)
endif ()

# Extract the archive when:
#   - the extracted directory doesn't exist yet, OR
#   - we just freshly downloaded the archive (force overwrite — version bump or replaced asset).
if ( EXTERNAL_DEPENDENCIES_FRESH_DOWNLOAD OR NOT EXISTS ${EMERAUDE_EXT_LIBS_PATH} )
	if ( EXISTS ${EMERAUDE_EXT_LIBS_PATH} )
		message("Removing previous extracted directory '${EMERAUDE_EXT_LIBS_PATH}' before re-extracting ...")
		file(REMOVE_RECURSE ${EMERAUDE_EXT_LIBS_PATH})
	endif ()

	message("Extracting archive ${EXTERNAL_DEPENDENCIES_PATH} ...")

	file(ARCHIVE_EXTRACT
		INPUT ${EXTERNAL_DEPENDENCIES_PATH}
		DESTINATION ${EXTERNAL_DEPENDENCIES_DIR}
		VERBOSE
	)

	if ( NOT EXISTS ${EMERAUDE_EXT_LIBS_PATH} )
		message(FATAL_ERROR
			"Archive extraction did not produce the expected directory.\n"
			"  Archive:  ${EXTERNAL_DEPENDENCIES_PATH}\n"
			"  Expected: ${EMERAUDE_EXT_LIBS_PATH}\n"
			"The archive contents may not match the engine's directory convention."
		)
	endif ()
else ()
	message("External dependencies binaries present !")
endif ()
