if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

if ( EMERAUDE_USE_SYSTEM_LIBS )
	message("Enabling zlib library from system ...")

	find_package(PkgConfig REQUIRED)

	pkg_check_modules(ZLIB REQUIRED zlib)

	target_include_directories(${TARGET_BINARY_FOR_SETUP} SYSTEM PUBLIC ${ZLIB_INCLUDE_DIRS})

	target_link_directories(${TARGET_BINARY_FOR_SETUP} PUBLIC ${ZLIB_LIBRARY_DIRS})

	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ${ZLIB_LIBRARIES})
else ()
	message("Enabling zlib library from local source ...")

	if ( MSVC )
		target_link_libraries(${TARGET_BINARY_FOR_SETUP} PUBLIC
			debug "${EMERAUDE_EXT_LIBS_PATH}/lib/zlibstaticd.lib"
			optimized "${EMERAUDE_EXT_LIBS_PATH}/lib/zlibstatic.lib"
		)
	else ()
		target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE "${EMERAUDE_EXT_LIBS_PATH}/lib/libz.a")
	endif ()
endif ()
