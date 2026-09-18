if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling WebP library ...")

# libwebp, the codec behind PixelFactory::FileFormatWebP and glTF's EXT_texture_webp.
#
# ⚠️ `libwebp.a` and NOT `libwebpdecoder.a`: the decoder-only archive would link, and then
# FileFormatWebP::writeStream() would fail at LINK time on WebPEncodeLossless*. The demux and mux
# archives are for container/animation features this codec does not use.
#
# ⚠️⚠️ `libsharpyuv.a` IS REQUIRED, and it is a separate archive. libwebp's ENCODER calls
# SharpYuvInit / SharpYuvConvert / SharpYuvGetConversionMatrix, which upstream moved out of libwebp
# into its own library. Linking libwebp alone builds every object file fine and then fails at LINK
# with those three symbols undefined -- the decoder half never touches them, so a decode-only test
# would not catch it. It must come AFTER libwebp: a static archive only resolves symbols demanded by
# what precedes it on the command line.
if ( MSVC )
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE
		debug "${EMERAUDE_EXT_LIBS_PATH}/lib/libwebp.lib"
		optimized "${EMERAUDE_EXT_LIBS_PATH}/lib/libwebp.lib"
		debug "${EMERAUDE_EXT_LIBS_PATH}/lib/libsharpyuv.lib"
		optimized "${EMERAUDE_EXT_LIBS_PATH}/lib/libsharpyuv.lib"
	)
else ()
	target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE
		"${EMERAUDE_EXT_LIBS_PATH}/lib/libwebp.a"
		"${EMERAUDE_EXT_LIBS_PATH}/lib/libsharpyuv.a"
	)
endif ()
