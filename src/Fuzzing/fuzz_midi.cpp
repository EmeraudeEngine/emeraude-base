/*
 * fuzzing/fuzz_midi.cpp
 * This file is part of Emeraude-Base
 *
 * libFuzzer target for the WaveFactory MIDI parser (Ave robustus! — A.3).
 * Feeds arbitrary bytes through FileFormatMIDI::readStream and lets ASan/UBSan + libFuzzer
 * surface any crash, hang, OOM, or undefined behaviour the manual hardening missed.
 *
 * TinySoundFont's implementation is compiled here (the base library does not provide it —
 * the SF2 render path is odr-used by renderToWave()). See fuzzing/README.md.
 */

#define TSF_IMPLEMENTATION

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <vector>

/* Local inclusions. */
#include "IO/MemoryStream.hpp"
#include "WaveFactory/FileFormatMIDI.hpp"
#include "WaveFactory/Types.hpp"
#include "WaveFactory/Wave.hpp"

extern "C" int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
	using namespace EmEn::Base;

	/* MemoryStream binds a const vector to its read constructor. */
	const std::vector< std::byte > buffer{
		reinterpret_cast< const std::byte * >(data),
		reinterpret_cast< const std::byte * >(data) + size
	};

	IO::MemoryStream stream{buffer};
	WaveFactory::FileFormatMIDI< int16_t > format;
	WaveFactory::Wave< int16_t > wave;

	/* Return value is irrelevant; we are hunting crashes/UB, not correctness here. */
	(void)format.readStream(stream, wave, {});

	return 0;
}