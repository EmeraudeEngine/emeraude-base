if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling meshoptimizer library ...")

# GLOBAL: emeraude-base links meshoptimizer and so does its consumer (the engine decodes
# EXT_meshopt_compression), each through this script. A non-GLOBAL imported target lives only in
# the directory that found it, so the engine would get a second meshoptimizer::meshoptimizer.
# CMake cannot merge two distinct targets naming the same archive: the library was emitted twice on
# the consumer's link line, which Apple's ld reports as "ignoring duplicate libraries". Made
# GLOBAL here, the consumer's find_package() reuses this target (the generated targets file returns
# early once its targets exist). Needs CMake 3.24, below the 3.25.1 minimum.
find_package(meshoptimizer CONFIG REQUIRED GLOBAL PATHS ${EMERAUDE_EXT_LIBS_PATH} NO_DEFAULT_PATH)

target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE meshoptimizer::meshoptimizer)
