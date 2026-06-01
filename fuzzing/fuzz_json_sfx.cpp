/*
 * fuzzing/fuzz_json_sfx.cpp
 * This file is part of Emeraude-Base
 *
 * libFuzzer target for the WaveFactory JSON procedural-SFX parser (Ave robustus! — A.3).
 * Feeds arbitrary bytes through FileFormatJSON::readStream (jsoncpp + the SFXScript
 * interpreter + the Synthesizer) under ASan/UBSan.
 */

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <vector>

/* Local inclusions. */
#include "IO/MemoryStream.hpp"
#include "WaveFactory/FileFormatJSON.hpp"
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
	WaveFactory::FileFormatJSON< int16_t > format;
	WaveFactory::Wave< int16_t > wave;

	(void)format.readStream(stream, wave, {});

	return 0;
}