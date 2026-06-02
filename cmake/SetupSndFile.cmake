if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

# Static libsndfile pulls codec dependencies (FLAC, Vorbis, Vorbisenc, Opus, mpg123, mp3lame)
# which in turn depend on Ogg. Link order matters for the GNU/lld linkers: dependent libs
# must appear before the libs they depend on. Ogg goes last as it is consumed by FLAC,
# Vorbis and Vorbisenc.
if ( MSVC )
	message("Enabling SNDFile library from local source ...")

	# mpg123 uses PathCombineW / PathIsRelativeW / PathIsUNCW from shlwapi
	# when WANT_WIN32_UNICODE is on (the default in our recipe). The official
	# mpg123-targets.cmake declares this as INTERFACE_LINK_LIBRARIES, but
	# since we link mpg123.lib by path we must add shlwapi.lib here too.
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE
		${EMERAUDE_EXT_LIBS_PATH}/lib/sndfile.lib
		${EMERAUDE_EXT_LIBS_PATH}/lib/FLAC.lib
		${EMERAUDE_EXT_LIBS_PATH}/lib/vorbisenc.lib
		${EMERAUDE_EXT_LIBS_PATH}/lib/vorbis.lib
		${EMERAUDE_EXT_LIBS_PATH}/lib/opus.lib
		${EMERAUDE_EXT_LIBS_PATH}/lib/mpg123.lib
		${EMERAUDE_EXT_LIBS_PATH}/lib/mp3lame.lib
		${EMERAUDE_EXT_LIBS_PATH}/lib/ogg.lib
		shlwapi.lib
	)
elseif ( EMERAUDE_USE_SYSTEM_LIBS )
	message("Enabling SNDFile library from system ...")

	find_package(PkgConfig REQUIRED)

	pkg_check_modules(SNDFILE REQUIRED sndfile)

	target_include_directories(${TARGET_BINARY_FOR_SETUP} SYSTEM PUBLIC ${SNDFILE_INCLUDE_DIRS})

	target_link_directories(${TARGET_BINARY_FOR_SETUP} PUBLIC ${SNDFILE_LIBRARY_DIRS})

	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ${SNDFILE_LIBRARIES})
else ()
	message("Enabling SNDFile library from local source ...")

	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE
		${EMERAUDE_EXT_LIBS_PATH}/lib/libsndfile.a
		${EMERAUDE_EXT_LIBS_PATH}/lib/libFLAC.a
		${EMERAUDE_EXT_LIBS_PATH}/lib/libvorbisenc.a
		${EMERAUDE_EXT_LIBS_PATH}/lib/libvorbis.a
		${EMERAUDE_EXT_LIBS_PATH}/lib/libopus.a
		${EMERAUDE_EXT_LIBS_PATH}/lib/libmpg123.a
		${EMERAUDE_EXT_LIBS_PATH}/lib/libmp3lame.a
		${EMERAUDE_EXT_LIBS_PATH}/lib/libogg.a
		m
	)
endif ()
