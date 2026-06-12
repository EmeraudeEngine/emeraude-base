if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling zlib library ...")

if ( MSVC )
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PUBLIC
		debug "${EMERAUDE_EXT_LIBS_PATH}/lib/zlibstaticd.lib"
		optimized "${EMERAUDE_EXT_LIBS_PATH}/lib/zlibstatic.lib"
	)
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE "${EMERAUDE_EXT_LIBS_PATH}/lib/libz.a")
endif ()
