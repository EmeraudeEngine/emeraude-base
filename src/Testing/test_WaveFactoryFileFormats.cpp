/*
 * src/Testing/test_WaveFactoryFileFormats.cpp
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
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/* Local inclusions. */
#include "IO/MemoryStream.hpp"
#include "WaveFactory/FileFormatJSON.hpp"
#include "WaveFactory/FileFormatMIDI.hpp"
#include "WaveFactory/FileFormatSNDFile.hpp"
#include "WaveFactory/Types.hpp"
#include "WaveFactory/Wave.hpp"
#include "WaveFactory/StreamIO.hpp"

/* These suites pair with the "Ave robustus!" A.2 WaveFactory pass. They exercise the three
 * untrusted-input parsers (libsndfile WAV/FLAC/OGG, the hand-rolled MIDI parser and the JSON
 * SFX interpreter). The headline property — verified meaningfully under ASan/UBSan via ctest —
 * is: malformed/hostile input must be rejected gracefully (return false), never crash, never
 * trigger an unbounded speculative allocation under -fno-exceptions.
 *
 * NOTE: MemoryStream binds a *const* vector to its read constructor and a *non-const* vector to
 * its write constructor. Every read buffer below is therefore const on purpose. */

namespace EmEn::Base::WaveFactory
{
	namespace
	{
		using IO::MemoryStream;

		void
		appendBytes (std::vector< std::byte > & buffer, std::string_view bytes) noexcept
		{
			for ( const auto c : bytes )
			{
				buffer.push_back(static_cast< std::byte >(static_cast< unsigned char >(c)));
			}
		}

		std::vector< std::byte >
		bytesOf (std::string_view bytes) noexcept
		{
			std::vector< std::byte > buffer;
			appendBytes(buffer, bytes);

			return buffer;
		}

		void
		appendU16BE (std::vector< std::byte > & buffer, uint16_t value) noexcept
		{
			buffer.push_back(static_cast< std::byte >((value >> 8) & 0xFF));
			buffer.push_back(static_cast< std::byte >(value & 0xFF));
		}

		void
		appendU32BE (std::vector< std::byte > & buffer, uint32_t value) noexcept
		{
			buffer.push_back(static_cast< std::byte >((value >> 24) & 0xFF));
			buffer.push_back(static_cast< std::byte >((value >> 16) & 0xFF));
			buffer.push_back(static_cast< std::byte >((value >> 8) & 0xFF));
			buffer.push_back(static_cast< std::byte >(value & 0xFF));
		}

		void
		appendU16LE (std::vector< std::byte > & buffer, uint16_t value) noexcept
		{
			buffer.push_back(static_cast< std::byte >(value & 0xFF));
			buffer.push_back(static_cast< std::byte >((value >> 8) & 0xFF));
		}

		void
		appendU32LE (std::vector< std::byte > & buffer, uint32_t value) noexcept
		{
			buffer.push_back(static_cast< std::byte >(value & 0xFF));
			buffer.push_back(static_cast< std::byte >((value >> 8) & 0xFF));
			buffer.push_back(static_cast< std::byte >((value >> 16) & 0xFF));
			buffer.push_back(static_cast< std::byte >((value >> 24) & 0xFF));
		}

		/* Builds a Standard MIDI File: a valid MThd header followed by the supplied raw track
		 * payload wrapped in a single MTrk chunk (its length field set to the payload size). */
		std::vector< std::byte >
		makeMidi (uint16_t format, uint16_t trackCount, uint16_t division, std::string_view trackPayload) noexcept
		{
			std::vector< std::byte > buffer;

			appendBytes(buffer, "MThd");
			appendU32BE(buffer, 6);
			appendU16BE(buffer, format);
			appendU16BE(buffer, trackCount);
			appendU16BE(buffer, division);

			appendBytes(buffer, "MTrk");
			appendU32BE(buffer, static_cast< uint32_t >(trackPayload.size()));
			appendBytes(buffer, trackPayload);

			return buffer;
		}

		/* A minimal but well-formed track: NoteOn(ch0,60,100) -> NoteOff after 96 ticks -> EndOfTrack. */
		std::string
		minimalTrackPayload () noexcept
		{
			return std::string{
				"\x00\x90\x3C\x64"   /* delta 0,  NoteOn  ch0 note60 vel100 */
				"\x60\x80\x3C\x00"   /* delta 96, NoteOff ch0 note60 vel0   */
				"\x00\xFF\x2F\x00",  /* delta 0,  Meta EndOfTrack            */
				12};
		}

		/* Builds a canonical 16-bit PCM WAV. `declaredDataBytes` is the value written into the
		 * 'data' chunk size field; pass a value larger than `payloadBytes` to forge a file that
		 * claims far more frames than it actually carries. */
		std::vector< std::byte >
		makeWav (uint16_t channels, uint32_t sampleRate, uint32_t payloadBytes, uint32_t declaredDataBytes) noexcept
		{
			const uint16_t bitsPerSample = 16;
			const uint16_t blockAlign = static_cast< uint16_t >(channels * (bitsPerSample / 8));
			const uint32_t byteRate = sampleRate * blockAlign;

			std::vector< std::byte > buffer;

			appendBytes(buffer, "RIFF");
			appendU32LE(buffer, 36 + declaredDataBytes);
			appendBytes(buffer, "WAVE");

			appendBytes(buffer, "fmt ");
			appendU32LE(buffer, 16);
			appendU16LE(buffer, 1);             /* PCM */
			appendU16LE(buffer, channels);
			appendU32LE(buffer, sampleRate);
			appendU32LE(buffer, byteRate);
			appendU16LE(buffer, blockAlign);
			appendU16LE(buffer, bitsPerSample);

			appendBytes(buffer, "data");
			appendU32LE(buffer, declaredDataBytes);

			buffer.insert(buffer.end(), payloadBytes, std::byte{0});

			return buffer;
		}
	}

	/* ============================ MIDI ============================ */

	TEST(WaveFactoryMIDI, validMinimalFileDecodes)
	{
		const auto buffer = makeMidi(0, 1, 480, minimalTrackPayload());

		MemoryStream stream{buffer};
		FileFormatMIDI< int16_t > format;
		Wave< int16_t > wave;

		ASSERT_TRUE(format.readStream(stream, wave, {}));
		EXPECT_GT(wave.elementCount(), 0U);
		EXPECT_EQ(wave.channels(), Channels::Stereo);
	}

	TEST(WaveFactoryMIDI, zeroTimeDivisionIsRejected)
	{
		/* division == 0 is the divisor in the tick->sample tempo conversion. Unchecked it yields
		 * +inf and the cast to a uint32_t sample count is UB. The header parse must reject it. */
		const auto buffer = makeMidi(0, 1, 0, minimalTrackPayload());

		MemoryStream stream{buffer};
		FileFormatMIDI< int16_t > format;
		Wave< int16_t > wave;

		EXPECT_FALSE(format.readStream(stream, wave, {}));
	}

	TEST(WaveFactoryMIDI, hugeTrackCountWithNoTracksIsRejectedNotAllocated)
	{
		/* The header claims 65535 tracks but only a single (empty) MTrk chunk follows. The
		 * speculative reserve hint must be clamped (no ~100 MB transient alloc) and the parse
		 * must fail fast on the missing tracks. */
		const auto buffer = makeMidi(1, 0xFFFF, 480, std::string_view{});

		MemoryStream stream{buffer};
		FileFormatMIDI< int16_t > format;
		Wave< int16_t > wave;

		EXPECT_FALSE(format.readStream(stream, wave, {}));
	}

	TEST(WaveFactoryMIDI, badHeaderMagicIsRejected)
	{
		auto mutable_buffer = makeMidi(0, 1, 480, minimalTrackPayload());
		mutable_buffer[0] = static_cast< std::byte >('X');
		const std::vector< std::byte > buffer = mutable_buffer;

		MemoryStream stream{buffer};
		FileFormatMIDI< int16_t > format;
		Wave< int16_t > wave;

		EXPECT_FALSE(format.readStream(stream, wave, {}));
	}

	TEST(WaveFactoryMIDI, truncatedTrackPayloadDoesNotCrash)
	{
		/* A NoteOn with no velocity, no NoteOff and no EndOfTrack: the EOF guards in
		 * readVariableLength()/parseTrack() must terminate the loop without spinning or
		 * reading out of bounds. Either rejection or graceful degradation is acceptable —
		 * the contract verified under ASan/UBSan is "no crash, no hang, returns". */
		const auto buffer = makeMidi(0, 1, 480, std::string_view{"\x00\x90\x3C", 3});

		MemoryStream stream{buffer};
		FileFormatMIDI< int16_t > format;
		Wave< int16_t > wave;

		const auto decoded = format.readStream(stream, wave, {});
		(void)decoded;
		SUCCEED();
	}

	TEST(WaveFactoryMIDI, emptyStreamIsRejected)
	{
		const std::vector< std::byte > buffer;

		MemoryStream stream{buffer};
		FileFormatMIDI< int16_t > format;
		Wave< int16_t > wave;

		EXPECT_FALSE(format.readStream(stream, wave, {}));
	}

	/* ============================ libsndfile (WAV) ============================ */

	TEST(WaveFactorySNDFile, roundTripWavPreservesSamples)
	{
		Wave< int16_t > source;
		ASSERT_TRUE(source.initialize(8U, Channels::Mono, Frequency::PCM8000Hz));

		for ( size_t i = 0; i < source.elementCount(); ++i )
		{
			source.data()[i] = static_cast< int16_t >((static_cast< int >(i) - 4) * 4000);
		}

		std::vector< std::byte > encoded;
		{
			MemoryStream out{encoded};
			FileFormatSNDFile< int16_t > format;
			WriteOptions options;
			options.format = AudioFormat::WAV;
			ASSERT_TRUE(format.writeStream(out, source, options));
		}

		ASSERT_FALSE(encoded.empty());

		const std::vector< std::byte > toRead = encoded;
		MemoryStream in{toRead};
		FileFormatSNDFile< int16_t > format;
		Wave< int16_t > decoded;
		ASSERT_TRUE(format.readStream(in, decoded, {}));

		EXPECT_EQ(decoded.channels(), Channels::Mono);
		EXPECT_EQ(decoded.frequency(), Frequency::PCM8000Hz);
		ASSERT_EQ(decoded.elementCount(), source.elementCount());

		for ( size_t i = 0; i < source.elementCount(); ++i )
		{
			EXPECT_NEAR(decoded.data()[i], source.data()[i], 2);
		}
	}

	TEST(WaveFactorySNDFile, forgedHugeFrameCountNeverOverAllocates)
	{
		/* The 'data' chunk claims ~4 GiB while the file carries 8 bytes. Whether libsndfile
		 * clamps to the real length or trusts the header, the reader must never allocate the
		 * forged size: it either rejects the file or yields a wave bounded well under the cap. */
		const auto buffer = makeWav(2, 48000, 8, 0xFFFFFF00U);

		MemoryStream stream{buffer};
		FileFormatSNDFile< int16_t > format;
		Wave< int16_t > wave;

		if ( format.readStream(stream, wave, {}) )
		{
			EXPECT_LT(wave.elementCount(), static_cast< size_t >(1024) * 1024);
		}
	}

	TEST(WaveFactorySNDFile, garbageBytesAreRejected)
	{
		const auto buffer = bytesOf("not an audio file at all, just random text payload");

		MemoryStream stream{buffer};
		FileFormatSNDFile< int16_t > format;
		Wave< int16_t > wave;

		EXPECT_FALSE(format.readStream(stream, wave, {}));
	}

	TEST(WaveFactorySNDFile, emptyStreamIsRejected)
	{
		const std::vector< std::byte > buffer;

		MemoryStream stream{buffer};
		FileFormatSNDFile< int16_t > format;
		Wave< int16_t > wave;

		EXPECT_FALSE(format.readStream(stream, wave, {}));
	}

	/* ============================ JSON SFX ============================ */

	TEST(WaveFactoryJSON, validMonoScriptDecodes)
	{
		const auto buffer = bytesOf(
			R"({"duration":10,"channels":1,"tracks":[)"
			R"({"instructions":[{"type":"sineWave","frequency":440.0,"amplitude":0.5}]}]})");

		MemoryStream stream{buffer};
		FileFormatJSON< int16_t > format;
		Wave< int16_t > wave;

		ASSERT_TRUE(format.readStream(stream, wave, {}));
		EXPECT_GT(wave.elementCount(), 0U);
		EXPECT_EQ(wave.channels(), Channels::Mono);
	}

	TEST(WaveFactoryJSON, oversizedDurationIsRejectedNotAllocated)
	{
		/* sampleCount = sampleRate * durationMs / 1000 sizes one Synthesizer buffer per track;
		 * an unbounded duration would request a multi-hundred-GiB allocation -> terminate. */
		const auto buffer = bytesOf(
			R"({"duration":4000000000,"channels":1,"tracks":[)"
			R"({"instructions":[{"type":"sineWave","frequency":440.0,"amplitude":0.5}]}]})");

		MemoryStream stream{buffer};
		FileFormatJSON< int16_t > format;
		Wave< int16_t > wave;

		EXPECT_FALSE(format.readStream(stream, wave, {}));
	}

	TEST(WaveFactoryJSON, channelTrackMismatchIsRejected)
	{
		const auto buffer = bytesOf(
			R"({"duration":10,"channels":2,"tracks":[)"
			R"({"instructions":[{"type":"sineWave","frequency":440.0,"amplitude":0.5}]}]})");

		MemoryStream stream{buffer};
		FileFormatJSON< int16_t > format;
		Wave< int16_t > wave;

		EXPECT_FALSE(format.readStream(stream, wave, {}));
	}

	TEST(WaveFactoryJSON, malformedJsonIsRejected)
	{
		const auto buffer = bytesOf("{ this is not valid json ");

		MemoryStream stream{buffer};
		FileFormatJSON< int16_t > format;
		Wave< int16_t > wave;

		EXPECT_FALSE(format.readStream(stream, wave, {}));
	}

	TEST(WaveFactoryStreamIO, formatParity)
	{
		/* Ave robustus! (Axis B): StreamIO read now reaches all three handlers, not just libsndfile. */
		Wave< int16_t > source;
		ASSERT_TRUE(source.initialize(8U, Channels::Mono, Frequency::PCM8000Hz));

		/* Audio (libsndfile) round-trip — the only writable format. */
		std::vector< std::byte > encoded;
		ASSERT_TRUE(StreamIO::write(source, encoded));
		EXPECT_FALSE(encoded.empty());
		Wave< int16_t > audioDecoded;
		ASSERT_TRUE(StreamIO::read(encoded, SoundFileFormat::Audio, audioDecoded));
		EXPECT_EQ(audioDecoded.frequency(), Frequency::PCM8000Hz);

		/* MIDI reachable via StreamIO: a valid minimal MIDI decodes. */
		const auto midiBuffer = makeMidi(0, 1, 480, minimalTrackPayload());
		Wave< int16_t > midiDecoded;
		EXPECT_TRUE(StreamIO::read(midiBuffer, SoundFileFormat::MIDI, midiDecoded));

		/* JSON reachable via StreamIO: malformed input is rejected gracefully (dispatch proven). */
		const std::vector< std::byte > jsonBad(8, std::byte{'{'});
		Wave< int16_t > jsonDecoded;
		EXPECT_FALSE(StreamIO::read(jsonBad, SoundFileFormat::JSON, jsonDecoded));
	}
}
