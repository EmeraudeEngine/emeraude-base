if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling tinyusdz (LightUSD) OpenUSD parsing library ...")

# Upstream rebranded tinyusdz to LightUSD at v1.0.0-rc4 (ext-deps-generator archive v018):
# package `lightusd`, target `lightusd::lightusd_static`, header `lightusd.hh`, namespace
# `lightusd` — with no `tinyusdz` alias, so the engine code switched in the same move.
find_package(lightusd CONFIG REQUIRED PATHS ${EMERAUDE_EXT_LIBS_PATH} NO_DEFAULT_PATH)

# Headers live at ${EMERAUDE_EXT_LIBS_PATH}/include/lightusd/ with their source tree layout
# preserved — they include one another by relative path, so BOTH that directory and its
# external/ subdirectory are carried by the imported target's interface.
#
# NOTE: built with LIGHTUSD_CXX_EXCEPTIONS=Off (and LIGHTUSD_CXX_RTTI=Off), which upstream supports natively on POSIX
# (its own default there) and which the cascade requires (-fno-exceptions). On MSVC upstream
# warns that disabling exceptions is hard — the option is forced Off anyway, so a Windows
# build is where that assumption gets verified.
target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE lightusd::lightusd_static)