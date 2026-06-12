if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling LZMA library ...")

target_compile_definitions(${TARGET_BINARY_FOR_SETUP} PUBLIC LZMA_API_STATIC)

target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE lzma)
