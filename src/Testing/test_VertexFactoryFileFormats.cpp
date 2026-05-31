/*
 * src/Testing/test_VertexFactoryFileFormats.cpp
 * This file is part of Emeraude-Base
 *
 * Copyright (C) 2010-2026 - Sébastien Léon Claude Christian Bémelmans "LondNoir" <londnoir@gmail.com>
 *
 * Emeraude-Base is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * Emeraude-Base is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Emeraude-Base; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Complete project and additional information can be found at :
 * https://github.com/EmeraudeEngine/emeraude-base
 *
 * --- THIS IS AUTOMATICALLY GENERATED, DO NOT CHANGE ---
 */

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* STL inclusions. */
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

/* Local inclusions. */
#include "IO/MemoryStream.hpp"
#include "VertexFactory/FileFormatMDx.hpp"
#include "VertexFactory/FileFormatNative.hpp"
#include "VertexFactory/FileFormatOBJ.hpp"
#include "VertexFactory/FileFormatSTL.hpp"
#include "VertexFactory/ShapeLoadResult.hpp"
#include "VertexFactory/ShapeTriangle.hpp"
#include "VertexFactory/ShapeVertex.hpp"

namespace EmEn::Base::VertexFactory
{
	namespace
	{
		using IO::MemoryStream;
		using Native = FileFormatNative< float, uint32_t >;
		using Result = ShapeLoadResult< float, uint32_t >;

		constexpr uint64_t HeaderBytes = 56;   /* 32-byte header + 3 * uint64 counts. */

		void
		putU16 (std::vector< std::byte > & buffer, size_t offset, uint16_t value) noexcept
		{
			std::memcpy(buffer.data() + offset, &value, sizeof(value));
		}

		void
		putU64 (std::vector< std::byte > & buffer, size_t offset, uint64_t value) noexcept
		{
			std::memcpy(buffer.data() + offset, &value, sizeof(value));
		}

		/* Builds a well-formed EE3D_V1 header (float/uint32 precision) followed by `blobBytes`
		 * of zeroed payload, with the three counts set as requested. */
		std::vector< std::byte >
		makeNative (uint64_t vertexCount, uint64_t triangleCount, uint64_t colorCount, size_t blobBytes) noexcept
		{
			std::vector< std::byte > buffer(HeaderBytes + blobBytes, std::byte{0});

			std::memcpy(buffer.data(), "EE3D_V1", 7);
			putU16(buffer, 8, 1);                                       /* version */
			buffer[10] = static_cast< std::byte >(sizeof(float));       /* vertex precision */
			buffer[11] = static_cast< std::byte >(sizeof(uint32_t));    /* index precision */
			putU64(buffer, 32, vertexCount);
			putU64(buffer, 40, triangleCount);
			putU64(buffer, 48, colorCount);

			return buffer;
		}
	}

	/* ===== Nominal ===== */

	TEST(VertexFactoryNative, wellFormedReadsBack)
	{
		const size_t blob = (3 * sizeof(ShapeVertex< float >)) + (1 * sizeof(ShapeTriangle< float, uint32_t >));
		const auto buffer = makeNative(3, 1, 0, blob);

		MemoryStream stream{buffer};
		Native format;
		Result result;
		ASSERT_TRUE(format.readStream(stream, result, {}));

		EXPECT_EQ(result.shape.vertices().size(), 3U);
		EXPECT_EQ(result.shape.triangles().size(), 1U);
		EXPECT_EQ(result.shape.vertexColors().size(), 0U);
	}

	/* ===== Malformed / hostile (run under ASan/UBSan: must reject, never crash) ===== */

	TEST(VertexFactoryNative, hugeVertexCountIsRejectedNotAllocated)
	{
		/* The headline bug: an unvalidated 64-bit count fed straight to resize() would request
		 * a multi-exabyte allocation -> std::length_error -> std::terminate under -fno-exceptions.
		 * The reader must reject it from the (tiny) actual stream size instead. */
		const auto buffer = makeNative(std::numeric_limits< uint64_t >::max(), 0, 0, 0);

		MemoryStream stream{buffer};
		Native format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	TEST(VertexFactoryNative, hugeTriangleAndColorCountsAreRejected)
	{
		{
			const auto buffer = makeNative(0, std::numeric_limits< uint64_t >::max(), 0, 0);
			MemoryStream stream{buffer};
			Native format;
			Result result;
			EXPECT_FALSE(format.readStream(stream, result, {}));
		}
		{
			const auto buffer = makeNative(0, 0, std::numeric_limits< uint64_t >::max(), 0);
			MemoryStream stream{buffer};
			Native format;
			Result result;
			EXPECT_FALSE(format.readStream(stream, result, {}));
		}
	}

	TEST(VertexFactoryNative, truncatedPayloadIsRejected)
	{
		/* Counts claim 3 vertices but no payload bytes follow. */
		const auto buffer = makeNative(3, 0, 0, 0);

		MemoryStream stream{buffer};
		Native format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	TEST(VertexFactoryNative, badMagicIsRejected)
	{
		auto buffer = makeNative(0, 0, 0, 0);
		buffer[0] = static_cast< std::byte >('X');

		MemoryStream stream{buffer};
		Native format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	TEST(VertexFactoryNative, shortHeaderIsRejected)
	{
		const std::vector< std::byte > buffer(10, std::byte{0});   /* < 32-byte header */

		MemoryStream stream{buffer};
		Native format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	/* ===== Binary STL ===== */

	namespace
	{
		using STL = FileFormatSTL< float, uint32_t >;

		constexpr size_t STLHeaderBytes = 84;       /* 80-byte header + uint32 triangle count. */
		constexpr size_t STLTriangleBytes = 50;     /* 12 normal + 36 vertices + 2 attribute. */

		/* A binary STL whose 80-byte header is all-zero (so it is NOT detected as ASCII),
		 * the triangle count set as requested, followed by `payloadTriangles` zeroed triangles. */
		std::vector< std::byte >
		makeBinarySTL (uint32_t triangleCount, size_t payloadTriangles) noexcept
		{
			std::vector< std::byte > buffer(STLHeaderBytes + (payloadTriangles * STLTriangleBytes), std::byte{0});
			std::memcpy(buffer.data() + 80, &triangleCount, sizeof(triangleCount));

			return buffer;
		}
	}

	TEST(VertexFactorySTL, wellFormedBinaryReadsBack)
	{
		const auto buffer = makeBinarySTL(1, 1);

		MemoryStream stream{buffer};
		STL format;
		Result result;
		ASSERT_TRUE(format.readStream(stream, result, {}));

		EXPECT_EQ(result.shape.triangles().size(), 1U);
		EXPECT_EQ(result.shape.vertices().size(), 3U);
	}

	TEST(VertexFactorySTL, hugeTriangleCountIsRejectedNotAllocated)
	{
		/* Count claims ~4 billion triangles (would reserve ~12 billion vertices) but the
		 * stream carries none. Must be rejected from the actual stream size, never reserved. */
		const auto buffer = makeBinarySTL(std::numeric_limits< uint32_t >::max(), 0);

		MemoryStream stream{buffer};
		STL format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	TEST(VertexFactorySTL, countLargerThanPayloadIsRejected)
	{
		/* Claims 10 triangles, provides 2. */
		const auto buffer = makeBinarySTL(10, 2);

		MemoryStream stream{buffer};
		STL format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	/* ===== Legacy id Tech formats (MDL/MD2/MD3) — read-only; goal is "never crash the engine,
	 * cancel the load on bad input". Run under ASan/UBSan. ===== */

	namespace
	{
		using MDx = FileFormatMDx< float, uint32_t >;

		/* A bare 84-byte MDL header ("IDPO"), with num_skins (offset 48) set as requested. */
		std::vector< std::byte >
		makeMDL (int32_t numSkins) noexcept
		{
			std::vector< std::byte > buffer(84, std::byte{0});
			buffer[0] = static_cast< std::byte >('I');
			buffer[1] = static_cast< std::byte >('D');
			buffer[2] = static_cast< std::byte >('P');
			buffer[3] = static_cast< std::byte >('O');
			std::memcpy(buffer.data() + 48, &numSkins, sizeof(numSkins));

			return buffer;
		}

		/* A recognised magic ("IDP" + variant) followed by 0xFF garbage — every header count
		 * decodes to a huge value and must be rejected, never allocated. */
		std::vector< std::byte >
		makeGarbageWithMagic (char variant) noexcept
		{
			std::vector< std::byte > buffer(256, std::byte{0xFF});
			buffer[0] = static_cast< std::byte >('I');
			buffer[1] = static_cast< std::byte >('D');
			buffer[2] = static_cast< std::byte >('P');
			buffer[3] = static_cast< std::byte >(variant);

			return buffer;
		}
	}

	TEST(VertexFactoryMDx, mdlHugeSkinCountIsRejectedNotAllocated)
	{
		const auto buffer = makeMDL(std::numeric_limits< int32_t >::max());

		MemoryStream stream{buffer};
		MDx format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	TEST(VertexFactoryMDx, garbageMD2DoesNotCrash)
	{
		const auto buffer = makeGarbageWithMagic('2');

		MemoryStream stream{buffer};
		MDx format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	TEST(VertexFactoryMDx, garbageMD3DoesNotCrash)
	{
		const auto buffer = makeGarbageWithMagic('3');

		MemoryStream stream{buffer};
		MDx format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	TEST(VertexFactoryMDx, writeIsRejectedReadOnly)
	{
		/* MDx is read-only: writeStream must fail cleanly, never assert. */
		std::vector< std::byte > sink;
		MemoryStream stream{sink};
		MDx format;
		Shape< float, uint32_t > shape;
		EXPECT_FALSE(format.writeStream(stream, shape, {}));
	}

	/* ===== Wavefront OBJ (text) — counts are line-derived (no allocation DoS); the crash risk
	 * is a face index referencing a non-existent vertex: m_v.at(idx) would throw out_of_range
	 * -> std::terminate under -fno-exceptions. Bad index must cancel the load, never crash. ===== */

	namespace
	{
		using OBJ = FileFormatOBJ< float, uint32_t >;

		std::vector< std::byte >
		toBytes (std::string_view text) noexcept
		{
			std::vector< std::byte > buffer(text.size());

			if ( !text.empty() )
			{
				std::memcpy(buffer.data(), text.data(), text.size());
			}

			return buffer;
		}
	}

	TEST(VertexFactoryOBJ, wellFormedReadsBack)
	{
		const auto buffer = toBytes("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");

		MemoryStream stream{buffer};
		OBJ format;
		Result result;
		ASSERT_TRUE(format.readStream(stream, result, {}));
		EXPECT_EQ(result.shape.triangles().size(), 1U);
	}

	TEST(VertexFactoryOBJ, outOfRangeVertexIndexIsRejected)
	{
		/* 3 vertices, face references the 999th -> must cancel, not terminate. */
		const auto buffer = toBytes("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 999\n");

		MemoryStream stream{buffer};
		OBJ format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	TEST(VertexFactoryOBJ, outOfRangeIndexWithFullAttributesIsRejected)
	{
		/* Exercises the V/VT/VN assembly path with an out-of-range position index. */
		const auto buffer = toBytes(
			"v 0 0 0\nv 1 0 0\nv 0 1 0\n"
			"vt 0 0\n"
			"vn 0 0 1\n"
			"f 1/1/1 2/1/1 999/1/1\n");

		MemoryStream stream{buffer};
		OBJ format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}
}
