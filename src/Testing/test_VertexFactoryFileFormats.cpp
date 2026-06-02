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
#include <array>
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

		/* Lays out a structurally-consistent MD2 ("IDP2"): header + 1 texcoord + 1 triangle + `numFrames`
		 * frames, with the triangle's first vertex index forced to `triVertexIndex`. Reproduces the two
		 * fuzz_mdx MD2 crashes: an empty frame table (frames[0] on a null buffer) and an out-of-range
		 * triangle vertex index (OOB read). Counts stay small so exceedsStream() does not reject first. */
		std::vector< std::byte >
		makeMD2 (int32_t numVertices, int32_t numFrames, uint16_t triVertexIndex) noexcept
		{
			constexpr size_t headerSize = 68;
			constexpr size_t stSize = 4;    /* short s, short t */
			constexpr size_t triSize = 12;  /* 3 * uint16 vertex + 3 * uint16 st */

			const size_t safeVertices = numVertices < 0 ? 0 : static_cast< size_t >(numVertices);
			const size_t safeFrames = numFrames < 0 ? 0 : static_cast< size_t >(numFrames);
			const size_t frameSize = 40 + safeVertices * 4;  /* scale+translate+name(40) + verts */

			const size_t offsetST = headerSize;
			const size_t offsetTris = offsetST + stSize;
			const size_t offsetFrames = offsetTris + triSize;
			const size_t total = offsetFrames + safeFrames * frameSize + 16;

			std::vector< std::byte > buffer(total, std::byte{0});

			const auto putI32 = [&buffer] (size_t off, int32_t value) noexcept { std::memcpy(buffer.data() + off, &value, sizeof(value)); };

			std::memcpy(buffer.data(), "IDP2", 4);
			putI32(8, 1);                                       /* skinwidth (non-zero divisor) */
			putI32(12, 1);                                      /* skinheight */
			putI32(24, numVertices);                            /* num_vertices */
			putI32(28, 1);                                      /* num_st */
			putI32(32, 1);                                      /* num_tris */
			putI32(40, numFrames);                              /* num_frames */
			putI32(48, static_cast< int32_t >(offsetST));
			putI32(52, static_cast< int32_t >(offsetTris));
			putI32(56, static_cast< int32_t >(offsetFrames));

			/* Triangle: vertex[0] = triVertexIndex, everything else zero. */
			std::memcpy(buffer.data() + offsetTris, &triVertexIndex, sizeof(triVertexIndex));

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

	TEST(VertexFactoryMDx, md2EmptyFrameTableIsRejected)
	{
		/* num_frames = 0 with otherwise-consistent counts: the build read frames[0] on an empty
		 * vector (null data) -> SEGV (fuzz_mdx). Must cancel the load instead. */
		const auto buffer = makeMD2(/* numVertices */ 1, /* numFrames */ 0, /* triVertexIndex */ 0);

		MemoryStream stream{buffer};
		MDx format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	TEST(VertexFactoryMDx, md2OutOfRangeTriangleIndexIsRejected)
	{
		/* A triangle vertex index past num_vertices indexed frame.verts out of bounds (OOB read). */
		const auto buffer = makeMD2(/* numVertices */ 1, /* numFrames */ 1, /* triVertexIndex */ 0xFFFF);

		MemoryStream stream{buffer};
		MDx format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	TEST(VertexFactoryMDx, md3HostileSurfaceDoesNotCrash)
	{
		/* The exact 171-byte input fuzz_mdx reached after the MD2/MDL fixes: a structurally-plausible
		 * MD3 whose surface triangle indices point outside the surface vertex table -> OOB read / SEGV
		 * in loadMD3. The bounds check must cancel the load instead. */
		static constexpr std::array< uint8_t, 171 > md3{{
			0x49, 0x44, 0x50, 0x33, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0x6E, 0x74, 0x73, 0x20, 0x68, 0x7B, 0x64, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xF1, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x44, 0x50, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00
		}};

		std::vector< std::byte > buffer(md3.size());
		std::memcpy(buffer.data(), md3.data(), md3.size());

		MemoryStream stream{buffer};
		MDx format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	TEST(VertexFactoryMDx, md5HostileReferencesDoNotCrash)
	{
		/* 136-byte input fuzz_mdx found in the text MD5 loader: weight/joint/vertex cross-references
		 * pointing outside their tables -> null-deref / OOB. The validation pass must cancel the load. */
		static constexpr std::array< uint8_t, 136 > md5{{
			0xB0, 0x20, 0x39, 0x65, 0x72, 0x18, 0x00, 0x00, 0xB1, 0x00, 0xF9, 0x00,
			0x6E, 0x75, 0x6D, 0x4A, 0x6F, 0x69, 0x6E, 0x74, 0x73, 0x69, 0x75, 0x6D,
			0x4A, 0x6F, 0x69, 0x6E, 0x6F, 0x4D, 0x44, 0x35, 0x56, 0x65, 0x72, 0x73,
			0x69, 0x6F, 0x6E, 0x00, 0x00, 0x00, 0x5B, 0x00, 0xFF, 0x00, 0x0A, 0x49,
			0x44, 0xFE, 0xFF, 0x6E, 0x75, 0x6D, 0x4A, 0x6F, 0x69, 0x6E, 0x74, 0x73,
			0xB0, 0x4A, 0x6F, 0x69, 0x6E, 0x74, 0x73, 0xB0, 0xB0, 0x78, 0x6F, 0x6F,
			0x4D, 0x44, 0x36, 0x56, 0x65, 0x72, 0x73, 0x4D, 0x11, 0x35, 0x56, 0x65,
			0x72, 0x73, 0x69, 0x6F, 0x6E, 0x04, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x0A,
			0x49, 0x44, 0xFE, 0xFF, 0x6E, 0x75, 0x6D, 0x4A, 0x6F, 0x69, 0x6E, 0x74,
			0x73, 0xB0, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x44, 0xFE, 0x78,
			0xFF, 0x6E, 0x75, 0x6D, 0x4A, 0x6F, 0x69, 0x6E, 0x74, 0x73, 0x6E, 0x75,
			0x44, 0x30, 0x78, 0x50
		}};

		std::vector< std::byte > buffer(md5.size());
		std::memcpy(buffer.data(), md5.data(), md5.size());

		MemoryStream stream{buffer};
		MDx format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	TEST(VertexFactoryMDx, md3HugeTriangleTotalDoesNotOOM)
	{
		/* 175-byte input fuzz_mdx found next: a surface declaring a huge num_triangles made loadMD3
		 * reserveData() request ~64 GB before the per-triangle bounds check. The triangle total must
		 * be bounded against the stream size -> cancel the load. */
		static constexpr std::array< uint8_t, 175 > md3{{
			0x49, 0x44, 0x50, 0x33, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x2C,
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x44,
			0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		}};

		std::vector< std::byte > buffer(md3.size());
		std::memcpy(buffer.data(), md3.data(), md3.size());

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

	TEST(VertexFactoryOBJ, negativeRelativeIndicesResolveLikePositive)
	{
		/* Ave robustus! (A.4): OBJ negative indices are relative to the current list size.
		 * resolveIndex() now widens listSize to int64_t before the relative arithmetic
		 * (the former int32_t cast was UB once a list exceeded INT_MAX). With three
		 * vertices, "f -1 -2 -3" must resolve to the same triangle as "f 3 2 1". */
		const auto buffer = toBytes("v 0 0 0\nv 1 0 0\nv 0 1 0\nf -1 -2 -3\n");

		MemoryStream stream{buffer};
		OBJ format;
		Result result;
		ASSERT_TRUE(format.readStream(stream, result, {}));
		EXPECT_EQ(result.shape.triangles().size(), 1U);
	}
}
