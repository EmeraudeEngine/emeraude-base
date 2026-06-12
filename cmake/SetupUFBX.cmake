if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling ufbx FBX parsing library ...")

find_package(ufbx CONFIG REQUIRED PATHS ${EMERAUDE_EXT_LIBS_PATH} NO_DEFAULT_PATH)

# Headers are at ${EMERAUDE_EXT_LIBS_PATH}/include/ufbx/, included via the imported target.
target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE ufbx::ufbx)
