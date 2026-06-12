if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling FastGLTF library ...")

if ( MSVC )
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE
		debug "${EMERAUDE_EXT_LIBS_PATH}/lib/fastgltf.lib"
		optimized "${EMERAUDE_EXT_LIBS_PATH}/lib/fastgltf.lib"
	)
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE "${EMERAUDE_EXT_LIBS_PATH}/lib/libfastgltf.a")
endif ()
