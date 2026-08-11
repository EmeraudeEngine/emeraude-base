if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling libvpx library ...")

# libvpx encodes the MSVC runtime in the lib name (mt = /MT, md = /MD) and
# appends `d` for the Debug configuration (vpxmtd/vpxmdd), matching the
# Debug/Release packages produced by ext-deps-generator.
if ( MSVC )
	if ( EMERAUDE_USE_STATIC_RUNTIME )
		target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE
			debug "${EMERAUDE_EXT_LIBS_PATH}/lib/vpxmtd.lib"
			optimized "${EMERAUDE_EXT_LIBS_PATH}/lib/vpxmt.lib"
		)
	else ()
		target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE
			debug "${EMERAUDE_EXT_LIBS_PATH}/lib/vpxmdd.lib"
			optimized "${EMERAUDE_EXT_LIBS_PATH}/lib/vpxmd.lib"
		)
	endif ()
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ${EMERAUDE_EXT_LIBS_PATH}/lib/libvpx.a)
endif ()
