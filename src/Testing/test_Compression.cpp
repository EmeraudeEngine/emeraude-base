/*
 * src/Testing/test_Compression.cpp
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

/* STL inclusions. */
#include <cstring>
#include <sstream>
#include <string>

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "Compression/LZMA.hpp"
#include "Compression/LZMA/Compressor.hpp"
#include "Compression/LZMA/Decompressor.hpp"
#include "Compression/ZLIB.hpp"
#include "Randomizer.hpp"
#include "Time/Elapsed/PrintScopeRealTime.hpp"
#include "BaseUtility.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::Compression;
using namespace EmEn::Base::Time::Elapsed;

namespace
{
	[[nodiscard]]
	std::string
	createSource (size_t size) noexcept
	{
		std::string source;

		Randomizer< float > randomizer;

		const auto coordinates = randomizer.vector(512 * 3 * size, -32000.0F, 32000.0F);

		const auto bytes = coordinates.size() * sizeof(float);

		source.resize(bytes);

		std::memcpy(source.data(), coordinates.data(), bytes);

		return source;
	}
}

TEST(Compression, ZLIBString)
{
	const auto source = createSource(2048UL);

	std::string compressed;

	{
		PrintScopeRealTime stat{"ZLIB::compressString()"};

		ASSERT_TRUE(ZLIB::compressString(source, compressed));
	}

	std::string recovered;

	{
		PrintScopeRealTime stat{"ZLIB::decompressString()"};

		ASSERT_TRUE(ZLIB::decompressString(compressed, recovered));
	}

	ASSERT_EQ(source, recovered);
}

TEST(Compression, LZMAStringBasic)
{
	const auto source = createSource(2048UL);

	std::string compressed;

	{
		PrintScopeRealTime stat{"LZMA::compressString()"};

		ASSERT_TRUE(LZMA::compressString(source, compressed));
	}

	std::string recovered;

	{
		PrintScopeRealTime stat{"LZMA::decompressString()"};

		ASSERT_TRUE(LZMA::decompressString(compressed, recovered));
	}

	ASSERT_EQ(source, recovered);
}

/*
 * Regression — Ave robustus! A.3 (fuzz_compression).
 * LZMA::decompressString() allocated decoder state via lzma_stream_decoder() but only released
 * it on the success path, leaking on every malformed/truncated input. Decompressing hostile
 * bytes must cancel the load (return false) AND free the decoder (verified under LeakSanitizer).
 */
TEST(Compression, LZMADecompressMalformedInputNoLeak)
{
	std::string recovered;

	/* Single garbage byte — the 1-byte reproducer found by the fuzzer. */
	ASSERT_FALSE(LZMA::decompressString(std::string{"\x00", 1}, recovered));

	/* Arbitrary non-LZMA payload. */
	ASSERT_FALSE(LZMA::decompressString("not a valid xz stream", recovered));

	/* A valid header followed by truncated/corrupt body: compress, then keep only a prefix. */
	const auto source = createSource(64UL);
	std::string compressed;
	ASSERT_TRUE(LZMA::compressString(source, compressed));
	ASSERT_GT(compressed.size(), 8U);
	ASSERT_FALSE(LZMA::decompressString(compressed.substr(0, compressed.size() / 2), recovered));
}

/*
 * Regression — Ave robustus! A.3 (fuzz_compression).
 * ZLIB::decompressStream() read an untrusted uncompressed-size header and resize()d the
 * destination to it, so a tiny input could request a multi-GB buffer (OOM). A chunk that claims
 * a huge decompressed size must be rejected (returns false) without the giant allocation.
 */
TEST(Compression, ZLIBDecompressImplausibleSizeRejected)
{
	const size_t hugeBaseSize = static_cast< size_t >(1) << 31;  // 2 GB, far above the cap
	const size_t compressedSize = 4;

	std::string chunk;
	chunk.resize(2 * sizeof(size_t) + compressedSize, '\0');
	std::memcpy(chunk.data(), &hugeBaseSize, sizeof(size_t));
	std::memcpy(chunk.data() + sizeof(size_t), &compressedSize, sizeof(size_t));

	std::string recovered;
	ASSERT_FALSE(ZLIB::decompressString(chunk, recovered));

	/* A truncated header (fewer bytes than one chunk size field) must also be rejected. */
	ASSERT_FALSE(ZLIB::decompressString(std::string{"\x05", 1}, recovered));
}

TEST(Compression, LZMAStringMT)
{
	const auto source = createSource(2048UL);

	std::string compressed;

	{
		PrintScopeRealTime stat{"LZMA::Compressor::compressString() [MT:8]"};

		LZMA::Compressor proc{8, 9};
		ASSERT_TRUE(proc.compressString(source, compressed));
	}

	std::string recovered;

	{
		PrintScopeRealTime stat{"LZMA::Decompressor::decompressString() [MT:8]"};

		LZMA::Decompressor proc{8};
		ASSERT_TRUE(proc.decompressString(compressed, recovered));
	}

	ASSERT_EQ(source, recovered);
}

TEST(Compression, LZMAStringMTDisabled)
{
	const auto source = createSource(2048UL);

	std::string compressed;

	{
		PrintScopeRealTime stat{"LZMA::Compressor::compressString() [MT:0]"};

		LZMA::Compressor proc{0, 9};

		ASSERT_TRUE(proc.compressString(source, compressed));
	}

	std::string recovered;

	{
		PrintScopeRealTime stat{"LZMA::Decompressor::decompressString() [MT:0]"};

		LZMA::Decompressor proc{0};

		ASSERT_TRUE(proc.decompressString(compressed, recovered));
	}

	ASSERT_EQ(source, recovered);
}

/* Ave Robustus II warning pass (2026-10-08): compressString() passed the LEVEL as the chunk size, so every
 * string was compressed in 9-byte chunks at the default level, and level 0 asked for 0-byte chunks — a
 * division by zero in compressStream(). Every level must round-trip. */
TEST(Compression, ZLIBStringEveryLevelRoundTrips)
{
	std::string source;

	for ( size_t index = 0; index < 20000; ++index )
	{
		source.push_back(static_cast< char >('a' + ((index * 7U) % 26U)));
	}

	for ( int level = 0; level <= 9; ++level )
	{
		std::string compressed;
		std::string recovered;

		ASSERT_TRUE(ZLIB::compressString(source, compressed, level)) << "level " << level;
		ASSERT_TRUE(ZLIB::decompressString(compressed, recovered)) << "level " << level;
		ASSERT_EQ(source, recovered) << "level " << level;
	}
}

/* A zero chunk size is refused instead of dividing by zero. */
TEST(Compression, ZLIBStreamZeroChunkSizeRefused)
{
	std::stringstream sourceStream{std::string(1000, 'x')};
	std::stringstream targetStream;

	EXPECT_EQ(ZLIB::compressStream(sourceStream, targetStream, 0, 9), 0U);
}

/* liblzma takes the preset as a uint32_t: a negative level used to wrap into its flag bits. */
TEST(Compression, LZMAStringLevelOutOfRangeRefused)
{
	const std::string source(1000, 'y');
	std::string compressed;

	EXPECT_FALSE(LZMA::compressString(source, compressed, -1));
	EXPECT_FALSE(LZMA::compressString(source, compressed, 10));
	EXPECT_TRUE(LZMA::compressString(source, compressed, 0));
}

/* The instrumented fuzz_compression campaign (2026-10-17; the parsers had been fuzzed uninstrumented): a chunk header
 * WITHOUT its payload — 27 bytes declaring ~0.27 GB / ~1 GB chunks — made both buffers allocate before the read failed
 * (2 GB peak). The payload is now read in steps. A result-only check: the allocation is proven by the peak RSS
 * (unit-testing.md), as for every DoesNotOOM test. */
TEST(Compression, ZLIBChunkHeaderWithoutItsPayloadIsRefused)
{
	const std::string input{"\x01\x00\x00\x3D\x00\x00\x00\x00\x00\x01\x00\x3F\x00\x00\x00\x00\x00\x01\x00\x3F\x00\x00\x00\x00\x00\x00\x00", 27};
	std::string recovered;

	EXPECT_FALSE(ZLIB::decompressString(input, recovered));
}

/* 8 MiB of zeros compress to a few kilobytes of xz: beyond MaxDecompressionRatio, refused by the function and by the
 * streaming class (a 1 GiB bomb of 156 KB decompressed whole: 2.1 GB peak). */
TEST(Compression, LZMABombBeyondTheRatioIsRefused)
{
	std::string bomb;
	ASSERT_TRUE(LZMA::compressString(std::string(8U << 20U, '\0'), bomb, 1));
	ASSERT_GT(static_cast< size_t >(8U << 20U) / bomb.size(), LZMA::MaxDecompressionRatio);

	std::string recovered;
	EXPECT_FALSE(LZMA::decompressString(bomb, recovered));
	EXPECT_TRUE(recovered.empty());

	LZMA::Decompressor decompressor;
	EXPECT_FALSE(decompressor.decompressString(bomb, recovered));
}

/* An 80-byte xz stream whose block header declares a 768 MiB dictionary (LZMA2 property byte 37, the header CRC
 * recomputed; the payload is 110 bytes): beyond DecoderMemoryLimit, refused by both. The streaming class had no limit. */
TEST(Compression, LZMAHeaderDeclaringALargeDictionaryIsRefused)
{
	const std::string stream{
		"\xFD\x37\x7A\x58\x5A\x00\x00\x04\xE6\xD6\xB4\x46\x02\x00\x21\x01\x25\x00\x00\x00\x3B\x78\x7B\x41\xE0\x00\x6D\x00"
		"\x12\x5D\x00\x34\x19\x49\xDB\x85\x5C\x63\xAD\x3E\xF9\x63\x76\x2F\x13\x83\x78\x00\x00\x00\x00\x00\x94\xD7\xA3\x28"
		"\xEF\x16\x2D\xD0\x00\x01\x2E\x6E\x58\xF0\x8B\x81\x1F\xB6\xF3\x7D\x01\x00\x00\x00\x00\x04\x59\x5A", 80};
	std::string recovered;

	EXPECT_FALSE(LZMA::decompressString(stream, recovered));

	LZMA::Decompressor decompressor;
	EXPECT_FALSE(decompressor.decompressString(stream, recovered));
}

