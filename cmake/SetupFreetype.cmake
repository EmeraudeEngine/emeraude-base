if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling FreeType library ...")

target_include_directories(${TARGET_BINARY_FOR_SETUP} SYSTEM PUBLIC "${EMERAUDE_EXT_LIBS_PATH}/include/freetype2")

target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE
	debug freetyped
	optimized freetype
)

if ( UNIX AND NOT APPLE )
	message("Enabling Fontconfig (system) ...")

	find_package(Fontconfig REQUIRED)

	target_include_directories(${TARGET_BINARY_FOR_SETUP} SYSTEM PUBLIC ${Fontconfig_INCLUDE_DIRS})

	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ${Fontconfig_LIBRARIES})
	#target_compile_options(${TARGET_BINARY_FOR_SETUP} PUBLIC ${Fontconfig_COMPILE_OPTIONS})
endif ()
