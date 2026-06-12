if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling TagLib library ...")

if ( MSVC )
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ${EMERAUDE_EXT_LIBS_PATH}/lib/tag.lib)
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ${EMERAUDE_EXT_LIBS_PATH}/lib/libtag.a)
endif ()

target_compile_definitions(${TARGET_BINARY_FOR_SETUP} PRIVATE TAGLIB_STATIC)
