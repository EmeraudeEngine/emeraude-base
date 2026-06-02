/*
 * fuzzing/fuzz_compression.cpp
 * This file is part of Emeraude-Base
 *
 * libFuzzer target for the Compression decompressors (Ave robustus! — A.3).
 * Feeds arbitrary bytes through LZMA::decompressString and ZLIB::decompressString under
 * ASan/UBSan. Decompressing hostile/truncated streams is the untrusted-input surface
 * (zip-bomb sizing, malformed headers, truncated payloads).
 */

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <string>

/* Local inclusions. */
#include "Compression/LZMA.hpp"
#include "Compression/ZLIB.hpp"

extern "C" int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
	using namespace EmEn::Base;

	const std::string input{
		reinterpret_cast< const char * >(data),
		size
	};

	{
		std::string output;
		(void)Compression::LZMA::decompressString(input, output);
	}

	{
		std::string output;
		(void)Compression::ZLIB::decompressString(input, output);
	}

	return 0;
}