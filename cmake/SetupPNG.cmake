if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling LibPNG library ...")

if ( MSVC )
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PUBLIC
		debug "${EMERAUDE_EXT_LIBS_PATH}/lib/libpng16_staticd.lib"
		optimized "${EMERAUDE_EXT_LIBS_PATH}/lib/libpng16_static.lib"
	)
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE
		debug "${EMERAUDE_EXT_LIBS_PATH}/lib/libpng16d.a"
		optimized "${EMERAUDE_EXT_LIBS_PATH}/lib/libpng16.a"
	)
endif ()
