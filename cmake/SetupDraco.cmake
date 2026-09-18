if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling Draco library ...")

# Google Draco, the geometry codec behind KHR_draco_mesh_compression. Only the DECODER is
# reached at runtime; the encoder ships in the same archive and the linker prunes it.
#
# ⚠️ Draco installs NO CMake config package — `lib/cmake/` holds nothing for it, only
# `lib/pkgconfig/draco.pc` exists. `find_package(draco CONFIG)` therefore CANNOT work here,
# unlike meshoptimizer/fastgltf's neighbours, hence the direct archive reference below.
# Verified against the ext-deps-generator output for 1.5.7.
#
# @note Nothing to do for symbol hiding: emeraude_base_target_hide_third_party_exports()
# globs `${EMERAUDE_EXT_LIBS_LIB_DIR}/*.a`, so libdraco.a is covered without being named.
if ( MSVC )
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE
		debug "${EMERAUDE_EXT_LIBS_PATH}/lib/draco.lib"
		optimized "${EMERAUDE_EXT_LIBS_PATH}/lib/draco.lib"
	)
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE "${EMERAUDE_EXT_LIBS_PATH}/lib/libdraco.a")
endif ()
