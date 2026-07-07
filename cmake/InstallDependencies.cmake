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
set(EXTERNAL_DEPENDENCIES_VERSION "v013")
set(EXTERNAL_DEPENDENCIES_FILENAME "${EMERAUDE_EXT_LIBS_DIRNAME}.${EXTERNAL_DEPENDENCIES_VERSION}.zip")

# Resolve URL and local paths.
# Archives are hosted as GitHub Release assets, tagged '<version>' (e.g. v013).
set(EXTERNAL_DEPENDENCIES_URL "https://github.com/EmeraudeEngine/ext-deps-generator/releases/download/${EXTERNAL_DEPENDENCIES_VERSION}/${EXTERNAL_DEPENDENCIES_FILENAME}")
set(EXTERNAL_DEPENDENCIES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
set(EXTERNAL_DEPENDENCIES_PATH "${EXTERNAL_DEPENDENCIES_DIR}/${EXTERNAL_DEPENDENCIES_FILENAME}")

# Download the archive if it isn't cached yet.
# A fresh download forces re-extraction afterwards (so version bumps overwrite the old extracted directory).
set(EXTERNAL_DEPENDENCIES_FRESH_DOWNLOAD FALSE)

if ( NOT EXISTS ${EXTERNAL_DEPENDENCIES_PATH} )
	message("External dependencies archive '${EXTERNAL_DEPENDENCIES_FILENAME}' is not present ! Downloading it from ${EXTERNAL_DEPENDENCIES_URL} ...")

	file(MAKE_DIRECTORY ${EXTERNAL_DEPENDENCIES_DIR})

	file(DOWNLOAD
		${EXTERNAL_DEPENDENCIES_URL}
		${EXTERNAL_DEPENDENCIES_PATH}
		SHOW_PROGRESS
		STATUS EXTERNAL_DEPENDENCIES_DOWNLOAD_STATUS
		LOG EXTERNAL_DEPENDENCIES_DOWNLOAD_LOG
	)

	list(GET EXTERNAL_DEPENDENCIES_DOWNLOAD_STATUS 0 EXTERNAL_DEPENDENCIES_DOWNLOAD_CODE)
	list(GET EXTERNAL_DEPENDENCIES_DOWNLOAD_STATUS 1 EXTERNAL_DEPENDENCIES_DOWNLOAD_MESSAGE)

	if ( NOT EXTERNAL_DEPENDENCIES_DOWNLOAD_CODE EQUAL 0 )
		# Remove the (potentially partial / HTML-error) file so we don't try to extract garbage.
		file(REMOVE ${EXTERNAL_DEPENDENCIES_PATH})

		message(FATAL_ERROR
			"Failed to download external dependencies.\n"
			"  URL:    ${EXTERNAL_DEPENDENCIES_URL}\n"
			"  Status: ${EXTERNAL_DEPENDENCIES_DOWNLOAD_CODE} (${EXTERNAL_DEPENDENCIES_DOWNLOAD_MESSAGE})\n"
			"  Log:\n${EXTERNAL_DEPENDENCIES_DOWNLOAD_LOG}"
		)
	endif ()

	set(EXTERNAL_DEPENDENCIES_FRESH_DOWNLOAD TRUE)
else ()
	message("External dependencies archive '${EXTERNAL_DEPENDENCIES_FILENAME}' is present !")
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
