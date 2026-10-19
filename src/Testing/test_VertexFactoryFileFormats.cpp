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
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "IO/MemoryStream.hpp"
#include "VertexFactory/FileIO.hpp"
#include "VertexFactory/FileFormatMDx.hpp"
#include "VertexFactory/FileFormatNative.hpp"
#include "VertexFactory/FileFormatOBJ.hpp"
#include "VertexFactory/FileFormatSTL.hpp"
#include "VertexFactory/ShapeGenerator.hpp"
#include "VertexFactory/ShapeLoadResult.hpp"
#include "VertexFactory/StreamIO.hpp"
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

		/* Builds a well-formed native header (float/uint32 precision) followed by `blobBytes`
		 * of zeroed payload, with the three counts set as requested.
		 * ⚠️ The magic string stayed "EE3D_V1" while the VERSION FIELD moved to 2, then 3 — the magic is
		 * only checked for identity, the uint16 at offset 8 is what gates the layout. The payload
		 * size is taken from sizeof(ShapeVertex) by the callers, so it follows the structure. */
		std::vector< std::byte >
		makeNative (uint64_t vertexCount, uint64_t triangleCount, uint64_t colorCount, size_t blobBytes, uint16_t version = 3) noexcept
		{
			std::vector< std::byte > buffer(HeaderBytes + blobBytes, std::byte{0});

			std::memcpy(buffer.data(), "EE3D_V1", 7);
			putU16(buffer, 8, version);
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

	/* Version 3 (2026-10-03) carries the secondary texture coordinates, last in each vertex record. */
	TEST(VertexFactoryNative, version3KeepsTheSecondaryTextureCoordinates)
	{
		ShapeVertex< float > vertex{Math::Vector< 3, float >{1.0F, 2.0F, 3.0F}};

		vertex.setSecondaryTextureCoordinates(Math::Vector< 2, float >{0.25F, 0.75F});

		auto buffer = makeNative(1, 0, 0, sizeof(ShapeVertex< float >), 3);

		std::memcpy(buffer.data() + HeaderBytes, &vertex, sizeof(ShapeVertex< float >));

		/* ⚠️ A CONST buffer: bound to a non-const vector, MemoryStream opens for WRITING. */
		MemoryStream stream{std::as_const(buffer)};
		Native format;
		Result result;
		ASSERT_TRUE(format.readStream(stream, result, {}));
		ASSERT_EQ(result.shape.vertices().size(), 1U);

		EXPECT_EQ(result.shape.vertices()[0].position()[Math::Y], 2.0F);
		EXPECT_EQ(result.shape.vertices()[0].secondaryTextureCoordinates()[Math::X], 0.25F);
		EXPECT_EQ(result.shape.vertices()[0].secondaryTextureCoordinates()[Math::Y], 0.75F);
	}

	/* A version-2 record (84 bytes) is the exact prefix of a version-3 one: it still reads, its secondary set at
	 * (0, 0), and its other attributes intact. Read at the version-3 stride it would misparse. */
	TEST(VertexFactoryNative, version2StillReadsWithoutSecondaryTextureCoordinates)
	{
		constexpr size_t VersionTwoVertexBytes = 84;

		ShapeVertex< float > first{Math::Vector< 3, float >{1.0F, 2.0F, 3.0F}};
		ShapeVertex< float > second{Math::Vector< 3, float >{4.0F, 5.0F, 6.0F}};

		first.setSecondaryTextureCoordinates(Math::Vector< 2, float >{0.5F, 0.5F});
		second.setSecondaryTextureCoordinates(Math::Vector< 2, float >{0.5F, 0.5F});

		auto buffer = makeNative(2, 0, 0, 2 * VersionTwoVertexBytes, 2);

		std::memcpy(buffer.data() + HeaderBytes, &first, VersionTwoVertexBytes);
		std::memcpy(buffer.data() + HeaderBytes + VersionTwoVertexBytes, &second, VersionTwoVertexBytes);

		/* ⚠️ A CONST buffer: bound to a non-const vector, MemoryStream opens for WRITING. */
		MemoryStream stream{std::as_const(buffer)};
		Native format;
		Result result;
		ASSERT_TRUE(format.readStream(stream, result, {}));
		ASSERT_EQ(result.shape.vertices().size(), 2U);

		EXPECT_EQ(result.shape.vertices()[1].position()[Math::X], 4.0F);
		EXPECT_EQ(result.shape.vertices()[1].position()[Math::Z], 6.0F);
		EXPECT_EQ(result.shape.vertices()[1].secondaryTextureCoordinates()[Math::X], 0.0F);
		EXPECT_EQ(result.shape.vertices()[1].secondaryTextureCoordinates()[Math::Y], 0.0F);
	}

	/* A version-1 file predates the tangent handedness: its vertices are 80 bytes where this
	 * build expects 84. There is no read path for it and there must not be one — reading a v1
	 * blob at the v2 stride misparses SILENTLY, because the count validation can pass on a wrong
	 * stride. The rejection is the feature. */
	TEST(VertexFactoryNative, version1IsRejectedNotMisparsed)
	{
		const size_t blob = (3 * sizeof(ShapeVertex< float >)) + (1 * sizeof(ShapeTriangle< float, uint32_t >));
		const auto buffer = makeNative(3, 1, 0, blob, 1);

		MemoryStream stream{buffer};
		Native format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
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

	/* The triangles are a raw blob: their vertex and color indices come from the FILE (the coverage pass,
	 * 2026-10-17). One pointing past the vertex table was accepted, and the first consumer read out of bounds. */
	TEST(VertexFactoryNative, aTriangleIndexOutOfItsTableIsRefused)
	{
		const auto build = [] (uint32_t vertexIndex, uint32_t colorIndex, uint64_t colorCount) {
			const size_t vertexBytes = 3 * sizeof(ShapeVertex< float >);
			const size_t triangleBytes = sizeof(ShapeTriangle< float, uint32_t >);
			auto buffer = makeNative(3, 1, colorCount, vertexBytes + triangleBytes + (colorCount * sizeof(Math::Vector< 4, float >)));

			ShapeTriangle< float, uint32_t > triangle{0, 1, vertexIndex};
			triangle.setVertexColorIndex(0, colorIndex);
			std::memcpy(buffer.data() + HeaderBytes + vertexBytes, static_cast< const void * >(&triangle), triangleBytes);

			return buffer;
		};

		{
			const auto buffer = build(2, 0, 0);
			Result result;
			ASSERT_TRUE(StreamIO::read(buffer, FileFormatType::Native, result)) << "the control: every index in its table";
			EXPECT_TRUE(result.shape.computeTriangleNormal());
		}

		{
			const auto buffer = build(999, 0, 0);
			Result result;
			EXPECT_FALSE(StreamIO::read(buffer, FileFormatType::Native, result)) << "a vertex index past the table";

			if ( !result.shape.triangles().empty() )
			{
				static_cast< void >(result.shape.computeTriangleNormal());
			}
		}

		{
			const auto buffer = build(2, 7, 2);
			Result result;
			EXPECT_FALSE(StreamIO::read(buffer, FileFormatType::Native, result)) << "a color index past the color table";
		}
	}

	TEST(VertexFactoryNative, everyTruncationOfARealFileIsRefused)
	{
		/* The coverage pass (2026-10-18): a real file, cut at EVERY length, is refused at every stage (header, metadata,
		 * counts, vertices, triangles, colors) — never read short, never past its end (ASan). */
		const ShapeBuilderOptions< float > options{true, true, true, false, false};
		const auto cube = ShapeGenerator::generateCuboid< float >(1.0F, 1.0F, 1.0F, options);
		ASSERT_TRUE(cube.isValid());

		std::vector< std::byte > written;
		{
			MemoryStream output{written};
			ASSERT_TRUE(Native{}.writeStream(output, cube, {}));
		}

		{
			const auto & whole = written;
			MemoryStream input{whole};
			Result result;
			ASSERT_TRUE(Native{}.readStream(input, result, {}));
			EXPECT_EQ(result.shape.vertices().size(), cube.vertices().size());
			EXPECT_EQ(result.shape.vertexColors().size(), cube.vertexColors().size());
		}

		size_t refused = 0;

		for ( size_t length = 0; length < written.size(); ++length )
		{
			const std::vector< std::byte > truncated(written.cbegin(), written.cbegin() + static_cast< std::ptrdiff_t >(length));
			MemoryStream input{truncated};
			Result result;

			if ( !Native{}.readStream(input, result, {}) )
			{
				++refused;
			}
			else
			{
				ADD_FAILURE() << "a file cut at " << length << " of " << written.size() << " bytes was accepted";
			}
		}

		EXPECT_EQ(refused, written.size());
	}

	TEST(VertexFactoryNative, aPrecisionMismatchIsRefused)
	{
		auto buffer = makeNative(1, 0, 0, sizeof(ShapeVertex< float >));
		buffer[10] = static_cast< std::byte >(sizeof(double));

		const auto & input = buffer;
		MemoryStream stream{input};
		Result result;
		EXPECT_FALSE(Native{}.readStream(stream, result, {}));
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

	/* 2026-10-19 (cppcheck invalidPointerCast): the binary records go through a 50-byte buffer, no float pointer cast;
	 * the writer checks every write (it returned true on a stream that refused them). */
	TEST(VertexFactorySTL, binaryWriteReadsBackAndRefusesAFailingStream)
	{
		const auto cube = ShapeGenerator::generateCuboid< float, uint32_t >(1.0F, 2.0F, 3.0F);

		ASSERT_TRUE(cube.isValid());

		std::vector< std::byte > buffer;

		{
			MemoryStream output{buffer};
			const STL format;

			ASSERT_TRUE(format.writeStream(output, cube, {}));
		}

		ASSERT_EQ(buffer.size(), STLHeaderBytes + (cube.triangles().size() * STLTriangleBytes));

		MemoryStream input{std::as_const(buffer)};
		STL format;
		Result result;

		ASSERT_TRUE(format.readStream(input, result, {}));
		ASSERT_EQ(result.shape.triangles().size(), cube.triangles().size());

		for ( size_t triangleIndex = 0; triangleIndex < cube.triangles().size(); ++triangleIndex )
		{
			for ( uint32_t corner = 0; corner < 3; ++corner )
			{
				const auto & expected = cube.vertex(cube.triangles()[triangleIndex].vertexIndex(corner)).position();
				const auto & actual = result.shape.vertex(result.shape.triangles()[triangleIndex].vertexIndex(corner)).position();

				EXPECT_EQ(actual, expected) << "triangle " << triangleIndex << " corner " << corner;
			}
		}

		/* A read-only stream refuses the writes: the writer must say so. */
		const std::vector< std::byte > readOnly;
		MemoryStream refusing{readOnly};

		EXPECT_FALSE(format.writeStream(refusing, cube, {}));
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

	/* ===== ASCII STL — a malformed or non-finite triplet refuses the file (owner, 2026-10-08) ===== */

	namespace
	{
		/* One facet whose normal and vertex lines are given; CRLF line ends, like a Windows exporter. */
		std::vector< std::byte >
		makeAsciiSTL (const std::string & normalLine, const std::string & vertexLine) noexcept
		{
			const std::string text =
				"solid test\r\n"
				"  facet normal " + normalLine + "\r\n"
				"    outer loop\r\n"
				"      vertex " + vertexLine + "\r\n"
				"      vertex 1 0 0\r\n"
				"      vertex 0 1 0\r\n"
				"    endloop\r\n"
				"  endfacet\r\n"
				"endsolid test\r\n";

			std::vector< std::byte > buffer(text.size());
			std::memcpy(buffer.data(), text.data(), text.size());

			return buffer;
		}

		[[nodiscard]]
		bool
		readAsciiSTL (const std::string & normalLine, const std::string & vertexLine, Result & result) noexcept
		{
			const auto buffer = makeAsciiSTL(normalLine, vertexLine);

			MemoryStream stream{buffer};
			STL format;

			return format.readStream(stream, result, {});
		}
	}

	TEST(VertexFactorySTL, wellFormedAsciiReadsBack)
	{
		Result result;
		ASSERT_TRUE(readAsciiSTL("0 0 1", "0 0 0", result));

		EXPECT_EQ(result.shape.triangles().size(), 1U);
		EXPECT_EQ(result.shape.vertices().size(), 3U);
	}

	TEST(VertexFactorySTL, asciiAcceptsSignsExponentsAndBlanks)
	{
		/* A leading '+' (accepted by the former std::sscanf()), exponents, tabs and several blanks. */
		Result result;
		ASSERT_TRUE(readAsciiSTL("+0.0e+00\t0.000000e+00   1.0E0", "-1.5e-3 +2 \t 3.25  ", result));

		EXPECT_EQ(result.shape.triangles().size(), 1U);
	}

	TEST(VertexFactorySTL, asciiMalformedVertexRefusesTheFile)
	{
		/* Each one used to load as a (0, 0, 0) vertex, silently. */
		for ( const auto * const vertexLine : {"1 2", "", "1 2 x", "a b c", "1 2 3 garbage", "1,0 2,0 3,0", "++1 2 3"} )
		{
			Result result;
			EXPECT_FALSE(readAsciiSTL("0 0 1", vertexLine, result)) << "vertex '" << vertexLine << "'";
		}
	}

	TEST(VertexFactorySTL, asciiNonFiniteValueRefusesTheFile)
	{
		for ( const auto * const vertexLine : {"nan 0 0", "0 inf 0", "0 0 -inf", "1e40 0 0"} )
		{
			Result result;
			EXPECT_FALSE(readAsciiSTL("0 0 1", vertexLine, result)) << "vertex '" << vertexLine << "'";
		}
	}

	TEST(VertexFactorySTL, asciiMalformedNormalRefusesTheFile)
	{
		for ( const auto * const normalLine : {"0 0", "0 0 nan", "x y z"} )
		{
			Result result;
			EXPECT_FALSE(readAsciiSTL(normalLine, "0 0 0", result)) << "normal '" << normalLine << "'";
		}
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
		makeMD2 (int32_t numVertices, int32_t numFrames, uint16_t triVertexIndex, int32_t skinSize = 1) noexcept
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
			putI32(8, skinSize);                                /* skinwidth (a divisor of the texture coordinates) */
			putI32(12, skinSize);                               /* skinheight */
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

	TEST(VertexFactoryMDx, md3TruncatedHeaderIsRejected)
	{
		/* NOTE: "IDP3" then 8 bytes of a 108-byte header. Before 2026-10-16 the rest of the header stayed indeterminate
		 * and only the stream's fail state (tellg() == -1 in exceedsStream()) refused the file; a regression guard for
		 * the explicit refusal that replaced that luck. */
		std::vector< std::byte > buffer(12, std::byte{0});
		buffer[0] = static_cast< std::byte >('I');
		buffer[1] = static_cast< std::byte >('D');
		buffer[2] = static_cast< std::byte >('P');
		buffer[3] = static_cast< std::byte >('3');

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

	/* Ave Robustus II (2026-10-08): the skin size divides every texture coordinate — a zero size from the file put inf /
	 * NaN into the geometry and the load SUCCEEDED. The control (size 1) proves the layout itself loads. */
	TEST(VertexFactoryMDx, md2ZeroSkinSizeIsRejected)
	{
		{
			const auto control = makeMD2(/* numVertices */ 1, /* numFrames */ 1, /* triVertexIndex */ 0, /* skinSize */ 1);

			MemoryStream stream{control};
			MDx format;
			Result result;
			EXPECT_TRUE(format.readStream(stream, result, {}));
		}

		for ( const int32_t skinSize : {0, -1} )
		{
			const auto buffer = makeMD2(/* numVertices */ 1, /* numFrames */ 1, /* triVertexIndex */ 0, skinSize);

			MemoryStream stream{buffer};
			MDx format;
			Result result;
			EXPECT_FALSE(format.readStream(stream, result, {})) << "skin size " << skinSize;
		}
	}

	/* An MD5 block shorter than its declared count read the closing brace and the next blocks as entries. */
	TEST(VertexFactoryMDx, md5ShortJointBlockIsRejected)
	{
		const auto md5 = [] (int numJoints) {
			const std::string text =
				"MD5Version 10\n"
				"commandline \"\"\n"
				"numJoints " + std::to_string(numJoints) + "\n"
				"numMeshes 1\n"
				"joints {\n"
				"\t\"root\"\t-1 ( 0 0 0 ) ( 0 0 0 )\n"
				"}\n"
				"mesh {\n"
				"\tshader \"x\"\n"
				"\tnumverts 3\n"
				"\tvert 0 ( 0 0 ) 0 1\n"
				"\tvert 1 ( 0 0 ) 1 1\n"
				"\tvert 2 ( 0 0 ) 2 1\n"
				"\tnumtris 1\n"
				"\ttri 0 0 1 2\n"
				"\tnumweights 3\n"
				"\tweight 0 0 1.0 ( 0 0 0 )\n"
				"\tweight 1 0 1.0 ( 1 0 0 )\n"
				"\tweight 2 0 1.0 ( 0 1 0 )\n"
				"}\n";

			std::vector< std::byte > bytes(text.size());
			std::memcpy(bytes.data(), text.data(), text.size());

			return bytes;
		};

		{
			const auto control = md5(1);

			MemoryStream stream{control};
			MDx format;
			Result result;
			EXPECT_TRUE(format.readStream(stream, result, {}));
		}

		{
			const auto shortBlock = md5(2);

			MemoryStream stream{shortBlock};
			MDx format;
			Result result;
			EXPECT_FALSE(format.readStream(stream, result, {}));
		}
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

		std::vector< std::byte > raw(md3.size());
		std::memcpy(raw.data(), md3.data(), md3.size());

		/* NOTE: const — MemoryStream's non-const overload opens for WRITING, and the read
		 * then fails before the loader is reached, making this test pass vacuously. */
		const std::vector< std::byte > & buffer = raw;

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

		std::vector< std::byte > raw(md5.size());
		std::memcpy(raw.data(), md5.data(), md5.size());

		/* NOTE: const — MemoryStream's non-const overload opens for WRITING, and the read
		 * then fails before the loader is reached, making this test pass vacuously. */
		const std::vector< std::byte > & buffer = raw;

		MemoryStream stream{buffer};
		MDx format;
		Result result;
		EXPECT_FALSE(format.readStream(stream, result, {}));
	}

	TEST(VertexFactoryMDx, md5VertexWithMoreThanFourWeightsLoads)
	{
		/* A vertex may legitimately bind to more than four joints; the loader keeps the four largest
		 * biases and renormalizes them. That branch used to test an ALREADY PARSED, in-memory weight
		 * count against the STREAM — which is at EOF by then, so tellg() returned -1 and the guard
		 * answered "exceeds" every single time. Every MD5 model owning such a vertex was rejected
		 * (cyberdemon.md5mesh). The real bound is enforced by the validation phase
		 * (startWeight + countWeight <= weights.size()), so the guard was redundant as well as wrong. */
		static constexpr std::string_view md5{
			"MD5Version 10\n"
			"commandline \"\"\n"
			"numJoints 2\n"
			"numMeshes 1\n"
			"joints {\n"
			"\t\"root\" -1 ( 0 0 0 ) ( 0 0 0 )\n"
			"\t\"bone\" 0 ( 1 0 0 ) ( 0 0 0 )\n"
			"}\n"
			"mesh {\n"
			"\tshader \"test\"\n"
			"\tnumverts 3\n"
			"\tvert 0 ( 0 0 ) 0 5\n"
			"\tvert 1 ( 1 0 ) 5 1\n"
			"\tvert 2 ( 0 1 ) 6 1\n"
			"\tnumtris 1\n"
			"\ttri 0 0 1 2\n"
			"\tnumweights 7\n"
			"\tweight 0 0 0.4 ( 0 0 0 )\n"
			"\tweight 1 1 0.3 ( 1 0 0 )\n"
			"\tweight 2 0 0.15 ( 0 1 0 )\n"
			"\tweight 3 1 0.1 ( 0 0 1 )\n"
			"\tweight 4 0 0.05 ( 1 1 0 )\n"
			"\tweight 5 0 1.0 ( 0 0 0 )\n"
			"\tweight 6 1 1.0 ( 1 1 1 )\n"
			"}\n"
		};

		std::vector< std::byte > raw(md5.size());
		std::memcpy(raw.data(), md5.data(), md5.size());

		/* NOTE: The buffer MUST be const — MemoryStream's non-const overload opens for WRITING, and
		 * a read from it fails before the loader is ever reached. */
		const std::vector< std::byte > & buffer = raw;

		MemoryStream stream{buffer};
		MDx format;
		Result result;
		ASSERT_TRUE(format.readStream(stream, result, {}));

		ASSERT_EQ(result.shape.triangles().size(), 1U);
		ASSERT_EQ(result.shape.vertices().size(), 3U);

		/* The first vertex declares FIVE weights: the four largest biases (0.4, 0.3, 0.15, 0.1) are
		 * kept, the 0.05 one is dropped, and the survivors are renormalized to sum to 1. */
		const auto & vertex = result.shape.vertices()[0];

		EXPECT_EQ(vertex.influences()[0], 0);
		EXPECT_EQ(vertex.influences()[1], 1);

		const auto weightSum = vertex.weights()[0] + vertex.weights()[1] + vertex.weights()[2] + vertex.weights()[3];
		EXPECT_NEAR(weightSum, 1.0F, 1e-5F);
		EXPECT_NEAR(vertex.weights()[0], 0.4F / 0.95F, 1e-5F);
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

		std::vector< std::byte > raw(md3.size());
		std::memcpy(raw.data(), md3.data(), md3.size());

		/* NOTE: const — MemoryStream's non-const overload opens for WRITING, and the read
		 * then fails before the loader is reached, making this test pass vacuously. */
		const std::vector< std::byte > & buffer = raw;

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

	/* The coverage pass (2026-10-17): files that LOAD, checked value by value. The MDL skin / frame GROUPS were read as
	 * a single skin / simple frame (owner decision: read the variants); the MD3 normal bytes were swapped. */
	namespace
	{
		/* Little-endian writer for the hand-built id Tech files. */
		class Bytes final
		{
			public:

				void i32 (int32_t value) { this->raw(&value, sizeof(value)); }
				void f32 (float value) { this->raw(&value, sizeof(value)); }
				void i16 (int16_t value) { this->raw(&value, sizeof(value)); }
				void u8 (uint8_t value) { this->raw(&value, sizeof(value)); }
				void zeros (size_t count) { m_data.insert(m_data.end(), count, std::byte{0}); }
				void tag (std::string_view text) { this->raw(text.data(), text.size()); }

				[[nodiscard]]
				const std::vector< std::byte > &
				data () const noexcept
				{
					return m_data;
				}

			private:

				void
				raw (const void * source, size_t size)
				{
					const auto offset = m_data.size();
					m_data.resize(offset + size);
					std::memcpy(m_data.data() + offset, source, size);
				}

				std::vector< std::byte > m_data;
		};

		/* A Quake 1 MDL: 2x2 skin, 3 vertices (10, 0, 0), (0, 20, 0), (0, 0, 30) with normal 5 (0, 0, 1), one triangle.
		 * With groups, the skin is a group of two images and the frame a group of two frames — the first one carrying the
		 * same vertices, the second other ones. */
		std::vector< std::byte >
		makeQuakeMDL (bool groups)
		{
			Bytes file;
			file.tag("IDPO");
			file.i32(6);
			for ( int index = 0; index < 3; ++index ) { file.f32(1.0F); }
			for ( int index = 0; index < 3; ++index ) { file.f32(0.0F); }
			file.f32(1.0F);
			for ( int index = 0; index < 3; ++index ) { file.f32(0.0F); }
			file.i32(1);    /* num_skins */
			file.i32(2);    /* skinwidth */
			file.i32(2);    /* skinheight */
			file.i32(3);    /* num_verts */
			file.i32(1);    /* num_tris */
			file.i32(1);    /* num_frames */
			file.i32(0);    /* synctype */
			file.i32(0);    /* flags */
			file.f32(1.0F); /* size */

			if ( groups )
			{
				file.i32(1);
				file.i32(2);
				file.f32(0.1F);
				file.f32(0.2F);
				file.zeros(8);
			}
			else
			{
				file.i32(0);
				file.zeros(4);
			}

			/* Texture coordinates (onseam, s, t), then the triangle (facesfront, three indices). */
			file.i32(0); file.i32(0); file.i32(0);
			file.i32(0); file.i32(1); file.i32(0);
			file.i32(0); file.i32(0); file.i32(1);
			file.i32(1); file.i32(0); file.i32(1); file.i32(2);

			const auto simpleFrame = [&file] (uint8_t a, uint8_t b, uint8_t c) {
				file.zeros(8);
				file.zeros(16);
				file.u8(a); file.u8(0); file.u8(0); file.u8(5);
				file.u8(0); file.u8(b); file.u8(0); file.u8(5);
				file.u8(0); file.u8(0); file.u8(c); file.u8(5);
			};

			if ( groups )
			{
				file.i32(1);
				file.i32(2);
				file.zeros(8);
				file.f32(0.1F);
				file.f32(0.2F);
				simpleFrame(10, 20, 30);
				simpleFrame(99, 99, 99);
			}
			else
			{
				file.i32(0);
				simpleFrame(10, 20, 30);
			}

			return file.data();
		}

		/* Whether the shape holds a vertex at this position (any order), and every vertex has this normal. */
		bool
		hasVertex (const Shape< float, uint32_t > & shape, float x, float y, float z, float s, float t)
		{
			return std::ranges::any_of(shape.vertices(), [&] (const auto & vertex) {
				const auto & position = vertex.position();
				const auto & uv = vertex.textureCoordinates();

				return std::abs(position[0] - x) < 1e-5F && std::abs(position[1] - y) < 1e-5F && std::abs(position[2] - z) < 1e-5F
					&& std::abs(uv[0] - s) < 1e-5F && std::abs(uv[1] - t) < 1e-5F;
			});
		}

		bool
		everyNormalIs (const Shape< float, uint32_t > & shape, float x, float y, float z)
		{
			return !shape.vertices().empty() && std::ranges::all_of(shape.vertices(), [&] (const auto & vertex) {
				const auto & normal = vertex.normal();

				return std::abs(normal[0] - x) < 1e-3F && std::abs(normal[1] - y) < 1e-3F && std::abs(normal[2] - z) < 1e-3F;
			});
		}
	}

	TEST(VertexFactoryMDx, mdlLoadsItsVerticesNormalsAndCoordinates)
	{
		/* id Tech (X forward, Y left, Z up) -> engine (y, z, x), x 0.01; UV (s + 0.5) / width; normal 5 = (0, 0, 1). */
		for ( const bool groups : {false, true} )
		{
			const auto buffer = makeQuakeMDL(groups);
			MemoryStream stream{buffer};
			MDx format;
			Result result;

			ASSERT_TRUE(format.readStream(stream, result, {})) << "groups " << groups;
			ASSERT_EQ(result.shape.triangles().size(), 1U);

			EXPECT_TRUE(hasVertex(result.shape, 0.0F, 0.0F, 0.1F, 0.25F, 0.25F)) << "groups " << groups;
			EXPECT_TRUE(hasVertex(result.shape, 0.2F, 0.0F, 0.0F, 0.75F, 0.25F)) << "groups " << groups;
			EXPECT_TRUE(hasVertex(result.shape, 0.0F, 0.3F, 0.0F, 0.25F, 0.75F)) << "groups " << groups;
			EXPECT_TRUE(everyNormalIs(result.shape, 0.0F, 1.0F, 0.0F)) << "groups " << groups;
		}
	}

	TEST(VertexFactoryMDx, mdlTruncatedOrBadGroupIsRefused)
	{
		/* Cut in the middle of the frame: the reads used to leave zeros and load. */
		{
			auto buffer = makeQuakeMDL(false);
			buffer.resize(buffer.size() - 4);

			MemoryStream stream{std::as_const(buffer)};
			MDx format;
			Result result;
			EXPECT_FALSE(format.readStream(stream, result, {}));
		}

		/* A skin group of zero images. */
		{
			auto buffer = makeQuakeMDL(true);
			const int32_t zero = 0;
			std::memcpy(buffer.data() + 88, &zero, sizeof(zero));

			MemoryStream stream{std::as_const(buffer)};
			MDx format;
			Result result;
			EXPECT_FALSE(format.readStream(stream, result, {}));
		}
	}

	TEST(VertexFactoryMDx, md3NormalsFollowQuake3)
	{
		/* One surface, one triangle, three vertices whose normal is +X in id Tech space — Quake III encodes it as the
		 * short (lat 0 << 8) | lng 64, the bytes [64, 0] on disk — which is +Z in the engine (y, z, x). The bytes were
		 * read swapped: +X came out +Y. */
		Bytes file;
		file.tag("IDP3");
		file.i32(15);
		file.zeros(64);
		file.i32(0);
		file.i32(1);      /* num_frames */
		file.i32(0);      /* num_tags */
		file.i32(1);      /* num_surfaces */
		file.i32(0);      /* num_skins */
		file.i32(108);    /* offset_frames */
		file.i32(108);    /* offset_tags */
		file.i32(108);    /* offset_surfaces */
		file.i32(108 + 168);

		/* The surface: header (108), triangle (12), texture coordinates (3 x 8), vertices (3 x 8). */
		file.tag("IDP3");
		file.zeros(64);
		file.i32(0);
		file.i32(1);      /* num_frames */
		file.i32(0);      /* num_shaders */
		file.i32(3);      /* num_verts */
		file.i32(1);      /* num_triangles */
		file.i32(108);    /* offset_triangles */
		file.i32(108);    /* offset_shaders */
		file.i32(120);    /* offset_st */
		file.i32(144);    /* offset_xyzn */
		file.i32(168);    /* offset_end */
		file.i32(0); file.i32(1); file.i32(2);
		for ( int index = 0; index < 3; ++index ) { file.f32(0.5F); file.f32(0.5F); }

		for ( const auto & position : std::array< std::array< int16_t, 3 >, 3 >{{{64, 0, 0}, {0, 64, 0}, {0, 0, 64}}} )
		{
			file.i16(position[0]);
			file.i16(position[1]);
			file.i16(position[2]);
			file.u8(64);
			file.u8(0);
		}

		const auto & buffer = file.data();
		MemoryStream stream{buffer};
		MDx format;
		Result result;

		ASSERT_TRUE(format.readStream(stream, result, {}));
		ASSERT_EQ(result.shape.triangles().size(), 1U);
		EXPECT_TRUE(everyNormalIs(result.shape, 0.0F, 0.0F, 1.0F));
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

	namespace
	{
		/* The attribute a triangle corner ends up with, read back from the loaded shape. */
		Math::Vector< 3, float >
		cornerNormal (const Result & result, size_t triangle, uint32_t corner) noexcept
		{
			return result.shape.vertices()[result.shape.triangles()[triangle].vertexIndex(corner)].normal();
		}

		Math::Vector< 3, float >
		cornerTextureCoordinates (const Result & result, size_t triangle, uint32_t corner) noexcept
		{
			return result.shape.vertices()[result.shape.triangles()[triangle].vertexIndex(corner)].textureCoordinates();
		}
	}

	TEST(VertexFactoryOBJ, sharedPositionKeepsEachCornerNormalAndTextureCoordinates)
	{
		/* A position shared by two faces with DIFFERENT normals and texture coordinates (a hard
		 * edge, a UV seam) is two vertices: an OBJ vertex is the whole (v, vt, vn) triple. The
		 * loader keyed its vertices by ONE index (the most numerous attribute) and duplicated a
		 * vertex only when the POSITION differed, so the second face inherited the first face's
		 * normal and UV — on basic-scenery's temple, 381 triangles inherited a ZERO normal (black)
		 * and 4880 corners a foreign one (2026-10-05). Positions (4) outnumber normals (2) and
		 * texture coordinates (2) here, the case that keyed by position. */
		const auto buffer = toBytes(
			"v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
			"vt 0 0\nvt 1 1\n"
			"vn 0 0 1\nvn 1 0 0\n"
			"f 1/1/1 2/1/1 3/1/1\n"
			"f 1/2/2 3/2/2 4/2/2\n");

		MemoryStream stream{buffer};
		OBJ format;
		Result result;
		ASSERT_TRUE(format.readStream(stream, result, {}));
		ASSERT_EQ(result.shape.triangles().size(), 2U);

		for ( uint32_t corner = 0; corner < 3; ++corner )
		{
			EXPECT_EQ(cornerNormal(result, 0, corner), (Math::Vector< 3, float >{0.0F, 0.0F, 1.0F})) << "triangle 0, corner " << corner;
			EXPECT_EQ(cornerNormal(result, 1, corner), (Math::Vector< 3, float >{1.0F, 0.0F, 0.0F})) << "triangle 1, corner " << corner;
			EXPECT_EQ(cornerTextureCoordinates(result, 0, corner)[0], 0.0F) << "triangle 0, corner " << corner;
			EXPECT_EQ(cornerTextureCoordinates(result, 1, corner)[0], 1.0F) << "triangle 1, corner " << corner;
		}

		/* Position 1 opens both faces with two different triples: two vertices. */
		EXPECT_NE(result.shape.triangles()[0].vertexIndex(0), result.shape.triangles()[1].vertexIndex(0));
	}

	/* A MIRROR SEAM: two quads share an edge (same position, UV and normal on both sides) and the right one is mapped
	 * mirrored in U — the usual way a symmetric model saves texture space. Its frame is the other side's mirror image
	 * (opposite tangent, opposite handedness, the same bitangent), so the seam corners cannot be ONE vertex: the loader
	 * keys a vertex by (v, vt, vn) AND the side of its face's UV mapping (2026-10-07, base item
	 * computed-tangent-space-never-derives-the-handedness). Before, the two seam vertices were shared and took one
	 * side's frame, lighting the other side's normal map backwards along the seam. */
	TEST(VertexFactoryOBJ, aMirrorSeamSplitsItsVerticesAndEachSideGetsItsFrame)
	{
		const auto buffer = toBytes(
			"v -1 -1 0\nv 0 -1 0\nv 0 1 0\nv -1 1 0\nv 1 -1 0\nv 1 1 0\n"
			"vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
			"vn 0 0 1\n"
			"f 1/1/1 2/2/1 3/3/1 4/4/1\n"
			"f 2/2/1 5/1/1 6/4/1 3/3/1\n");

		MemoryStream stream{buffer};
		OBJ format;
		Result result;
		ReadOptions options{};
		options.requestTangentSpace = true;
		ASSERT_TRUE(format.readStream(stream, result, options));
		ASSERT_EQ(result.shape.triangles().size(), 4U);

		/* Four corners per quad: the two seam corners are split. */
		EXPECT_EQ(result.shape.vertices().size(), 8U);

		const auto & vertices = result.shape.vertices();
		const auto & triangles = result.shape.triangles();
		const auto & reference = vertices[triangles[0].vertexIndex(0)];

		for ( uint32_t triangle = 0; triangle < 4; ++triangle )
		{
			/* Triangles 0-1 are the left quad, 2-3 the mirrored right one. */
			const auto side = triangle < 2 ? 1.0F : -1.0F;

			for ( uint32_t corner = 0; corner < 3; ++corner )
			{
				const auto & vertex = vertices[triangles[triangle].vertexIndex(corner)];

				EXPECT_EQ(vertex.tangentHandedness(), reference.tangentHandedness() * side) << "triangle " << triangle << ", corner " << corner;
				EXPECT_NEAR(vertex.tangent()[Math::X], reference.tangent()[Math::X] * side, 1.0E-5F) << "triangle " << triangle << ", corner " << corner;
				EXPECT_NEAR(vertex.biNormal()[Math::Y], reference.biNormal()[Math::Y], 1.0E-5F) << "triangle " << triangle << ", corner " << corner;
			}
		}

		EXPECT_NEAR(std::abs(reference.biNormal()[Math::Y]), 1.0F, 1.0E-5F);
	}

	TEST(VertexFactoryOBJ, sharedPositionKeepsEachCornerNormalWithoutTextureCoordinates)
	{
		/* The same defect on the "v//vn" path. */
		const auto buffer = toBytes(
			"v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
			"vn 0 0 1\nvn 1 0 0\n"
			"f 1//1 2//1 3//1\n"
			"f 1//2 3//2 4//2\n");

		MemoryStream stream{buffer};
		OBJ format;
		Result result;
		ASSERT_TRUE(format.readStream(stream, result, {}));
		ASSERT_EQ(result.shape.triangles().size(), 2U);

		for ( uint32_t corner = 0; corner < 3; ++corner )
		{
			EXPECT_EQ(cornerNormal(result, 0, corner), (Math::Vector< 3, float >{0.0F, 0.0F, 1.0F})) << "triangle 0, corner " << corner;
			EXPECT_EQ(cornerNormal(result, 1, corner), (Math::Vector< 3, float >{1.0F, 0.0F, 0.0F})) << "triangle 1, corner " << corner;
		}
	}

	TEST(VertexFactoryOBJ, sharedPositionKeepsEachCornerTextureCoordinatesWithoutNormals)
	{
		/* The same defect on the "v/vt" path (a UV seam). */
		const auto buffer = toBytes(
			"v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
			"vt 0 0\nvt 1 1\n"
			"f 1/1 2/1 3/1\n"
			"f 1/2 3/2 4/2\n");

		MemoryStream stream{buffer};
		OBJ format;
		Result result;
		ASSERT_TRUE(format.readStream(stream, result, {}));
		ASSERT_EQ(result.shape.triangles().size(), 2U);

		for ( uint32_t corner = 0; corner < 3; ++corner )
		{
			EXPECT_EQ(cornerTextureCoordinates(result, 0, corner)[0], 0.0F) << "triangle 0, corner " << corner;
			EXPECT_EQ(cornerTextureCoordinates(result, 1, corner)[0], 1.0F) << "triangle 1, corner " << corner;
		}
	}

	TEST(VertexFactoryOBJ, repeatedTripleSharesOneVertex)
	{
		/* The fix must not stop sharing: a quad's diagonal corners repeat the whole triple and stay
		 * ONE vertex (4 vertices for 2 triangles, not 6). */
		const auto buffer = toBytes(
			"v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
			"vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
			"vn 0 0 1\n"
			"f 1/1/1 2/2/1 3/3/1 4/4/1\n");

		MemoryStream stream{buffer};
		OBJ format;
		Result result;
		ASSERT_TRUE(format.readStream(stream, result, {}));
		ASSERT_EQ(result.shape.triangles().size(), 2U);
		EXPECT_EQ(result.shape.triangles()[0].vertexIndex(0), result.shape.triangles()[1].vertexIndex(0));
		EXPECT_EQ(result.shape.triangles()[0].vertexIndex(2), result.shape.triangles()[1].vertexIndex(1));
		EXPECT_EQ(result.shape.vertices().size(), 4U);
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

	/* The coverage pass (2026-10-17): the writer, the attribute forms, groups, the scale option, CRLF, malformed faces. */
	namespace
	{
		/* Reads an OBJ text; an empty shape on refusal. */
		Result
		readOBJ (std::string_view text, const ReadOptions & options = {})
		{
			const auto buffer = toBytes(text);
			MemoryStream stream{buffer};
			OBJ format;
			Result result;

			if ( !format.readStream(stream, result, options) )
			{
				result = Result{};
			}

			return result;
		}

		/* Writes a shape as OBJ text. */
		std::string
		writeOBJ (const Shape< float, uint32_t > & shape)
		{
			std::vector< std::byte > buffer;
			MemoryStream stream{buffer};
			const OBJ format;

			if ( !format.writeStream(stream, shape, {}) )
			{
				return {};
			}

			std::string text(buffer.size(), '\0');
			std::memcpy(text.data(), buffer.data(), buffer.size());

			return text;
		}
	}

	TEST(VertexFactoryOBJ, writerRoundTripKeepsEveryFloat)
	{
		/* Values a 6-digit output would round: the file must give back the same floats. */
		const auto source = readOBJ("v 0.123456789 1234.56789 -0.000123456789\nv 1.00000012 2 3\nv 7.654321e-5 -98765.4321 0.1\n"
			"vt 0.333333343 0.666666687\nvn 0 0.707106781 0.707106781\nf 1/1/1 2/1/1 3/1/1\n");
		ASSERT_EQ(source.shape.triangles().size(), 1U);

		const auto text = writeOBJ(source.shape);
		ASSERT_FALSE(text.empty());

		const auto copy = readOBJ(text);
		ASSERT_EQ(copy.shape.vertices().size(), source.shape.vertices().size());

		for ( size_t index = 0; index < source.shape.vertices().size(); ++index )
		{
			const auto & a = source.shape.vertices()[index];
			const auto & b = copy.shape.vertices()[index];

			for ( size_t axis = 0; axis < 3; ++axis )
			{
				EXPECT_EQ(a.position()[axis], b.position()[axis]) << "vertex " << index << " axis " << axis;
				EXPECT_EQ(a.normal()[axis], b.normal()[axis]) << "vertex " << index << " axis " << axis;
			}

			EXPECT_EQ(a.textureCoordinates()[0], b.textureCoordinates()[0]) << "vertex " << index;
			EXPECT_EQ(a.textureCoordinates()[1], b.textureCoordinates()[1]) << "vertex " << index;
		}
	}

	TEST(VertexFactoryOBJ, writerEmitsTheFaceFormOfItsAttributes)
	{
		const auto faceLine = [] (std::string_view obj) {
			const auto text = writeOBJ(readOBJ(obj).shape);
			const auto start = text.find("\nf ");

			return start == std::string::npos ? std::string{} : text.substr(start + 1, text.find('\n', start + 1) - start - 1);
		};

		EXPECT_EQ(faceLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"), "f 1 2 3");
		EXPECT_EQ(faceLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/1 2/1 3/1\n"), "f 1/1 2/2 3/3");
		EXPECT_EQ(faceLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n"), "f 1//1 2//2 3//3");
		EXPECT_EQ(faceLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvn 0 0 1\nf 1/1/1 2/1/1 3/1/1\n"), "f 1/1/1 2/2/2 3/3/3");

		/* An empty shape is not written. */
		EXPECT_TRUE(writeOBJ(Shape< float, uint32_t >{}).empty());
	}

	TEST(VertexFactoryOBJ, groupsScaleAndLineEndings)
	{
		/* Two "g" sections: (first triangle, count) = (0, 1) and (1, 1). */
		const auto grouped = readOBJ("v 0 0 0\nv 1 0 0\nv 0 1 0\ng first\nf 1 2 3\ng second\nf 3 2 1\n");
		ASSERT_EQ(grouped.shape.groups().size(), 2U);
		EXPECT_EQ(grouped.shape.groups()[0], (std::pair< uint32_t, uint32_t >{0, 1}));
		EXPECT_EQ(grouped.shape.groups()[1], (std::pair< uint32_t, uint32_t >{1, 1}));

		/* The scale option. */
		ReadOptions options{};
		options.scaleFactor = 2.0F;
		const auto scaled = readOBJ("v 1 2 3\nv 1 0 0\nv 0 1 0\nf 1 2 3\n", options);
		ASSERT_EQ(scaled.shape.vertices().size(), 3U);
		EXPECT_TRUE(std::ranges::any_of(scaled.shape.vertices(), [] (const auto & vertex) { return vertex.position()[0] == 2.0F && vertex.position()[1] == 4.0F && vertex.position()[2] == 6.0F; }));

		/* CRLF reads like LF. */
		const auto lf = readOBJ("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0.5 0.25\nf 1/1 2/1 3/1\n");
		const auto crlf = readOBJ("v 0 0 0\r\nv 1 0 0\r\nv 0 1 0\r\nvt 0.5 0.25\r\nf 1/1 2/1 3/1\r\n");
		ASSERT_EQ(crlf.shape.triangles().size(), 1U);
		ASSERT_EQ(crlf.shape.vertices().size(), lf.shape.vertices().size());

		for ( size_t index = 0; index < lf.shape.vertices().size(); ++index )
		{
			EXPECT_EQ(crlf.shape.vertices()[index].position(), lf.shape.vertices()[index].position());
			EXPECT_EQ(crlf.shape.vertices()[index].textureCoordinates(), lf.shape.vertices()[index].textureCoordinates());
		}
	}

	TEST(VertexFactoryOBJ, requestedNormalsAndTangentsInEveryFaceForm)
	{
		/* A triangle in the XY plane: whatever the face form, a requested normal is +Z (counter-clockwise winding). */
		ReadOptions options{};
		options.requestNormal = true;
		options.requestTangentSpace = true;

		for ( const auto * const obj : {
			"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n",
			"v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nf 1/1 2/2 3/3\n",
			"v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n",
			"v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\nf 1/1/1 2/2/1 3/3/1\n"} )
		{
			const auto result = readOBJ(obj, options);

			ASSERT_EQ(result.shape.triangles().size(), 1U) << obj;
			EXPECT_TRUE(result.shape.isNormalsAvailable()) << obj;
			EXPECT_TRUE(std::ranges::all_of(result.shape.vertices(), [] (const auto & vertex) { return std::abs(vertex.normal()[2] - 1.0F) < 1e-5F; })) << obj;
		}
	}

	TEST(VertexFactoryOBJ, malformedFilesAreRefused)
	{
		EXPECT_TRUE(readOBJ("v 0 0 0\nv 1 0 0\nf 1 2\n").shape.triangles().empty()) << "a face of two vertices";
		EXPECT_TRUE(readOBJ("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 x 3\n").shape.triangles().empty()) << "a face token that is not a number";
		EXPECT_TRUE(readOBJ("f 1 2 3\n").shape.triangles().empty()) << "no vertex";
		EXPECT_TRUE(readOBJ("v 0 0 0\nv 1 0 0\nv 0 1 0\n").shape.triangles().empty()) << "no face";
		EXPECT_TRUE(readOBJ("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/1 2/1 3/9\n").shape.triangles().empty()) << "a texture index out of range";
		EXPECT_TRUE(readOBJ("v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//9\n").shape.triangles().empty()) << "a normal index out of range";
	}

	TEST(VertexFactoryStreamIO, nonFiniteGeometryIsRefused)
	{
		/* NOTE: Owner decision 2026-10-16 (base item shape-merge-quantization-overflow): the file readers refuse a
		 * geometry holding a NaN or an infinity. The native format stores raw floats, so a NaN survives the round trip
		 * and only the read path's check can refuse it. */
		const EmEn::Base::Math::Vector< 3, float > up{0.0F, 1.0F, 0.0F};

		Shape< float, uint32_t > shape;
		static_cast< void >(shape.saveVertex({0.0F, 0.0F, 0.0F}, up));
		static_cast< void >(shape.saveVertex({1.0F, 0.0F, 0.0F}, up));
		static_cast< void >(shape.saveVertex({0.0F, std::numeric_limits< float >::quiet_NaN(), 0.0F}, up));
		ShapeTriangle< float > triangle{0, 1, 2};
		shape.addTriangle(triangle);

		ASSERT_FALSE(shape.hasOnlyFiniteData());

		std::vector< std::byte > buffer;
		ASSERT_TRUE(StreamIO::write(shape, FileFormatType::Native, buffer));

		Result result;
		EXPECT_FALSE(StreamIO::read(buffer, FileFormatType::Native, result));
		EXPECT_TRUE(result.shape.vertices().empty()) << "a refused geometry must not be left half-usable";

		/* Control: the same shape without the NaN reads back. */
		Shape< float, uint32_t > control;
		static_cast< void >(control.saveVertex({0.0F, 0.0F, 0.0F}, up));
		static_cast< void >(control.saveVertex({1.0F, 0.0F, 0.0F}, up));
		static_cast< void >(control.saveVertex({0.0F, 1.0F, 0.0F}, up));
		control.addTriangle(triangle);

		ASSERT_TRUE(control.hasOnlyFiniteData());

		std::vector< std::byte > controlBuffer;
		ASSERT_TRUE(StreamIO::write(control, FileFormatType::Native, controlBuffer));

		Result controlResult;
		EXPECT_TRUE(StreamIO::read(controlBuffer, FileFormatType::Native, controlResult));
	}

	TEST(VertexFactoryStreamIO, formatParity)
	{
		/* Ave robustus! (Axis B): StreamIO must reach every format FileIO does. OBJ read gives a shape;
		 * Native and STL round-trip through the buffer; MDx is read-only but dispatch-reachable. */
		const auto objBuffer = toBytes("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
		Result objResult;
		ASSERT_TRUE(StreamIO::read(objBuffer, FileFormatType::OBJ, objResult));
		ASSERT_EQ(objResult.shape.triangles().size(), 1U);

		std::vector< std::byte > nativeBuffer;
		ASSERT_TRUE(StreamIO::write(objResult.shape, FileFormatType::Native, nativeBuffer));
		EXPECT_FALSE(nativeBuffer.empty());
		Result nativeResult;
		ASSERT_TRUE(StreamIO::read(nativeBuffer, FileFormatType::Native, nativeResult));
		EXPECT_EQ(nativeResult.shape.triangles().size(), 1U);

		std::vector< std::byte > stlBuffer;
		ASSERT_TRUE(StreamIO::write(objResult.shape, FileFormatType::STL, stlBuffer));
		EXPECT_FALSE(stlBuffer.empty());
		Result stlResult;
		ASSERT_TRUE(StreamIO::read(stlBuffer, FileFormatType::STL, stlResult));
		EXPECT_GE(stlResult.shape.triangles().size(), 1U);

		/* MDx is read-only: dispatch is reachable, write fails cleanly, garbage read fails cleanly. */
		const auto mdxGarbage = makeGarbageWithMagic('2');
		Result mdxResult;
		EXPECT_FALSE(StreamIO::read(mdxGarbage, FileFormatType::MDx, mdxResult));
		std::vector< std::byte > mdxBuffer;
		EXPECT_FALSE(StreamIO::write(objResult.shape, FileFormatType::MDx, mdxBuffer));
	}

	TEST(VertexFactoryFileIO, readableExtensionPredicate)
	{
		/* The predicate mirrors the FileIO::read() dispatch : every readable
		 * extension answers true, anything else answers false. */
		for ( const auto & extension : FileIO::ReadableExtensions )
		{
			EXPECT_TRUE(FileIO::isReadableExtension(extension)) << extension;
		}

		/* Composite/scene formats belong to the engine scene loaders, not here. */
		EXPECT_FALSE(FileIO::isReadableExtension("gltf"));
		EXPECT_FALSE(FileIO::isReadableExtension("glb"));
		EXPECT_FALSE(FileIO::isReadableExtension("fbx"));
		EXPECT_FALSE(FileIO::isReadableExtension("png"));
		EXPECT_FALSE(FileIO::isReadableExtension(""));

		/* The contract is lowercase without the leading dot. */
		EXPECT_FALSE(FileIO::isReadableExtension("OBJ"));
		EXPECT_FALSE(FileIO::isReadableExtension(".obj"));
	}
}
