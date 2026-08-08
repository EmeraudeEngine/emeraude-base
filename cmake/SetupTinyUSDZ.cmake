if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling tinyusdz OpenUSD parsing library ...")

find_package(tinyusdz CONFIG REQUIRED PATHS ${EMERAUDE_EXT_LIBS_PATH} NO_DEFAULT_PATH)

# Headers live at ${EMERAUDE_EXT_LIBS_PATH}/include/tinyusdz/ with their source tree layout
# preserved — they include one another by relative path, so BOTH that directory and its
# external/ subdirectory are carried by the imported target's interface.
#
# NOTE: built with TINYUSDZ_CXX_EXCEPTIONS=Off, which upstream supports natively on POSIX
# (its own default there) and which the cascade requires (-fno-exceptions). On MSVC upstream
# warns that disabling exceptions is hard — the option is forced Off anyway, so a Windows
# build is where that assumption gets verified.
target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE tinyusdz::tinyusdz_static)