/*
 * src/Testing/test_WaveFactoryDLS.cpp
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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

/* Local inclusions. */
#include "Constants.hpp"
#include "IO/MemoryStream.hpp"
#include "WaveFactory/DLSSoundBank.hpp"
#include "WaveFactory/FileFormatMIDI.hpp"
#include "WaveFactory/Types.hpp"
#include "WaveFactory/Wave.hpp"

/* The RMID embedded DLS bank (2026-10-27, owner decision: a DLS -> SF2 converter): the banks are built here, byte by
 * byte, after the DLS Level 1 layout (no free reference bank exists: the GM bank of Windows is proprietary). */
namespace
{
	using namespace EmEn::Base;
	using namespace EmEn::Base::WaveFactory;

	using Bytes = std::vector< std::byte >;

	void
	appendU16 (Bytes & bytes, uint32_t value)
	{
		bytes.push_back(static_cast< std::byte >(value & 0xFFU));
		bytes.push_back(static_cast< std::byte >((value >> 8U) & 0xFFU));
	}

	void
	appendU32 (Bytes & bytes, uint32_t value)
	{
		appendU16(bytes, value & 0xFFFFU);
		appendU16(bytes, value >> 16U);
	}

	void
	appendTag (Bytes & bytes, std::string_view tag)
	{
		for ( const auto character : tag )
		{
			bytes.push_back(static_cast< std::byte >(character));
		}
	}

	void
	appendBytes (Bytes & bytes, const Bytes & tail)
	{
		bytes.insert(bytes.end(), tail.cbegin(), tail.cend());
	}

	/* A RIFF chunk: its id, its size, its payload, a pad byte when odd. */
	Bytes
	chunk (std::string_view id, const Bytes & payload)
	{
		Bytes bytes;
		appendTag(bytes, id);
		appendU32(bytes, static_cast< uint32_t >(payload.size()));
		appendBytes(bytes, payload);

		if ( (payload.size() & 1U) != 0 )
		{
			bytes.push_back(std::byte{0});
		}

		return bytes;
	}

	/* The payload of a LIST or RIFF chunk: its type, then its children. */
	Bytes
	typed (std::string_view type, const Bytes & children)
	{
		Bytes payload;
		appendTag(payload, type);
		appendBytes(payload, children);

		return payload;
	}

	/* A LIST chunk of a type. */
	Bytes
	listChunk (std::string_view type, const Bytes & children)
	{
		return chunk("LIST", typed(type, children));
	}

	/* A RIFF chunk of a type. */
	Bytes
	riffChunk (std::string_view type, const Bytes & children)
	{
		return chunk("RIFF", typed(type, children));
	}

	/* One absolute articulation connection (no source, no control). */
	struct Connection
	{
		uint16_t destination{0};
		int32_t scale{0};
	};

	Bytes
	articulation (const std::vector< Connection > & connections)
	{
		Bytes art;
		appendU32(art, 8);
		appendU32(art, static_cast< uint32_t >(connections.size()));

		for ( const auto & connection : connections )
		{
			appendU16(art, 0);
			appendU16(art, 0);
			appendU16(art, connection.destination);
			appendU16(art, 0);
			appendU32(art, static_cast< uint32_t >(connection.scale));
		}

		return listChunk("lart", chunk("art1", art));
	}

	/* A 'wsmp': unity note 69, no gain, one forward loop over [0, loopLength) (none when it is 0). */
	Bytes
	waveSample (uint32_t loopLength)
	{
		Bytes wsmp;
		appendU32(wsmp, 20);
		appendU16(wsmp, 69);
		appendU16(wsmp, 0);
		appendU32(wsmp, 0);
		appendU32(wsmp, 0);
		appendU32(wsmp, loopLength > 0 ? 1U : 0U);

		if ( loopLength > 0 )
		{
			appendU32(wsmp, 16);
			appendU32(wsmp, 0);
			appendU32(wsmp, 0);
			appendU32(wsmp, loopLength);
		}

		return chunk("wsmp", wsmp);
	}

	struct TestRegion
	{
		uint32_t wave{0};
		std::vector< Connection > articulation;
	};

	/* A region with no articulation of its own. */
	TestRegion
	plainRegion (uint32_t wave)
	{
		return TestRegion{.wave = wave, .articulation = {}};
	}

	struct TestInstrument
	{
		uint32_t bank{0};
		uint32_t program{0};
		std::vector< TestRegion > regions;
	};

	struct TestWave
	{
		std::vector< int16_t > samples;
		uint16_t channels{1};
	};

	/* A bank: the instruments (every region over every key, unity note 69, its whole wave looped) and the waves. */
	Bytes
	makeBank (const std::vector< TestInstrument > & instruments, const std::vector< TestWave > & waves)
	{
		Bytes instrumentList;

		for ( const auto & instrument : instruments )
		{
			Bytes header;
			appendU32(header, static_cast< uint32_t >(instrument.regions.size()));
			appendU32(header, instrument.bank);
			appendU32(header, instrument.program);

			Bytes regions;

			for ( const auto & region : instrument.regions )
			{
				Bytes regionHeader;
				appendU16(regionHeader, 0);
				appendU16(regionHeader, 127);
				appendU16(regionHeader, 0);
				appendU16(regionHeader, 127);
				appendU16(regionHeader, 0);
				appendU16(regionHeader, 0);

				Bytes link;
				appendU16(link, 0);
				appendU16(link, 0);
				appendU32(link, 1);
				appendU32(link, region.wave);

				const auto waveLength = region.wave < waves.size() ? static_cast< uint32_t >(waves[region.wave].samples.size()) : 0U;

				Bytes body = chunk("rgnh", regionHeader);
				appendBytes(body, waveSample(waveLength));
				appendBytes(body, chunk("wlnk", link));

				if ( !region.articulation.empty() )
				{
					appendBytes(body, articulation(region.articulation));
				}

				appendBytes(regions, listChunk("rgn ", body));
			}

			Bytes body = chunk("insh", header);
			appendBytes(body, listChunk("lrgn", regions));
			appendBytes(instrumentList, listChunk("ins ", body));
		}

		Bytes pool;
		std::vector< uint32_t > cues;

		for ( const auto & wave : waves )
		{
			Bytes format;
			appendU16(format, 1);
			appendU16(format, wave.channels);
			appendU32(format, 44100);
			appendU32(format, 44100U * 2U * wave.channels);
			appendU16(format, static_cast< uint32_t >(2U * wave.channels));
			appendU16(format, 16);

			Bytes data;

			for ( const auto sample : wave.samples )
			{
				appendU16(data, static_cast< uint16_t >(sample));
			}

			Bytes body = chunk("fmt ", format);
			appendBytes(body, chunk("data", data));

			cues.push_back(static_cast< uint32_t >(pool.size()));
			appendBytes(pool, listChunk("wave", body));
		}

		Bytes poolTable;
		appendU32(poolTable, 8);
		appendU32(poolTable, static_cast< uint32_t >(cues.size()));

		for ( const auto cue : cues )
		{
			appendU32(poolTable, cue);
		}

		Bytes collection;
		appendU32(collection, static_cast< uint32_t >(instruments.size()));

		Bytes body = chunk("colh", collection);
		appendBytes(body, listChunk("lins", instrumentList));
		appendBytes(body, chunk("ptbl", poolTable));
		appendBytes(body, listChunk("wvpl", pool));

		return riffChunk("DLS ", body);
	}

	/* One period of a sine over 100 samples: 441 Hz at 44.1 kHz. */
	TestWave
	sinePeriod ()
	{
		TestWave wave;

		for ( int index = 0; index < 100; ++index )
		{
			wave.samples.push_back(static_cast< int16_t >(16000.0 * std::sin(2.0 * std::numbers::pi * index / 100.0)));
		}

		return wave;
	}

	using SoundFont = std::unique_ptr< tsf, decltype(&tsf_close) >;

	SoundFont
	load (const Bytes & soundFont)
	{
		return SoundFont{tsf_load_memory(soundFont.data(), static_cast< int >(soundFont.size())), &tsf_close};
	}

	struct Note
	{
		int preset{0};
		int key{69};
		float seconds{1.0F};
	};

	/* Renders a note at 44.1 kHz mono. */
	std::vector< float >
	renderNote (tsf * soundFont, const Note & note)
	{
		tsf_set_output(soundFont, TSF_MONO, 44100, 0.0F);
		tsf_note_on(soundFont, note.preset, note.key, 1.0F);

		std::vector< float > output(static_cast< size_t >(44100.0F * note.seconds));
		tsf_render_float(soundFont, output.data(), static_cast< int >(output.size()), 0);

		return output;
	}

	size_t
	risingCrossings (const std::vector< float > & samples)
	{
		size_t count = 0;

		for ( size_t index = 1; index < samples.size(); ++index )
		{
			count += samples[index - 1] < 0.0F && samples[index] >= 0.0F ? 1U : 0U;
		}

		return count;
	}

	float
	rms (std::span< const float > samples)
	{
		double sum = 0.0;

		for ( const auto sample : samples )
		{
			sum += static_cast< double >(sample) * static_cast< double >(sample);
		}

		return samples.empty() ? 0.0F : static_cast< float >(std::sqrt(sum / static_cast< double >(samples.size())));
	}

	/* An RMID: one note (69) held 480 ticks, then the bank (when given) as a nested RIFF 'DLS '. */
	Bytes
	makeRMID (const Bytes & bank)
	{
		Bytes track;
		for ( const int value : {0x00, 0x90, 0x45, 0x64,  0x83, 0x60, 0x80, 0x45, 0x00,  0x00, 0xFF, 0x2F, 0x00} )
		{
			track.push_back(static_cast< std::byte >(value));
		}

		Bytes smf;
		appendTag(smf, "MThd");
		for ( const int value : {0, 0, 0, 6,  0, 0,  0, 1,  0x01, 0xE0} )
		{
			smf.push_back(static_cast< std::byte >(value));
		}
		appendTag(smf, "MTrk");
		for ( const int shift : {24, 16, 8, 0} )
		{
			smf.push_back(static_cast< std::byte >((track.size() >> shift) & 0xFFU));
		}
		appendBytes(smf, track);

		Bytes body = chunk("data", smf);
		appendBytes(body, bank);

		return riffChunk("RMID", body);
	}

	bool
	decodeRMID (const Bytes & file, Wave< int16_t > & wave, tsf * soundFont = nullptr)
	{
		IO::MemoryStream stream{file};
		FileFormatMIDI< int16_t > format;
		ReadOptions options{};
		options.synthesisFrequency = Frequency::PCM22050Hz;
		options.soundfont = soundFont;

		return format.readStream(stream, wave, options);
	}

	bool
	isSilent (const Wave< int16_t > & wave)
	{
		return std::ranges::all_of(wave.data(), [] (int16_t sample) {
			return sample == 0;
		});
	}
}

TEST(WaveFactoryDLS, aBankConvertsToALoadableSoundFontThatPlaysItsWave)
{
	const auto bank = makeBank({{.bank = 0, .program = 5, .regions = {plainRegion(0)}}}, {sinePeriod()});
	const auto soundFont = DLS::convertToSoundFont(bank);

	ASSERT_TRUE(soundFont.has_value());

	const auto loaded = load(soundFont.value_or(Bytes{}));
	ASSERT_NE(loaded, nullptr);
	EXPECT_EQ(tsf_get_presetcount(loaded.get()), 1);

	const auto preset = tsf_get_presetindex(loaded.get(), 0, 5);
	ASSERT_GE(preset, 0);

	/* The unity note (69) plays the looped period at its own pitch: 441 periods in one second. */
	const auto output = renderNote(loaded.get(), {.preset = preset, .key = 69, .seconds = 1.0F});

	EXPECT_NEAR(static_cast< double >(risingCrossings(output)), 441.0, 3.0);
	EXPECT_GT(rms(output), 0.01F);

	/* An octave up doubles it. */
	tsf_reset(loaded.get());
	const auto octave = renderNote(loaded.get(), {.preset = preset, .key = 81, .seconds = 1.0F});
	EXPECT_NEAR(static_cast< double >(risingCrossings(octave)), 882.0, 6.0);
}

TEST(WaveFactoryDLS, aDrumInstrumentIsTheSoundFontPercussionBank)
{
	const auto bank = makeBank({{.bank = 0x80000000U, .program = 0, .regions = {plainRegion(0)}}}, {sinePeriod()});
	const auto soundFont = DLS::convertToSoundFont(bank);

	ASSERT_TRUE(soundFont.has_value());

	const auto loaded = load(soundFont.value_or(Bytes{}));
	ASSERT_NE(loaded, nullptr);
	EXPECT_GE(tsf_get_presetindex(loaded.get(), 128, 0), 0);
	EXPECT_LT(tsf_get_presetindex(loaded.get(), 0, 0), 0);
}

TEST(WaveFactoryDLS, theArticulationAttackShapesTheVolumeEnvelope)
{
	/* EG1 attack 1 s (0 timecents): the first 50 ms are far quieter than without an articulation. */
	const auto plain = DLS::convertToSoundFont(makeBank({{.regions = {plainRegion(0)}}}, {sinePeriod()}));
	const auto slow = DLS::convertToSoundFont(makeBank({{.regions = {{.wave = 0, .articulation = {{.destination = 0x0206, .scale = 0}}}}}}, {sinePeriod()}));

	ASSERT_TRUE(plain.has_value());
	ASSERT_TRUE(slow.has_value());

	const auto plainFont = load(plain.value_or(Bytes{}));
	const auto slowFont = load(slow.value_or(Bytes{}));
	ASSERT_NE(plainFont, nullptr);
	ASSERT_NE(slowFont, nullptr);

	const auto plainOutput = renderNote(plainFont.get(), {.preset = 0, .key = 69, .seconds = 0.05F});
	const auto slowOutput = renderNote(slowFont.get(), {.preset = 0, .key = 69, .seconds = 0.05F});

	EXPECT_LT(rms(slowOutput), rms(plainOutput) * 0.25F);
}

TEST(WaveFactoryDLS, malformedOrUnplayableBanksAreRefused)
{
	const auto good = makeBank({{.regions = {plainRegion(0)}}}, {sinePeriod()});

	/* Not a DLS, truncated, empty. */
	EXPECT_FALSE(DLS::convertToSoundFont(Bytes{}).has_value());
	EXPECT_FALSE(DLS::convertToSoundFont(std::span< const std::byte >{good}.first(good.size() / 2)).has_value());

	Bytes notDLS = good;
	notDLS[8] = std::byte{'X'};
	EXPECT_FALSE(DLS::convertToSoundFont(notDLS).has_value());

	/* A region linked past the pool, and a stereo wave: no playable region left. */
	EXPECT_FALSE(DLS::convertToSoundFont(makeBank({{.regions = {plainRegion(7)}}}, {sinePeriod()})).has_value());

	auto stereo = sinePeriod();
	stereo.channels = 2;
	EXPECT_FALSE(DLS::convertToSoundFont(makeBank({{.regions = {plainRegion(0)}}}, {stereo})).has_value());

	/* A size field pointing past the end anywhere: refused, never read past the end. */
	for ( size_t offset = 4; offset + 4 <= good.size(); offset += 4 )
	{
		Bytes corrupt = good;
		corrupt[offset] = std::byte{0xFF};
		corrupt[offset + 1] = std::byte{0xFF};
		corrupt[offset + 2] = std::byte{0xFF};
		corrupt[offset + 3] = std::byte{0x7F};

		static_cast< void >(DLS::convertToSoundFont(corrupt));
	}
}

TEST(WaveFactoryDLS, anRMIDPlaysWithItsEmbeddedBankFirst)
{
	/* A SILENT bank (its one wave all zeros): the RMID renders silence with it — even with a global SoundFont given —
	 * and sound without it (the additive synthesizer), which proves the embedded bank is the one played. */
	TestWave silence;
	silence.samples.assign(100, 0);

	const auto silentBank = makeBank({{.regions = {plainRegion(0)}}}, {silence});

	Wave< int16_t > withBank;
	ASSERT_TRUE(decodeRMID(makeRMID(silentBank), withBank));
	EXPECT_TRUE(isSilent(withBank));

	const SoundFont global{tsf_load_filename(TestSoundFont.string().c_str()), &tsf_close};
	ASSERT_NE(global, nullptr) << TestSoundFont;

	Wave< int16_t > overGlobal;
	ASSERT_TRUE(decodeRMID(makeRMID(silentBank), overGlobal, global.get()));
	EXPECT_TRUE(isSilent(overGlobal));

	Wave< int16_t > withoutBank;
	ASSERT_TRUE(decodeRMID(makeRMID({}), withoutBank));
	EXPECT_FALSE(isSilent(withoutBank));

	/* A malformed embedded bank: reported, and the file still renders (the additive synthesizer here). */
	Bytes broken = silentBank;
	broken.resize(broken.size() / 2);

	Wave< int16_t > withBrokenBank;
	ASSERT_TRUE(decodeRMID(makeRMID(broken), withBrokenBank));
	EXPECT_FALSE(isSilent(withBrokenBank));
}
