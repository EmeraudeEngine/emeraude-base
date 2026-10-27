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
#include "tsf.h"

/* STL inclusions. */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Constants.hpp"
#include "IO/MemoryStream.hpp"
#include "WaveFactory/FileFormatJSON.hpp"
#include "WaveFactory/FileFormatMIDI.hpp"
#include "WaveFactory/FileFormatSNDFile.hpp"
#include "WaveFactory/Types.hpp"
#include "WaveFactory/Wave.hpp"
#include "WaveFactory/StreamIO.hpp"
#include "WaveFactory/Synthesizer.hpp"

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
		MemoryStream stream{std::as_const(mutable_buffer)};
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

	/* The coverage pass (2026-10-17): the tempo map, the duration ceiling (owner decision: 30 minutes for both
	 * renderers — a 36-byte file can declare any duration), the header refusals, the RMID wrapper, every channel event
	 * and both renderers. */
	namespace
	{
		/* The bytes of a track, as given. */
		std::string
		midiBytes (std::initializer_list< int > values) noexcept
		{
			std::string bytes;

			for ( const auto value : values )
			{
				bytes.push_back(static_cast< char >(value & 0xFF));
			}

			return bytes;
		}

		/* A MIDI variable-length quantity. */
		std::string
		midiDelta (uint32_t ticks) noexcept
		{
			std::string bytes(1, static_cast< char >(ticks & 0x7FU));

			for ( uint32_t rest = ticks >> 7U; rest != 0; rest >>= 7U )
			{
				bytes.insert(bytes.begin(), static_cast< char >((rest & 0x7FU) | 0x80U));
			}

			return bytes;
		}

		/* One note (channel 0, note 60) held for the given ticks, then the end of the track. */
		std::string
		heldNoteTrack (uint32_t ticks) noexcept
		{
			return midiBytes({0x00, 0x90, 0x3C, 0x64}) + midiDelta(ticks) + midiBytes({0x80, 0x3C, 0x00,  0x00, 0xFF, 0x2F, 0x00});
		}

		/* Decodes a MIDI file at 4 kHz (small buffers), with an optional SoundFont. */
		bool
		decodeMidi (const std::vector< std::byte > & file, Wave< int16_t > & wave, tsf * soundfont = nullptr, const std::stop_token & stopToken = {}) noexcept
		{
			MemoryStream stream{file};
			FileFormatMIDI< int16_t > format;
			ReadOptions options{};
			options.synthesisFrequency = Frequency::PCM4000Hz;
			options.soundfont = soundfont;
			options.stopToken = stopToken;

			return format.readStream(stream, wave, options);
		}

		/* A note as the polyphony bound sees it. */
		struct TestNote
		{
			uint32_t startTick{0};
			uint32_t endTick{0};
		};

		/* The tick every staggered note is held to, and the length of a back-to-back note. */
		constexpr uint32_t HeldUntilTick{10000};
		constexpr uint32_t BackToBackLength{10};

		/* `count` notes, one per tick, all held to HeldUntilTick. */
		std::vector< TestNote >
		staggeredHeldNotes (uint32_t count) noexcept
		{
			std::vector< TestNote > notes(count);

			for ( uint32_t index = 0; index < count; ++index )
			{
				notes[index] = TestNote{.startTick = index, .endTick = HeldUntilTick};
			}

			return notes;
		}

		/* `count` notes of BackToBackLength ticks, each starting where the previous one ends. */
		std::vector< TestNote >
		backToBackNotes (uint32_t count) noexcept
		{
			std::vector< TestNote > notes(count);

			for ( uint32_t index = 0; index < count; ++index )
			{
				notes[index] = TestNote{.startTick = index * BackToBackLength, .endTick = (index + 1) * BackToBackLength};
			}

			return notes;
		}

		/* The most notes sounding at once over [0, lastTick). */
		template< typename note_t >
		size_t
		loudestPolyphony (const std::vector< note_t > & notes, uint32_t lastTick) noexcept
		{
			size_t loudest = 0;

			for ( uint32_t tick = 0; tick < lastTick; ++tick )
			{
				loudest = std::max(loudest, static_cast< size_t >(std::ranges::count_if(notes, [tick] (const note_t & note) { return note.startTick <= tick && tick < note.endTick; })));
			}

			return loudest;
		}

		/* Whether any sample is not zero. */
		bool
		isAudible (const Wave< int16_t > & wave) noexcept
		{
			return std::ranges::any_of(wave.data(), [] (int16_t sample) { return sample != 0; });
		}

		/* A track using every channel event the parser knows: a program per channel (every instrument family), the
		 * controllers it maps and two it passes through or ignores, both aftertouches, a pitch bend, running status, a
		 * note-on of velocity 0 as a note-off, SysEx (F0 and F7), a text meta event, a drum note, and two successive
		 * notes on channel 0 with portamento on. */
		std::string
		everyEventTrack () noexcept
		{
			std::string track = midiBytes({0x00, 0xFF, 0x01, 0x04, 'E', 'M', 'E', 'N',  0x00, 0xF0, 0x02, 0x7E, 0xF7,  0x00, 0xF7, 0x01, 0x00});

			for ( int channel = 0; channel < 16; ++channel )
			{
				if ( channel == 9 )
				{
					continue;
				}

				track += midiBytes({0x00, 0xC0 | channel, channel * 8,  0x00, 0xB0 | channel, 0x07, 0x64,  0x00, 0x90 | channel, 0x30 + channel, 0x64});
			}

			track += midiBytes({
				0x00, 0xB0, 0x01, 0x40,  0x00, 0x05, 0x40,  0x00, 0x0B, 0x64,  0x00, 0x40, 0x7F,  0x00, 0x41, 0x7F,  /* running status */
				0x00, 0x47, 0x40,  0x00, 0x4A, 0x40,  0x00, 0x5C, 0x40,  0x00, 0x0A, 0x30,  0x00, 0x00, 0x00,  0x00, 0x79, 0x00,  0x00, 0x32, 0x10,
				0x00, 0xA0, 0x30, 0x40,  0x00, 0xD0, 0x50,  0x00, 0xE0, 0x00, 0x50,
				0x00, 0x99, 0x24, 0x64
			});

			for ( int channel = 0; channel < 16; ++channel )
			{
				if ( channel == 9 )
				{
					continue;
				}

				/* Half the notes end with a note-off, half with a note-on of velocity 0. */
				track += midiDelta(channel == 0 ? 960U : 0U) + midiBytes({(channel % 2 == 0 ? 0x80 : 0x90) | channel, 0x30 + channel, 0x00});
			}

			track += midiBytes({0x00, 0x89, 0x24, 0x00,  0x00, 0x90, 0x37, 0x64,  0x83, 0x60, 0x80, 0x37, 0x00,  0x00, 0xFF, 0x2F, 0x00});

			return track;
		}
	}

	TEST(WaveFactoryMIDI, lengthFollowsTheTempoMap)
	{
		/* 57 600 ticks at 480 per quarter and the default 500 000 us per quarter = 60 s, plus the additive renderer's
		 * tail (division / 2 = 240 ticks = 0.25 s): 60.25 s at 4 kHz. */
		{
			Wave< int16_t > wave;

			ASSERT_TRUE(decodeMidi(makeMidi(0, 1, 480, heldNoteTrack(57600)), wave));
			EXPECT_EQ(wave.sampleCount(), 241000U);
		}

		/* The tempo halved at tick 28 800 (30 s): the remaining 28 800 + 240 ticks last 15.125 s. */
		{
			const auto track = midiBytes({0x00, 0x90, 0x3C, 0x64}) + midiDelta(28800) + midiBytes({0xFF, 0x51, 0x03, 0x03, 0xD0, 0x90}) +
				midiDelta(28800) + midiBytes({0x80, 0x3C, 0x00,  0x00, 0xFF, 0x2F, 0x00});
			Wave< int16_t > wave;

			ASSERT_TRUE(decodeMidi(makeMidi(0, 1, 480, track), wave));
			EXPECT_EQ(wave.sampleCount(), 180500U);
		}
	}

	TEST(WaveFactoryMIDI, declaredDurationBeyondTheLimitIsTruncated)
	{
		/* One 4-byte delta (2^28 - 1 ticks, ~77 hours): the rendering stops at 30 minutes. It used to convert ~1.3e10
		 * samples to uint32_t (undefined behaviour), and below that limit the additive renderer allocated without bound
		 * (2 hours: 6 GB, then std::bad_alloc -> abort). */
		const auto file = makeMidi(0, 1, 480, heldNoteTrack(0x0FFFFFFFU));
		constexpr auto ThirtyMinutes = static_cast< size_t >(30 * 60 * 4000);

		{
			Wave< int16_t > wave;

			ASSERT_TRUE(decodeMidi(file, wave));
			EXPECT_EQ(wave.sampleCount(), ThirtyMinutes);
		}

		{
			const std::unique_ptr< tsf, decltype(&tsf_close) > soundfont{tsf_load_filename(TestSoundFont.string().c_str()), &tsf_close};
			ASSERT_NE(soundfont, nullptr) << TestSoundFont;

			Wave< int16_t > wave;

			ASSERT_TRUE(decodeMidi(file, wave, soundfont.get()));
			EXPECT_EQ(wave.sampleCount(), ThirtyMinutes);
		}
	}

	/* The rendering is stoppable and the additive renderer's polyphony bounded (owner decision 2026-10-27,
	 * wavefactory doc 06): 256 voices, the oldest stolen, as TinySoundFont. */
	TEST(WaveFactoryMIDI, aRequestedStopRefusesBothRenderers)
	{
		const auto file = makeMidi(0, 1, 480, heldNoteTrack(4800));
		std::stop_source stop;

		stop.request_stop();

		Wave< int16_t > additive;

		EXPECT_FALSE(decodeMidi(file, additive, nullptr, stop.get_token()));

		const std::unique_ptr< tsf, decltype(&tsf_close) > soundfont{tsf_load_filename(TestSoundFont.string().c_str()), &tsf_close};
		ASSERT_NE(soundfont, nullptr) << TestSoundFont;

		Wave< int16_t > sampled;

		EXPECT_FALSE(decodeMidi(file, sampled, soundfont.get(), stop.get_token()));
	}

	TEST(WaveFactoryMIDI, theAdditiveRendererStealsTheOldestVoiceBeyondItsPolyphony)
	{
		static_assert(FileFormatMIDI< int16_t >::MaxAdditiveVoices == 256);

		/* 300 notes, one per tick, all held to tick 10 000: from the 257th on, each start steals the oldest voice. */
		auto notes = staggeredHeldNotes(300);

		EXPECT_EQ(FileFormatMIDI< int16_t >::stealOldestVoices(notes), 44U);
		EXPECT_EQ(notes[0].endTick, 256U);
		EXPECT_EQ(notes[43].endTick, 299U);
		EXPECT_EQ(notes[44].endTick, 10000U);

		/* Never more than 256 voices sound at once. */
		EXPECT_EQ(loudestPolyphony(notes, 300), 256U);

		/* Notes that do not overlap are never stolen. */
		auto sequence = backToBackNotes(1000);

		EXPECT_EQ(FileFormatMIDI< int16_t >::stealOldestVoices(sequence), 0U);
	}

	/* The measured stop latency of the additive renderer: four notes held 30 minutes at 48 kHz (~14 s of CPU unstopped), a
	 * stop requested from another thread 100 ms in. Disabled: a measurement, run by hand. */
	TEST(WaveFactoryMIDI, DISABLED_AdditiveRenderStopLatency)
	{
		std::string track;

		for ( int note = 0; note < 4; ++note )
		{
			track += midiBytes({0x00, 0x90, 0x3C + note, 0x64});
		}

		track += midiDelta(0x0FFFFFFFU) + midiBytes({0x80, 0x3C, 0x00,  0x00, 0x80, 0x3D, 0x00,  0x00, 0x80, 0x3E, 0x00,  0x00, 0x80, 0x3F, 0x00,  0x00, 0xFF, 0x2F, 0x00});

		const auto file = makeMidi(0, 1, 480, track);
		std::stop_source stop;
		std::chrono::steady_clock::time_point requestedAt;

		std::jthread stopper{[&stop, &requestedAt] {
			std::this_thread::sleep_for(std::chrono::milliseconds{100});
			requestedAt = std::chrono::steady_clock::now();
			static_cast< void >(stop.request_stop());
		}};

		MemoryStream stream{file};
		FileFormatMIDI< int16_t > format;
		ReadOptions options{};
		options.stopToken = stop.get_token();
		Wave< int16_t > wave;

		const auto result = format.readStream(stream, wave, options);
		const auto returnedAt = std::chrono::steady_clock::now();

		stopper.join();

		EXPECT_FALSE(result);

		const auto latency = std::chrono::duration_cast< std::chrono::microseconds >(returnedAt - requestedAt).count();

		std::cout << "[latency] additive MIDI render stop: " << latency << " us\n";
		EXPECT_LT(latency, 50'000);
	}

	TEST(WaveFactoryMIDI, headerRefusals)
	{
		Wave< int16_t > wave;

		/* SMPTE time division (bit 15). */
		EXPECT_FALSE(decodeMidi(makeMidi(0, 1, 0xE728, minimalTrackPayload()), wave));

		/* A header length other than 6. */
		{
			auto file = makeMidi(0, 1, 480, minimalTrackPayload());
			file[7] = std::byte{7};

			EXPECT_FALSE(decodeMidi(std::as_const(file), wave));
		}

		/* A header cut in the middle of its fields. */
		{
			auto file = makeMidi(0, 1, 480, minimalTrackPayload());
			file.resize(10);

			EXPECT_FALSE(decodeMidi(std::as_const(file), wave));
		}

		/* A track chunk that is not "MTrk". */
		{
			auto file = makeMidi(0, 1, 480, minimalTrackPayload());
			file[14] = std::byte{'X'};

			EXPECT_FALSE(decodeMidi(std::as_const(file), wave));
		}

		/* A track without a single note. */
		EXPECT_FALSE(decodeMidi(makeMidi(0, 1, 480, std::string_view{"\x00\xFF\x2F\x00", 4}), wave));
	}

	TEST(WaveFactoryMIDI, rmidWrapperDecodesLikeThePlainFile)
	{
		const auto smf = makeMidi(0, 1, 480, minimalTrackPayload());

		std::vector< std::byte > rmid;
		appendBytes(rmid, "RIFF");
		appendU32LE(rmid, static_cast< uint32_t >(12 + smf.size()));
		appendBytes(rmid, "RMIDdata");
		appendU32LE(rmid, static_cast< uint32_t >(smf.size()));
		rmid.insert(rmid.end(), smf.cbegin(), smf.cend());

		Wave< int16_t > plain;
		Wave< int16_t > wrapped;

		ASSERT_TRUE(decodeMidi(smf, plain));
		ASSERT_TRUE(decodeMidi(std::as_const(rmid), wrapped));
		EXPECT_EQ(wrapped.sampleCount(), plain.sampleCount());
	}

	TEST(WaveFactoryMIDI, writeIsRefused)
	{
		Wave< int16_t > wave;
		ASSERT_TRUE(decodeMidi(makeMidi(0, 1, 480, minimalTrackPayload()), wave));

		std::vector< std::byte > output;
		MemoryStream stream{output};
		const FileFormatMIDI< int16_t > format;

		EXPECT_FALSE(format.writeStream(stream, wave, {}));
	}

	TEST(WaveFactoryMIDI, everyChannelEventRendersWithBothRenderers)
	{
		const auto file = makeMidi(0, 1, 480, everyEventTrack());

		/* Additive synthesis: 960 + 480 ticks, plus 240 of tail = 1.75 s. */
		{
			Wave< int16_t > wave;

			ASSERT_TRUE(decodeMidi(file, wave));
			EXPECT_EQ(wave.sampleCount(), 7000U);
			EXPECT_TRUE(isAudible(wave));
		}

		/* The SoundFont: the same events through TinySoundFont (480 ticks of tail = 2 s). */
		{
			const std::unique_ptr< tsf, decltype(&tsf_close) > soundfont{tsf_load_filename(TestSoundFont.string().c_str()), &tsf_close};
			ASSERT_NE(soundfont, nullptr) << TestSoundFont;

			Wave< int16_t > wave;

			ASSERT_TRUE(decodeMidi(file, wave, soundfont.get()));
			EXPECT_EQ(wave.sampleCount(), 8000U);
			EXPECT_TRUE(isAudible(wave));
		}
	}

	TEST(WaveFactoryMIDI, resonantFilterStaysBoundedOverTheWholeControllerRange)
	{
		/* The additive renderer's filter: every CC#71 (resonance) x CC#74 (cutoff) pair, 2 s of a 220 Hz saw of
		 * amplitude 0.25 at 48 kHz. It diverged to infinity from CC#71 = 40 (the amount reached 3.8; its source defines
		 * it between 0 and 1); bounded now (4.15 at worst in the simulation). */
		using Synth = Synthesizer< int16_t >;

		constexpr float SampleRate = 48000.0F;
		float worstPeak = 0.0F;

		for ( int16_t cutoff = 0; cutoff < 128; cutoff += 8 )
		{
			for ( int16_t resonance = 0; resonance < 128; resonance += 8 )
			{
				const auto coefficient = Synth::filterCutoffToCoefficient(cutoff, SampleRate);
				const auto amount = Synth::filterResonanceToFeedback(resonance);
				Synth::FilterState state{};
				float phase = 0.0F;

				for ( int index = 0; index < 96000; ++index )
				{
					const float output = Synth::applyResonantFilterSample(0.25F * ((2.0F * phase) - 1.0F), coefficient, amount, state);
					phase = std::fmod(phase + (220.0F / SampleRate), 1.0F);

					ASSERT_TRUE(std::isfinite(output)) << "CC#74 " << cutoff << ", CC#71 " << resonance;

					worstPeak = std::max(worstPeak, std::abs(output));
				}
			}
		}

		EXPECT_LT(worstPeak, 8.0F);
	}

	TEST(WaveFactoryMIDI, panHardLeftLeavesTheRightChannelSilent)
	{
		/* CC#10 = 0 on channel 0, then a note (additive renderer: right gain = sin(0)). */
		const auto track = midiBytes({0x00, 0xB0, 0x0A, 0x00}) + heldNoteTrack(480);
		Wave< int16_t > wave;

		ASSERT_TRUE(decodeMidi(makeMidi(0, 1, 480, track), wave));

		const auto & samples = wave.data();
		bool leftAudible = false;
		bool rightAudible = false;

		for ( size_t index = 0; index + 1 < samples.size(); index += 2 )
		{
			leftAudible = leftAudible || samples[index] != 0;
			rightAudible = rightAudible || samples[index + 1] != 0;
		}

		EXPECT_TRUE(leftAudible);
		EXPECT_FALSE(rightAudible);
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

		MemoryStream in{std::as_const(encoded)};
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

	/* The decode is stoppable (Ave Robustus II, docs/subsystems/wavefactory/07, 2026-10-27): a file longer than one
	 * decode block, read whole, then read with a stop already requested. */
	namespace
	{
		std::vector< std::byte >
		encodedRamp (size_t sampleCount)
		{
			Wave< int16_t > source;

			if ( !source.initialize(sampleCount, Channels::Mono, Frequency::PCM8000Hz) )
			{
				return {};
			}

			for ( size_t i = 0; i < source.elementCount(); ++i )
			{
				source.data()[i] = static_cast< int16_t >((static_cast< int >(i % 2000) - 1000) * 30);
			}

			std::vector< std::byte > encoded;
			MemoryStream out{encoded};
			const FileFormatSNDFile< int16_t > format;
			WriteOptions options;
			options.format = AudioFormat::WAV;

			if ( !format.writeStream(out, source, options) )
			{
				return {};
			}

			return encoded;
		}
	}

	TEST(WaveFactorySNDFile, aDecodeLongerThanOneBlockKeepsEverySample)
	{
		constexpr size_t SampleCount{50'000};
		const auto encoded = encodedRamp(SampleCount);

		ASSERT_FALSE(encoded.empty());

		MemoryStream in{encoded};
		FileFormatSNDFile< int16_t > format;
		Wave< int16_t > decoded;

		ASSERT_TRUE(format.readStream(in, decoded, {}));
		ASSERT_EQ(decoded.elementCount(), SampleCount);

		size_t mismatches = 0;

		for ( size_t i = 0; i < SampleCount; ++i )
		{
			const auto expected = (static_cast< int >(i % 2000) - 1000) * 30;

			mismatches += std::abs(static_cast< int >(decoded.data()[i]) - expected) > 2 ? 1U : 0U;
		}

		EXPECT_EQ(mismatches, 0U);
	}

	TEST(WaveFactorySNDFile, aRequestedStopRefusesTheDecode)
	{
		const auto encoded = encodedRamp(50'000);

		ASSERT_FALSE(encoded.empty());

		std::stop_source stop;
		stop.request_stop();

		MemoryStream in{encoded};
		FileFormatSNDFile< int16_t > format;
		Wave< int16_t > decoded;
		ReadOptions options;
		options.stopToken = stop.get_token();

		EXPECT_FALSE(format.readStream(in, decoded, options));
	}
}
