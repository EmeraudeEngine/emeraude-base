/*
 * fuzzing/fuzz_png.cpp
 * This file is part of Emeraude-Base
 *
 * libFuzzer target for the PixelFactory PNG parser (Ave robustus! — A.3).
 * Feeds arbitrary bytes through FileFormatPNG::readStream (libPNG + our ByteStream) under ASan/UBSan.
 */

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <vector>

/* Local inclusions. */
#include "IO/MemoryStream.hpp"
#include "PixelFactory/FileFormatPNG.hpp"
#include "PixelFactory/Pixmap.hpp"

extern "C" int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
	using namespace EmEn::Base;

	const std::vector< std::byte > buffer{
		reinterpret_cast< const std::byte * >(data),
		reinterpret_cast< const std::byte * >(data) + size
	};

	IO::MemoryStream stream{buffer};
	PixelFactory::FileFormatPNG< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	(void)format.readStream(stream, pixmap);

	return 0;
}