if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling simdjson JSON parsing library ...")

find_package(simdjson CONFIG REQUIRED PATHS ${EMERAUDE_EXT_LIBS_PATH} NO_DEFAULT_PATH)

# Header is at ${EMERAUDE_EXT_LIBS_PATH}/include/simdjson.h, reachable through the imported target.
# NOTE: link this AFTER SetupFastGLTF. fastgltf is linked as a raw archive path, so CMake cannot see
# that it needs simdjson symbols; the static link order is the order of these include() calls.
target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE simdjson::simdjson)
