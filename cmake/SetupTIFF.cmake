if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling LibTIFF library ...")

# TIFF is not an exotic format here: Intel's Jungle Ruins stores the base colour and the
# translucency of its WHOLE vegetation as 16-bit TIFF. Without this codec those materials fall
# back to a flat colour and the plants render pure white.
#
# ⚠️ NO DEBUG POSTFIX. Unlike libpng — which ships `libpng16d.a` in Debug — libtiff sets no
# `DEBUG_POSTFIX`, so the archive is `libtiff.a` in BOTH configurations. Naming a `libtiffd.a`
# here breaks the Debug build with "no rule to make target", and Release stays perfectly green,
# so the mistake hides until someone actually builds Debug.
#
# NOTE: libtiff pulls its own compression back-ends (zlib, jpeg, xz, zstd) which the cascade
# already links; only the TIFF archive is added here.
if ( MSVC )
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PUBLIC "${EMERAUDE_EXT_LIBS_PATH}/lib/tiff.lib")
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE "${EMERAUDE_EXT_LIBS_PATH}/lib/libtiff.a")
endif ()
