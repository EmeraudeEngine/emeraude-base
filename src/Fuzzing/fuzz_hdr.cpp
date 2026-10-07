/*
 * fuzzing/fuzz_hdr.cpp
 * This file is part of Emeraude-Base
 *
 * libFuzzer target for the PixelFactory Radiance HDR (RGBE) parser (Ave robustus! — A.3).
 * Feeds arbitrary bytes through FileFormatHDR::readStream (hand-rolled header + adaptive RLE, flat and legacy RLE
 * scanlines) under ASan/UBSan. The engine reaches it on untrusted cubemap files (Graphics/CubemapResource.cpp).
 */

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <vector>

/* Local inclusions. */
#include "IO/MemoryStream.hpp"
#include "PixelFactory/FileFormatHDR.hpp"
#include "PixelFactory/Pixmap.hpp"

extern "C" int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
	using namespace EmEn::Base;

	/* ⚠️ A CONST buffer: bound to a non-const vector, MemoryStream opens for WRITING and the read path is never fuzzed. */
	const std::vector< std::byte > buffer{
		reinterpret_cast< const std::byte * >(data),
		reinterpret_cast< const std::byte * >(data) + size
	};

	IO::MemoryStream stream{buffer};
	PixelFactory::FileFormatHDR< float, uint32_t > format;
	PixelFactory::Pixmap< float, uint32_t > pixmap;

	(void)format.readStream(stream, pixmap);

	return 0;
}
