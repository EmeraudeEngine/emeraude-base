if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling LibJPEG-turbo library ...")

if ( MSVC )
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PUBLIC jpeg-static)
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE jpeg)
endif ()
