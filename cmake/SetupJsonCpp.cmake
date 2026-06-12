if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling JsonCpp library from local precompiled source ...")

# NOTE: Headers are already included via ${EMERAUDE_EXT_LIBS_PATH}/include in the main CMakeLists.txt

target_compile_definitions(${TARGET_BINARY_FOR_SETUP} PUBLIC JSON_USE_EXCEPTION=Off)

if ( MSVC )
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PUBLIC
		debug "${EMERAUDE_EXT_LIBS_PATH}/lib/jsoncpp.lib"
		optimized "${EMERAUDE_EXT_LIBS_PATH}/lib/jsoncpp.lib"
	)
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PUBLIC "${EMERAUDE_EXT_LIBS_PATH}/lib/libjsoncpp.a")
endif ()
