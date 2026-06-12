if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling bc7enc_rdo BC7 texture compression library ...")

find_package(bc7enc_rdo CONFIG REQUIRED PATHS ${EMERAUDE_EXT_LIBS_PATH} NO_DEFAULT_PATH)

# Headers are at ${EMERAUDE_EXT_LIBS_PATH}/include/bc7enc_rdo/, included via the imported target.
target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE bc7enc_rdo::bc7enc_rdo)
