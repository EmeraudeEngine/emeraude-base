/*
 * fuzzing/fuzz_mdx.cpp
 * This file is part of Emeraude-Base
 *
 * libFuzzer target for the VertexFactory MDx (Quake MDL/MD2/MD3) parser (Ave robustus! — A.3).
 * Feeds arbitrary bytes through FileFormatMDx::readStream under ASan/UBSan. Read-only legacy
 * formats with untrusted header counts — must never crash, only cancel the load.
 */

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <vector>

/* Local inclusions. */
#include "IO/MemoryStream.hpp"
#include "VertexFactory/FileFormatMDx.hpp"
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
	VertexFactory::FileFormatMDx< float, uint32_t > format;
	VertexFactory::ShapeLoadResult< float, uint32_t > result;

	(void)format.readStream(stream, result, {});

	return 0;
}