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
