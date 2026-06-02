/*
 * fuzzing/fuzz_obj.cpp
 * This file is part of Emeraude-Base
 *
 * libFuzzer target for the VertexFactory OBJ parser (Ave robustus! — A.3).
 * Feeds arbitrary bytes through FileFormatOBJ::readStream under ASan/UBSan.
 */

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <vector>

/* Local inclusions. */
#include "IO/MemoryStream.hpp"
#include "VertexFactory/FileFormatOBJ.hpp"
#include "VertexFactory/ShapeLoadResult.hpp"

extern "C" int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
	using namespace EmEn::Base;

	const std::vector< std::byte > buffer{
		reinterpret_cast< const std::byte * >(data),
		reinterpret_cast< const std::byte * >(data) + size
	};

	IO::MemoryStream stream{buffer};
	VertexFactory::FileFormatOBJ< float, uint32_t > format;
	VertexFactory::ShapeLoadResult< float, uint32_t > result;

	(void)format.readStream(stream, result, {});

	return 0;
}