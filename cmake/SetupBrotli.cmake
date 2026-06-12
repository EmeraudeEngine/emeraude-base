if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling brotli library ...")

target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE
	brotlidec
	brotlienc
	brotlicommon
)
