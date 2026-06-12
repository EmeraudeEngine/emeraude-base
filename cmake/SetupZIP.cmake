if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling LibZib library ...")

target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE zip)
