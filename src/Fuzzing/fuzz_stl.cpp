/*
 * fuzzing/fuzz_stl.cpp
 * This file is part of Emeraude-Base
 *
 * libFuzzer target for the VertexFactory STL parser (Ave robustus! — A.3).
 * Feeds arbitrary bytes through FileFormatSTL::readStream (binary + ASCII STL) under ASan/UBSan, as read and welded
 * (ReadOptions::weldVertices with requestNormal, 2026-11-26: the welder's table and the smooth normals).
 */

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <vector>

/* Local inclusions. */
#include "IO/MemoryStream.hpp"
#include "VertexFactory/FileFormatSTL.hpp"
#include "VertexFactory/ShapeLoadResult.hpp"

extern "C" int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
	using namespace EmEn::Base;

	const std::vector< std::byte > buffer{
		reinterpret_cast< const std::byte * >(data),
		reinterpret_cast< const std::byte * >(data) + size
	};

	VertexFactory::FileFormatSTL< float, uint32_t > format;

	{
		IO::MemoryStream stream{buffer};
		VertexFactory::ShapeLoadResult< float, uint32_t > result;

		(void)format.readStream(stream, result, {});
	}

	{
		VertexFactory::ReadOptions options{};
		options.weldVertices = true;
		options.requestNormal = true;

		IO::MemoryStream stream{buffer};
		VertexFactory::ShapeLoadResult< float, uint32_t > result;

		(void)format.readStream(stream, result, options);
	}

	return 0;
}
