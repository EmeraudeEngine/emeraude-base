/*
 * fuzzing/fuzz_wav.cpp
 * This file is part of Emeraude-Base
 *
 * libFuzzer target for the WaveFactory libsndfile reader (Ave robustus! — A.3).
 * Feeds arbitrary bytes through FileFormatSNDFile::readStream under ASan/UBSan. This exercises
 * our virtual-I/O callbacks and the decode-buffer bound on top of libsndfile's own parsing.
 */

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <vector>

/* Local inclusions. */
#include "IO/MemoryStream.hpp"
#include "WaveFactory/FileFormatSNDFile.hpp"
#include "WaveFactory/Wave.hpp"

extern "C" int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
	using namespace EmEn::Base;

	const std::vector< std::byte > buffer{
		reinterpret_cast< const std::byte * >(data),
		reinterpret_cast< const std::byte * >(data) + size
	};

	IO::MemoryStream stream{buffer};
	WaveFactory::FileFormatSNDFile< int16_t > format;
	WaveFactory::Wave< int16_t > wave;

	(void)format.readStream(stream, wave, {});

	return 0;
}