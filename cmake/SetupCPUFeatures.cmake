if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling cpu_features library from local source ...")

if ( MSVC )
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ${EMERAUDE_EXT_LIBS_PATH}/lib/cpu_features.lib)
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ${EMERAUDE_EXT_LIBS_PATH}/lib/libcpu_features.a)
endif ()
