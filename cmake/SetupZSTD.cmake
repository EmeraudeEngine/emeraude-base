if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling ZSTD library ...")

if ( MSVC )
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ${EMERAUDE_EXT_LIBS_PATH}/lib/zstd_static.lib)
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ${EMERAUDE_EXT_LIBS_PATH}/lib/libzstd.a)
endif ()
