/*
 * src/Testing/test_WaveFactorySynthesizer.cpp
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
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string_view>
#include <vector>

/* Local inclusions. */
#include "IO/MemoryStream.hpp"
#include "WaveFactory/FileFormatJSON.hpp"
#include "WaveFactory/Synthesizer.hpp"
#include "WaveFactory/Wave.hpp"

/* The coverage pass (2026-10-17): the generators and effects, their documented parameter domains (owner decision: a
 * value outside is REFUSED — SFX scripts are data), the saturated output, and SFXScript refusing such a script. */
namespace
{
	using namespace EmEn::Base;
	using namespace EmEn::Base::WaveFactory;

	using FloatSynth = Synthesizer< float >;

	/* 0.1 s at 48 kHz. */
	constexpr size_t SampleCount{4800};

	size_t
	risingCrossings (const std::vector< float > & data)
	{
		size_t count = 0;

		for ( size_t index = 1; index < data.size(); ++index )
		{
			if ( data[index - 1] < 0.0F && data[index] >= 0.0F )
			{
				++count;
			}
		}

		return count;
	}

	float
	peakOf (const std::vector< float > & data)
	{
		float peak = 0.0F;

		for ( const auto sample : data )
		{
			peak = std::max(peak, std::abs(sample));
		}

		return peak;
	}

	/* A wave holding 1.0 everywhere. */
	void
	fillWithOne (Wave< float > & wave)
	{
		std::ranges::fill(wave.data(), 1.0F);
	}
}

TEST(WaveFactorySynthesizer, tonesHaveTheirFrequencyAndAmplitude)
{
	Wave< float > wave;
	FloatSynth synth{wave, SampleCount, Frequency::PCM48000Hz};

	ASSERT_TRUE(synth.sineWave(1000.0F, 0.5F));
	EXPECT_NEAR(static_cast< double >(risingCrossings(wave.data())), 100.0, 1.0);
	EXPECT_NEAR(peakOf(wave.data()), 0.5F, 0.01F);

	ASSERT_TRUE(synth.squareWave(1000.0F, 0.5F));
	EXPECT_TRUE(std::ranges::all_of(wave.data(), [] (float sample) { return std::abs(std::abs(sample) - 0.5F) < 1e-6F; }));

	ASSERT_TRUE(synth.triangleWave(1000.0F, 0.5F));
	EXPECT_NEAR(peakOf(wave.data()), 0.5F, 0.01F);
	EXPECT_NEAR(static_cast< double >(risingCrossings(wave.data())), 100.0, 1.0);

	ASSERT_TRUE(synth.sawtoothWave(1000.0F, 0.5F));
	EXPECT_LE(peakOf(wave.data()), 0.5F + 1e-6F);

	ASSERT_TRUE(synth.pitchSweep(200.0F, 2000.0F, 0.5F));
	EXPECT_LE(peakOf(wave.data()), 0.5F + 1e-6F);
}

TEST(WaveFactorySynthesizer, anAmplitudeAboveOneIsClipped)
{
	/* The integer wave: converting 2.0 * 32767 to int16 was undefined behaviour; now clipped at 32767. */
	Wave< int16_t > wave;
	Synthesizer< int16_t > synth{wave, SampleCount, Frequency::PCM48000Hz};

	ASSERT_TRUE(synth.sineWave(1000.0F, 2.0F));
	EXPECT_EQ(*std::ranges::max_element(wave.data()), std::numeric_limits< int16_t >::max());
	EXPECT_EQ(*std::ranges::min_element(wave.data()), -std::numeric_limits< int16_t >::max());

	Wave< float > floatWave;
	FloatSynth floatSynth{floatWave, SampleCount, Frequency::PCM48000Hz};

	ASSERT_TRUE(floatSynth.squareWave(1000.0F, 3.0F));
	EXPECT_EQ(peakOf(floatWave.data()), 1.0F);
}

TEST(WaveFactorySynthesizer, outOfDomainParametersAreRefused)
{
	const auto nan = std::numeric_limits< float >::quiet_NaN();
	const std::vector< std::function< bool (FloatSynth &) > > calls{
		[] (FloatSynth & s) { return s.sineWave(-440.0F, 0.5F); },
		[] (FloatSynth & s) { return s.sineWave(30000.0F, 0.5F); },
		[] (FloatSynth & s) { return s.triangleWave(440.0F, -0.5F); },
		[nan] (FloatSynth & s) { return s.sawtoothWave(nan, 0.5F); },
		[] (FloatSynth & s) { return s.applyADSR(-0.1F, 0.1F, 0.5F, 0.1F); },
		[] (FloatSynth & s) { return s.applyADSR(0.1F, 0.1F, 1.5F, 0.1F); },
		[] (FloatSynth & s) { return s.applyADSR(1e30F, 0.1F, 0.5F, 0.1F); },
		[] (FloatSynth & s) { return s.pitchSweep(0.0F, 880.0F, 0.5F); },
		[] (FloatSynth & s) { return s.applyVibrato(1e6F, 0.02F); },
		[] (FloatSynth & s) { return s.applyTremolo(8.0F, 2.0F); },
		[] (FloatSynth & s) { return s.applyFadeOut(-1.0F); },
		[] (FloatSynth & s) { return s.applyLowPass(0.0F); },
		[] (FloatSynth & s) { return s.applyHighPass(1e9F); },
		[] (FloatSynth & s) { return s.applyBitCrush(0); },
		[] (FloatSynth & s) { return s.applyBitCrush(17); },
		[] (FloatSynth & s) { return s.applyDistortion(100.0F, 1.0F, false); },
		[] (FloatSynth & s) { return s.applyOverdrive(5.0F, 2.0F); },
		[] (FloatSynth & s) { return s.applyFuzz(0.5F, false); },
		[] (FloatSynth & s) { return s.applyChorus(1.5F, -10.0F, 0.5F); },
		[] (FloatSynth & s) { return s.applyFlanger(0.5F, 5.0F, 2.0F, 0.5F); },
		[] (FloatSynth & s) { return s.applyPhaser(0.5F, 0.7F, 3, 0.5F, 0.5F); },
		[] (FloatSynth & s) { return s.applyPhaser(0.5F, 0.7F, 100000, 0.5F, 0.5F); },
		[] (FloatSynth & s) { return s.applyDelay(0.0F, 0.4F, 0.5F); },
		[] (FloatSynth & s) { return s.applyReverb(5.0F, 0.5F, 0.3F); },
		[] (FloatSynth & s) { return s.applyWahWah(2.0F, 0.8F, 2000.0F, 400.0F); },
		[] (FloatSynth & s) { return s.applyAutoWah(3.0F, 200.0F, 3000.0F, -0.01F, 0.1F); },
		[] (FloatSynth & s) { return s.applyCompressor(-20.0F, 0.5F, 0.01F, 0.1F, 0.0F); },
		[] (FloatSynth & s) { return s.applyNoiseGate(10.0F, 0.001F, 0.05F, 0.1F); },
		[] (FloatSynth & s) { return s.applyPitchShift(24.0F, 1.0F); },
		[] (FloatSynth & s) { return s.applySampleRateReduce(0); },
	};

	for ( size_t index = 0; index < calls.size(); ++index )
	{
		Wave< float > wave;
		FloatSynth synth{wave, SampleCount, Frequency::PCM48000Hz};
		ASSERT_TRUE(synth.sineWave(440.0F, 0.5F));
		const auto before = wave.data();

		EXPECT_FALSE(calls[index](synth)) << "call " << index;
		EXPECT_EQ(wave.data(), before) << "call " << index << ": a refused call must leave the wave untouched";
	}
}

TEST(WaveFactorySynthesizer, envelopesFadesReverseNormalizeAndRegions)
{
	Wave< float > wave;
	FloatSynth synth{wave, SampleCount, Frequency::PCM48000Hz};

	/* ADSR on a constant: 0 at the start, the sustain level in the middle, near 0 at the end. */
	fillWithOne(wave);
	ASSERT_TRUE(synth.applyADSR(0.01F, 0.01F, 0.5F, 0.01F));
	EXPECT_NEAR(wave.data().front(), 0.0F, 1e-3F);
	EXPECT_NEAR(wave.data()[2400], 0.5F, 1e-3F);
	EXPECT_LT(wave.data().back(), 0.01F);

	/* A 50 ms fade-in reaches half way at 25 ms; a fade-out ends at 0. */
	fillWithOne(wave);
	ASSERT_TRUE(synth.applyFadeIn(0.05F));
	EXPECT_NEAR(wave.data().front(), 0.0F, 1e-3F);
	EXPECT_NEAR(wave.data()[1200], 0.5F, 0.01F);
	EXPECT_NEAR(wave.data()[3000], 1.0F, 1e-6F);

	fillWithOne(wave);
	ASSERT_TRUE(synth.applyFadeOut(0.05F));
	EXPECT_NEAR(wave.data().back(), 0.0F, 1e-3F);
	EXPECT_NEAR(wave.data()[0], 1.0F, 1e-6F);

	/* Reverse, normalize. */
	ASSERT_TRUE(synth.sawtoothWave(1000.0F, 0.25F));
	const auto forward = wave.data();
	ASSERT_TRUE(synth.reverse());
	EXPECT_TRUE(std::ranges::equal(wave.data(), std::vector< float >(forward.rbegin(), forward.rend())));
	ASSERT_TRUE(synth.normalize());
	EXPECT_NEAR(peakOf(wave.data()), 1.0F, 1e-3F);

	/* A region confines a generator; resetRegion() covers the wave again. */
	std::ranges::fill(wave.data(), 0.0F);
	synth.setRegion(1000, 500);
	EXPECT_EQ(synth.regionOffset(), 1000U);
	ASSERT_TRUE(synth.sineWave(1000.0F, 0.5F));
	EXPECT_EQ(peakOf(std::vector< float >(wave.data().begin(), wave.data().begin() + 1000)), 0.0F);
	EXPECT_GT(peakOf(std::vector< float >(wave.data().begin() + 1000, wave.data().begin() + 1500)), 0.4F);
	EXPECT_EQ(peakOf(std::vector< float >(wave.data().begin() + 1500, wave.data().end())), 0.0F);
	synth.resetRegion();
	EXPECT_EQ(synth.regionOffset(), 0U);
}

TEST(WaveFactorySynthesizer, everyEffectAtItsDefaultsKeepsTheSignalInRange)
{
	const std::vector< std::function< bool (FloatSynth &) > > effects{
		[] (FloatSynth & s) { return s.whiteNoise(); },
		[] (FloatSynth & s) { return s.pinkNoise(); },
		[] (FloatSynth & s) { return s.brownNoise(); },
		[] (FloatSynth & s) { return s.blueNoise(); },
		[] (FloatSynth & s) { return s.noiseBurst(); },
		[] (FloatSynth & s) { return s.noiseBurst(0.1F, 0.8F, false); },
		[] (FloatSynth & s) { return s.applyVibrato(); },
		[] (FloatSynth & s) { return s.applyTremolo(); },
		[] (FloatSynth & s) { return s.applyLowPass(); },
		[] (FloatSynth & s) { return s.applyHighPass(); },
		[] (FloatSynth & s) { return s.applyRingModulation(); },
		[] (FloatSynth & s) { return s.applyBitCrush(); },
		[] (FloatSynth & s) { return s.applyDistortion(); },
		[] (FloatSynth & s) { return s.applyDistortion(10.0F, 1.0F, true); },
		[] (FloatSynth & s) { return s.applyOverdrive(); },
		[] (FloatSynth & s) { return s.applyFuzz(); },
		[] (FloatSynth & s) { return s.applyFuzz(10.0F, true); },
		[] (FloatSynth & s) { return s.applyChorus(); },
		[] (FloatSynth & s) { return s.applyFlanger(); },
		[] (FloatSynth & s) { return s.applyPhaser(); },
		[] (FloatSynth & s) { return s.applyDelay(); },
		[] (FloatSynth & s) { return s.applyReverb(); },
		[] (FloatSynth & s) { return s.applyWahWah(); },
		[] (FloatSynth & s) { return s.applyAutoWah(); },
		[] (FloatSynth & s) { return s.applyCompressor(); },
		[] (FloatSynth & s) { return s.applyNoiseGate(); },
		[] (FloatSynth & s) { return s.applyPitchShift(7.0F, 1.0F); },
		[] (FloatSynth & s) { return s.applySampleRateReduce(); },
	};

	for ( size_t index = 0; index < effects.size(); ++index )
	{
		Wave< float > wave;
		FloatSynth synth{wave, SampleCount, Frequency::PCM48000Hz};
		ASSERT_TRUE(synth.sineWave(440.0F, 0.5F));

		ASSERT_TRUE(effects[index](synth)) << "effect " << index;
		EXPECT_TRUE(std::ranges::all_of(wave.data(), [] (float sample) { return std::isfinite(sample) && std::abs(sample) <= 1.0F; })) << "effect " << index;
	}

	/* mix(): the other wave at the given level. */
	Wave< float > wave;
	FloatSynth synth{wave, SampleCount, Frequency::PCM48000Hz};
	Wave< float > other;
	FloatSynth otherSynth{other, SampleCount, Frequency::PCM48000Hz};
	ASSERT_TRUE(otherSynth.sineWave(440.0F, 0.5F));
	ASSERT_TRUE(synth.mix(other, 0.5F));
	EXPECT_FALSE(synth.mix(other, 2.0F));
}

TEST(WaveFactorySynthesizer, anSFXScriptWithAnOutOfDomainParameterIsRefused)
{
	const auto decode = [] (std::string_view script) {
		std::vector< std::byte > buffer(script.size());
		std::ranges::transform(script, buffer.begin(), [] (char character) { return static_cast< std::byte >(character); });

		IO::MemoryStream stream{std::as_const(buffer)};
		FileFormatJSON< int16_t > format;
		Wave< int16_t > wave;

		return format.readStream(stream, wave, {});
	};

	EXPECT_TRUE(decode(R"({"duration":50,"channels":1,"tracks":[{"instructions":[{"type":"sineWave","frequency":440.0,"amplitude":0.5},{"type":"applyDistortion","gain":20.0}]}]})"));
	EXPECT_FALSE(decode(R"({"duration":50,"channels":1,"tracks":[{"instructions":[{"type":"sineWave","frequency":440.0,"amplitude":0.5},{"type":"applyDistortion","gain":100.0}]}]})"));
	EXPECT_FALSE(decode(R"({"duration":50,"channels":1,"tracks":[{"instructions":[{"type":"sineWave","frequency":-440.0,"amplitude":0.5}]}]})"));
}
