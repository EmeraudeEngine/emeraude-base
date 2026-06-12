if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling HWLOC library ...")

if ( MSVC )
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ${EMERAUDE_EXT_LIBS_PATH}/lib/hwloc.lib)
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ${EMERAUDE_EXT_LIBS_PATH}/lib/libhwloc.a)

	if ( APPLE )
		target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE "-framework CoreFoundation -framework IOKit")
	endif ()
endif ()
