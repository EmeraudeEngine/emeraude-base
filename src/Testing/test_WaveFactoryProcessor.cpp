/*
 * src/Testing/test_WaveFactoryProcessor.cpp
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
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

/* Local inclusions. */
#include "WaveFactory/Processor.hpp"
#include "WaveFactory/Wave.hpp"

/* The coverage pass (2026-10-17): every operation of the wave processor, the engine's mix-down / resample path
 * (Audio::SoundResource, MusicResource) included, checked against known values. */
namespace
{
	using namespace EmEn::Base::WaveFactory;

	/* A sine of the given frequency (Hz) and amplitude, `seconds` long, the same on every channel. */
	Wave< float >
	sine (float hertz, float seconds, Frequency rate, Channels channels, float amplitude)
	{
		const auto sampleRate = static_cast< float >(rate);
		const auto sampleCount = static_cast< size_t >(seconds * sampleRate);
		const auto channelCount = static_cast< size_t >(channels);
		std::vector< float > data(sampleCount * channelCount);

		for ( size_t sample = 0; sample < sampleCount; ++sample )
		{
			const auto value = amplitude * std::sin(2.0F * std::numbers::pi_v< float > * hertz * static_cast< float >(sample) / sampleRate);

			for ( size_t channel = 0; channel < channelCount; ++channel )
			{
				data[(sample * channelCount) + channel] = value;
			}
		}

		Wave< float > wave;
		static_cast< void >(wave.initialize(data, channels, rate));

		return wave;
	}

	/* A wave built from its samples. */
	Wave< float >
	waveOf (const std::vector< float > & data, Channels channels, Frequency rate = Frequency::PCM48000Hz)
	{
		Wave< float > wave;
		static_cast< void >(wave.initialize(data, channels, rate));

		return wave;
	}

	/* The rising zero crossings of channel 0: twice the sine's period count, minus edges. */
	size_t
	risingCrossings (const Wave< float > & wave)
	{
		const auto channelCount = static_cast< size_t >(wave.channels());
		const auto & data = wave.data();
		size_t count = 0;

		for ( size_t sample = 1; sample < wave.sampleCount(); ++sample )
		{
			if ( data[(sample - 1) * channelCount] < 0.0F && data[sample * channelCount] >= 0.0F )
			{
				++count;
			}
		}

		return count;
	}
}

TEST(WaveFactoryProcessor, mixDownAveragesTheChannels)
{
	Processor processor{waveOf({0.5F, -0.25F, 0.5F, -0.25F}, Channels::Stereo)};

	ASSERT_TRUE(processor.mixDown());
	ASSERT_EQ(processor.wave().channels(), Channels::Mono);
	EXPECT_EQ(processor.wave().data(), (std::vector< float >{0.125F, 0.125F}));

	/* A mono wave is left as is. */
	EXPECT_TRUE(processor.mixDown());
	EXPECT_EQ(processor.wave().sampleCount(), 2U);
}

TEST(WaveFactoryProcessor, resampleKeepsTheDurationAndThePitch)
{
	/* One second of 440 Hz at 48 kHz, to 22.05 kHz: ~22 050 samples, still 440 periods. */
	Processor processor{sine(440.0F, 1.0F, Frequency::PCM48000Hz, Channels::Stereo, 0.5F)};

	ASSERT_TRUE(processor.resample(Frequency::PCM22050Hz));
	EXPECT_EQ(processor.wave().frequency(), Frequency::PCM22050Hz);
	EXPECT_EQ(processor.wave().channels(), Channels::Stereo);
	EXPECT_NEAR(static_cast< double >(processor.wave().sampleCount()), 22050.0, 2.0);
	EXPECT_NEAR(static_cast< double >(risingCrossings(processor.wave())), 440.0, 2.0);

	/* The same frequency: nothing to do. */
	const auto before = processor.wave().sampleCount();
	EXPECT_TRUE(processor.resample(Frequency::PCM22050Hz));
	EXPECT_EQ(processor.wave().sampleCount(), before);
}

TEST(WaveFactoryProcessor, structuralOperations)
{
	/* trim: the silent edges go, not the silence inside. */
	{
		Processor processor{waveOf({0.0F, 0.0F, 0.5F, 0.0F, -0.5F, 0.0F}, Channels::Mono)};

		ASSERT_TRUE(processor.trim(-60.0F));
		EXPECT_EQ(processor.wave().data(), (std::vector< float >{0.5F, 0.0F, -0.5F}));
	}

	/* crop [1, 3), pad 1 + 2, concat, split. */
	{
		Processor processor{waveOf({1.0F, 2.0F, 3.0F, 4.0F}, Channels::Mono)};

		ASSERT_TRUE(processor.crop(1, 3));
		EXPECT_EQ(processor.wave().data(), (std::vector< float >{2.0F, 3.0F}));
		EXPECT_FALSE(processor.crop(2, 1));

		ASSERT_TRUE(processor.pad(1, 2));
		EXPECT_EQ(processor.wave().data(), (std::vector< float >{0.0F, 2.0F, 3.0F, 0.0F, 0.0F}));

		ASSERT_TRUE(processor.concat(waveOf({9.0F}, Channels::Mono)));
		EXPECT_EQ(processor.wave().sampleCount(), 6U);
		EXPECT_FALSE(processor.concat(waveOf({9.0F, 9.0F}, Channels::Stereo))) << "other channel count";
		EXPECT_FALSE(processor.concat(waveOf({9.0F}, Channels::Mono, Frequency::PCM44100Hz))) << "other frequency";

		Wave< float > second;
		ASSERT_TRUE(processor.split(4, second));
		EXPECT_EQ(processor.wave().data(), (std::vector< float >{0.0F, 2.0F, 3.0F, 0.0F}));
		EXPECT_EQ(second.data(), (std::vector< float >{0.0F, 9.0F}));
		EXPECT_FALSE(processor.split(10, second));
	}
}

TEST(WaveFactoryProcessor, channelOperations)
{
	Processor processor{waveOf({0.25F, -0.5F}, Channels::Mono)};

	EXPECT_FALSE(processor.swapChannels()) << "mono";
	ASSERT_TRUE(processor.toStereo());
	EXPECT_EQ(processor.wave().data(), (std::vector< float >{0.25F, 0.25F, -0.5F, -0.5F}));
	EXPECT_FALSE(processor.toStereo()) << "already stereo";

	Processor stereo{waveOf({1.0F, 2.0F, 3.0F, 4.0F}, Channels::Stereo)};

	ASSERT_TRUE(stereo.swapChannels());
	EXPECT_EQ(stereo.wave().data(), (std::vector< float >{2.0F, 1.0F, 4.0F, 3.0F}));
	EXPECT_FALSE(stereo.extractChannel(2));
	ASSERT_TRUE(stereo.extractChannel(1));
	EXPECT_EQ(stereo.wave().data(), (std::vector< float >{1.0F, 3.0F}));
	EXPECT_EQ(stereo.wave().channels(), Channels::Mono);
}

TEST(WaveFactoryProcessor, levelsAndSilence)
{
	/* A sine of amplitude 0.5: peak 20 log10(0.5) = -6.02 dB, RMS 20 log10(0.5 / sqrt(2)) = -9.03 dB. */
	{
		const Processor processor{sine(1000.0F, 0.5F, Frequency::PCM48000Hz, Channels::Mono, 0.5F)};

		EXPECT_NEAR(processor.getPeakLevel(), -6.0206F, 0.01F);
		EXPECT_NEAR(processor.getRMSLevel(), -9.0309F, 0.01F);
		EXPECT_NEAR(processor.getDuration(), 0.5F, 1e-5F);
	}

	/* Silence, and an empty processor. */
	{
		const Processor processor{waveOf(std::vector< float >(100, 0.0F), Channels::Mono)};

		EXPECT_EQ(processor.getPeakLevel(), Processor::SilenceDB);
		EXPECT_EQ(processor.getRMSLevel(), Processor::SilenceDB);

		const Processor empty;
		EXPECT_EQ(empty.getPeakLevel(), Processor::SilenceDB);
		EXPECT_EQ(empty.getDuration(), 0.0F);
		EXPECT_TRUE(empty.detectSilence().empty());
	}

	/* 0.2 s of silence, 0.2 s of tone, 0.2 s of silence at 8 kHz: two zones of 1 600 samples. */
	{
		std::vector< float > data(4800, 0.0F);

		for ( size_t sample = 1600; sample < 3200; ++sample )
		{
			data[sample] = 0.5F;
		}

		const Processor processor{waveOf(data, Channels::Mono, Frequency::PCM8000Hz)};
		const auto zones = processor.detectSilence(-60.0F, 100.0F);

		ASSERT_EQ(zones.size(), 2U);
		EXPECT_EQ(zones[0], (std::pair< size_t, size_t >{0, 1600}));
		EXPECT_EQ(zones[1], (std::pair< size_t, size_t >{3200, 4800}));
		EXPECT_TRUE(processor.detectSilence(-60.0F, 300.0F).empty()) << "zones shorter than the minimum";
	}
}

TEST(WaveFactoryProcessor, normalizeAndBitDepth)
{
	{
		Processor processor{sine(1000.0F, 0.1F, Frequency::PCM48000Hz, Channels::Mono, 0.25F)};

		ASSERT_TRUE(processor.normalize(-6.0F));
		EXPECT_NEAR(processor.getPeakLevel(), -6.0F, 0.01F);
	}

	/* Without dither, 8 bits puts every sample on the 1/128 grid. */
	{
		Processor processor{sine(1000.0F, 0.01F, Frequency::PCM48000Hz, Channels::Mono, 0.9F)};

		ASSERT_TRUE(processor.convertBitDepth(8, false));

		for ( const auto sample : processor.wave().data() )
		{
			EXPECT_FLOAT_EQ(sample * 128.0F, std::round(sample * 128.0F));
		}

		EXPECT_FALSE(processor.convertBitDepth(12, false));
	}

	/* Every depth the API accepts, with dither: finite samples in [-1, 1]. 32 bits computed its level count as
	 * 1 << 31 (INT_MIN): a negative step, and the dither distribution's bounds reversed (its precondition a <= b). */
	for ( const int bits : {16, 24, 32} )
	{
		Processor processor{sine(1000.0F, 0.01F, Frequency::PCM48000Hz, Channels::Mono, 0.9F)};

		ASSERT_TRUE(processor.convertBitDepth(bits, true)) << bits;

		for ( const auto sample : processor.wave().data() )
		{
			ASSERT_TRUE(std::isfinite(sample) && sample >= -1.0F && sample <= 1.0F) << bits << " bits: " << sample;
		}
	}
}

TEST(WaveFactoryProcessor, anEmptyProcessorRefusesEveryOperation)
{
	Processor processor;
	Wave< float > second;

	EXPECT_FALSE(processor.mixDown());
	EXPECT_FALSE(processor.resample(Frequency::PCM44100Hz));
	EXPECT_FALSE(processor.trim());
	EXPECT_FALSE(processor.crop(0, 1));
	EXPECT_FALSE(processor.pad(1));
	EXPECT_FALSE(processor.concat(waveOf({1.0F}, Channels::Mono)));
	EXPECT_FALSE(processor.split(0, second));
	EXPECT_FALSE(processor.toStereo());
	EXPECT_FALSE(processor.extractChannel(0));
	EXPECT_FALSE(processor.swapChannels());
	EXPECT_FALSE(processor.normalize());
	EXPECT_FALSE(processor.convertBitDepth(16));
}
