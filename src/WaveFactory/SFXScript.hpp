/*
 * src/WaveFactory/SFXScript.hpp
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

#pragma once

/* STL inclusions. */
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>

/* Local inclusions. */
#include "FastJSON.hpp"
#include "IO/IO.hpp"
#include "Logging/Logging.hpp"
#include "Synthesizer.hpp"

namespace EmEn::Base::WaveFactory
{
	/**
	 * @brief The SFXScript class parses JSON files to generate sound effects.
	 * @tparam precision_t The precision type of the wave. Default int16_t.
	 *
	 * JSON format:
	 * @code{.json}
	 * {
	 *   "duration": 5000,
	 *   "channels": 2,
	 *   "tracks": [
	 *	 {
	 *	   "preInstructions": [...],
	 *	   "regions": [
	 *		 { "offset": 0, "length": 2000, "instructions": [...] }
	 *	   ],
	 *	   "instructions": [...]
	 *	 },
	 *	 {
	 *	   "preInstructions": [...],
	 *	   "regions": [
	 *		 { "offset": 0, "length": 2000, "instructions": [...] }
	 *	   ],
	 *	   "instructions": [...]
	 *	 }
	 *   ],
	 *   "finalInstructions": [...]
	 * }
	 * @endcode
	 *
	 * - `channels`: Number of channels (1 = mono, 2 = stereo)
	 * - `tracks`: Array of track definitions, one per channel
	 * - Each track is synthesized in mono, then interleaved into the final Wave
	 * - `preInstructions`: Applied first on the full track (generators)
	 * - `regions`: Applied to specific portions of the track (modifiers)
	 * - `instructions`: Applied last on the full track (post-processing)
	 * - `finalInstructions`: Applied to all tracks uniformly before interleaving
	 */
	template< typename precision_t = int16_t >
	requires (std::is_arithmetic_v< precision_t >)
	class SFXScript final
	{
		public:

			/**
			 * @brief Constructs a SFXScript with target wave and sample rate.
			 * @param wave A reference to the wave to synthesize into.
			 * @param frequency The sample rate (default: 48kHz).
			 */
			explicit
			SFXScript (Wave< precision_t > & wave, Frequency frequency = Frequency::PCM48000Hz) noexcept
				: m_wave{wave},
				m_frequency{frequency}
			{

			}

			/**
			 * @brief Generates audio from a JSON file.
			 * @param filepath Path to the JSON script file.
			 * @return bool True on success, false on error.
			 */
			[[nodiscard]]
			bool
			generateFromFile (const std::filesystem::path & filepath) noexcept
			{
				const auto root = FastJSON::getRootFromFile(filepath);

				if ( !root.has_value() )
				{
					Logging::error("SFXScript", std::string{"Failed to parse JSON file: "} + IO::toU8String(filepath));

					return false;
				}

				return this->processScript(root.value());
			}

			/**
			 * @brief Generates audio from a JSON string.
			 * @param jsonString The JSON content as a string.
			 * @return bool True on success, false on error.
			 */
			[[nodiscard]]
			bool
			generateFromString (const std::string & jsonString) noexcept
			{
				const auto root = FastJSON::getRootFromString(jsonString);

				if ( !root.has_value() )
				{
					Logging::error("SFXScript", "Failed to parse JSON string !");

					return false;
				}

				return this->processScript(root.value());
			}

			/**
			 * @brief Generates audio from a pre-parsed JSON value.
			 * @param data The JSON data node.
			 * @return bool True on success, false on error.
			 */
			[[nodiscard]]
			bool
			generateFromData (const Json::Value & data) noexcept
			{
				return this->processScript(data);
			}

		private:

			/**
			 * @brief Converts channel count to Channels enum.
			 * @param count The number of channels.
			 * @return Channels The channel enum value.
			 */
			[[nodiscard]]
			static
			Channels
			channelCountToEnum (size_t count) noexcept
			{
				switch ( count )
				{
					case 1:
						return Channels::Mono;

					case 2:
						return Channels::Stereo;

					default:
						return Channels::Invalid;
				}
			}

			/**
			 * @brief Processes the main script structure.
			 * @param root The root JSON node.
			 * @return bool
			 */
			bool
			processScript (const Json::Value & root) noexcept
			{
				/* Get duration in milliseconds. */
				const auto durationMs = FastJSON::getValue< uint32_t >(root, "duration");

				if ( !durationMs.has_value() )
				{
					Logging::error("SFXScript", "Missing 'duration' field (in milliseconds) !");

					return false;
				}

				/* Cap the requested duration. sampleCount below drives one full Synthesizer buffer
				 * allocation per track; an unbounded duration from the (untrusted) JSON would turn
				 * that allocation into an OOM -> std::terminate under -fno-exceptions. */
				constexpr uint32_t MaxDurationMs = 30 * 60 * 1000; /* 30 minutes. */

				if ( durationMs.value() > MaxDurationMs )
				{
					Logging::error("SFXScript", std::string{"processScript(), 'duration' exceeds the "} + std::to_string(MaxDurationMs) + " ms limit !");

					return false;
				}

				/* Get channel count from JSON. */
				SFXScript::warnUnknownKeys(root, "script", {"duration", "channels", "tracks", "finalInstructions"});

				m_parameterError = false;

				const auto channelCount = this->param< size_t >(root, "channels", 1);

				if ( !this->parametersRead() )
				{
					return false;
				}

				const auto channels = channelCountToEnum(channelCount);

				if ( channels == Channels::Invalid )
				{
					Logging::error("SFXScript", std::string{"Invalid channel count: "} + std::to_string(channelCount) + " (only 1 or 2 supported) !");

					return false;
				}

				/* Get tracks array. */
				const auto tracks = FastJSON::getArray(root, "tracks");

				if ( !tracks.has_value() )
				{
					Logging::error("SFXScript", "Missing 'tracks' array !");

					return false;
				}

				if ( tracks.value().size() != channelCount )
				{
					Logging::error("SFXScript", std::string{"Track count ("} + std::to_string(tracks.value().size()) + ") doesn't match channel count (" + std::to_string(channelCount) + ") !");

					return false;
				}

				/* Calculate sample count from duration. */
				const auto sampleRate = static_cast< size_t >(m_frequency);
				const auto sampleCount = (sampleRate * durationMs.value()) / 1000;

				/* Get finalInstructions early since they will be applied to each track. */
				const auto finalInstructions = FastJSON::getArray(root, "finalInstructions");

				if ( SFXScript::isPresentButNotAnArray(root, "finalInstructions") )
				{
					return false;
				}

				/* Synthesize each track separately in mono. */
				std::vector< Wave< precision_t > > trackWaves(channelCount);

				for ( size_t trackIndex = 0; trackIndex < channelCount; ++trackIndex )
				{
					const auto & trackData = tracks.value()[static_cast< Json::ArrayIndex >(trackIndex)];

					if ( !this->processTrack(trackWaves[trackIndex], trackData, sampleCount, sampleRate) )
					{
						Logging::error("SFXScript", std::string{"Failed to process track "} + std::to_string(trackIndex) + " !");

						return false;
					}

					/* Apply finalInstructions to each track (still mono). */
					if ( finalInstructions.has_value() )
					{
						Synthesizer< precision_t > finalSynth{trackWaves[trackIndex]};

						for ( const auto & instruction : finalInstructions.value() )
						{
							if ( !this->executeInstruction(finalSynth, instruction) )
							{
								return false;
							}
						}
					}
				}

				/* Initialize the final wave with proper channel count. */
				if ( !m_wave.initialize(sampleCount, channels, m_frequency) )
				{
					Logging::error("SFXScript", "Failed to initialize output wave !");

					return false;
				}

				/* Interleave track data into the final wave. */
				auto & outputData = m_wave.data();

				for ( size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex )
				{
					for ( size_t channel = 0; channel < channelCount; ++channel )
					{
						outputData[sampleIndex * channelCount + channel] = trackWaves[channel].data()[sampleIndex];
					}
				}

				return true;
			}

			/**
			 * @brief Processes a single track.
			 * @param trackWave The output wave for this track.
			 * @param trackData The track JSON node.
			 * @param sampleCount The number of samples.
			 * @param sampleRate The sample rate for time conversion.
			 * @return bool
			 */
			bool
			processTrack (Wave< precision_t > & trackWave, const Json::Value & trackData, size_t sampleCount, size_t sampleRate) noexcept
			{
				SFXScript::warnUnknownKeys(trackData, "track", {"preInstructions", "regions", "instructions"});

				if ( SFXScript::isPresentButNotAnArray(trackData, "preInstructions") || SFXScript::isPresentButNotAnArray(trackData, "regions") || SFXScript::isPresentButNotAnArray(trackData, "instructions") )
				{
					return false;
				}

				/* Initialize the track wave in mono. */
				Synthesizer< precision_t > synth{trackWave, sampleCount, m_frequency};

				/* Process preInstructions first (generators on full track). */
				if ( const auto preInstructions = FastJSON::getArray(trackData, "preInstructions"); preInstructions.has_value() )
				{
					for ( const auto & instruction : preInstructions.value() )
					{
						if ( !this->executeInstruction(synth, instruction) )
						{
							return false;
						}
					}
				}

				/* Process regions if present (modifiers on specific portions). */
				if ( const auto regions = FastJSON::getArray(trackData, "regions"); regions.has_value() )
				{
					for ( const auto & region : regions.value() )
					{
						if ( !this->processRegion(synth, region, sampleRate) )
						{
							return false;
						}
					}
				}

				/* Process instructions last (post-processing on full track). */
				if ( const auto instructions = FastJSON::getArray(trackData, "instructions"); instructions.has_value() )
				{
					synth.resetRegion();

					for ( const auto & instruction : instructions.value() )
					{
						if ( !this->executeInstruction(synth, instruction) )
						{
							return false;
						}
					}
				}

				return true;
			}

			/**
			 * @brief Processes a single region.
			 * @param synth The synthesizer instance.
			 * @param region The region JSON node.
			 * @param sampleRate The sample rate for time conversion.
			 * @return bool
			 */
			bool
			processRegion (Synthesizer< precision_t > & synth, const Json::Value & region, size_t sampleRate) noexcept
			{
				/* Get offset and length in milliseconds. */
				SFXScript::warnUnknownKeys(region, "region", {"offset", "length", "instructions"});

				m_parameterError = false;

				const auto offsetMs = this->param< uint32_t >(region, "offset", 0U);
				const auto lengthMs = this->param< uint32_t >(region, "length", 0U);

				if ( !this->parametersRead() || SFXScript::isPresentButNotAnArray(region, "instructions") )
				{
					return false;
				}

				/* Convert to samples. */
				const auto offsetSamples = (sampleRate * offsetMs) / 1000;
				const auto lengthSamples = (lengthMs > 0) ? (sampleRate * lengthMs) / 1000 : 0;

				synth.setRegion(offsetSamples, lengthSamples);

				/* Process instructions in this region. */
				if ( const auto instructions = FastJSON::getArray(region, "instructions"); instructions.has_value() )
				{
					for ( const auto & instruction : instructions.value() )
					{
						if ( !this->executeInstruction(synth, instruction) )
						{
							return false;
						}
					}
				}

				return true;
			}

			/**
			 * @brief Executes a single instruction.
			 * @param synth The synthesizer instance.
			 * @param instruction The instruction JSON node.
			 * @return bool
			 */
			bool
			executeInstruction (Synthesizer< precision_t > & synth, const Json::Value & instruction) noexcept
			{
				const auto type = FastJSON::getValue< std::string >(instruction, "type");

				if ( !type.has_value() )
				{
					Logging::error("SFXScript", "Instruction missing 'type' field !");

					return false;
				}

				const auto & typeStr = type.value();

				m_parameterError = false;

				/* Oscillators / Generators. */
				if ( typeStr == "whiteNoise" )
				{
					SFXScript::warnUnknownKeys(instruction, "whiteNoise", {});

					return this->parametersRead() && synth.whiteNoise();
				}

				if ( typeStr == "pinkNoise" )
				{
					SFXScript::warnUnknownKeys(instruction, "pinkNoise", {});

					return this->parametersRead() && synth.pinkNoise();
				}

				if ( typeStr == "brownNoise" )
				{
					SFXScript::warnUnknownKeys(instruction, "brownNoise", {});

					return this->parametersRead() && synth.brownNoise();
				}

				if ( typeStr == "blueNoise" )
				{
					SFXScript::warnUnknownKeys(instruction, "blueNoise", {});

					return this->parametersRead() && synth.blueNoise();
				}

				if ( typeStr == "sineWave" )
				{
					SFXScript::warnUnknownKeys(instruction, "sineWave", {"frequency", "amplitude"});

					const auto frequency = this->param< float >(instruction, "frequency", 440.0F);
					const auto amplitude = this->param< float >(instruction, "amplitude", 0.5F);

					return this->parametersRead() && synth.sineWave(frequency, amplitude);
				}

				if ( typeStr == "squareWave" )
				{
					SFXScript::warnUnknownKeys(instruction, "squareWave", {"frequency", "amplitude"});

					const auto frequency = this->param< float >(instruction, "frequency", 440.0F);
					const auto amplitude = this->param< float >(instruction, "amplitude", 0.5F);

					return this->parametersRead() && synth.squareWave(frequency, amplitude);
				}

				if ( typeStr == "triangleWave" )
				{
					SFXScript::warnUnknownKeys(instruction, "triangleWave", {"frequency", "amplitude"});

					const auto frequency = this->param< float >(instruction, "frequency", 440.0F);
					const auto amplitude = this->param< float >(instruction, "amplitude", 0.5F);

					return this->parametersRead() && synth.triangleWave(frequency, amplitude);
				}

				if ( typeStr == "sawtoothWave" )
				{
					SFXScript::warnUnknownKeys(instruction, "sawtoothWave", {"frequency", "amplitude"});

					const auto frequency = this->param< float >(instruction, "frequency", 440.0F);
					const auto amplitude = this->param< float >(instruction, "amplitude", 0.5F);

					return this->parametersRead() && synth.sawtoothWave(frequency, amplitude);
				}

				if ( typeStr == "pitchSweep" )
				{
					SFXScript::warnUnknownKeys(instruction, "pitchSweep", {"startFrequency", "endFrequency", "amplitude"});

					const auto startFreq = this->param< float >(instruction, "startFrequency", 440.0F);
					const auto endFreq = this->param< float >(instruction, "endFrequency", 880.0F);
					const auto amplitude = this->param< float >(instruction, "amplitude", 0.5F);

					return this->parametersRead() && synth.pitchSweep(startFreq, endFreq, amplitude);
				}

				if ( typeStr == "noiseBurst" )
				{
					SFXScript::warnUnknownKeys(instruction, "noiseBurst", {"decayTime", "amplitude", "whiteNoise"});

					const auto decayTime = this->param< float >(instruction, "decayTime", 0.1F);
					const auto amplitude = this->param< float >(instruction, "amplitude", 0.8F);
					const auto useWhite = this->param< bool >(instruction, "whiteNoise", true);

					return this->parametersRead() && synth.noiseBurst(decayTime, amplitude, useWhite);
				}

				/* Envelope / Modulation. */
				if ( typeStr == "applyADSR" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyADSR", {"attack", "decay", "sustain", "release"});

					const auto attack = this->param< float >(instruction, "attack", 0.01F);
					const auto decay = this->param< float >(instruction, "decay", 0.1F);
					const auto sustain = this->param< float >(instruction, "sustain", 0.7F);
					const auto release = this->param< float >(instruction, "release", 0.1F);

					return this->parametersRead() && synth.applyADSR(attack, decay, sustain, release);
				}

				if ( typeStr == "applyVibrato" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyVibrato", {"rate", "depth"});

					const auto rate = this->param< float >(instruction, "rate", 5.0F);
					const auto depth = this->param< float >(instruction, "depth", 0.02F);

					return this->parametersRead() && synth.applyVibrato(rate, depth);
				}

				if ( typeStr == "applyTremolo" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyTremolo", {"rate", "depth"});

					const auto rate = this->param< float >(instruction, "rate", 8.0F);
					const auto depth = this->param< float >(instruction, "depth", 0.5F);

					return this->parametersRead() && synth.applyTremolo(rate, depth);
				}

				if ( typeStr == "applyFadeIn" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyFadeIn", {"time"});

					const auto fadeTime = this->param< float >(instruction, "time", 0.1F);

					return this->parametersRead() && synth.applyFadeIn(fadeTime);
				}

				if ( typeStr == "applyFadeOut" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyFadeOut", {"time"});

					const auto fadeTime = this->param< float >(instruction, "time", 0.1F);

					return this->parametersRead() && synth.applyFadeOut(fadeTime);
				}

				/* Filters. */
				if ( typeStr == "applyLowPass" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyLowPass", {"cutoff"});

					const auto cutoff = this->param< float >(instruction, "cutoff", 1000.0F);

					return this->parametersRead() && synth.applyLowPass(cutoff);
				}

				if ( typeStr == "applyHighPass" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyHighPass", {"cutoff"});

					const auto cutoff = this->param< float >(instruction, "cutoff", 500.0F);

					return this->parametersRead() && synth.applyHighPass(cutoff);
				}

				if ( typeStr == "applyWahWah" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyWahWah", {"rate", "depth", "minFrequency", "maxFrequency"});

					const auto rate = this->param< float >(instruction, "rate", 2.0F);
					const auto depth = this->param< float >(instruction, "depth", 0.8F);
					const auto minFreq = this->param< float >(instruction, "minFrequency", 400.0F);
					const auto maxFreq = this->param< float >(instruction, "maxFrequency", 2000.0F);

					return this->parametersRead() && synth.applyWahWah(rate, depth, minFreq, maxFreq);
				}

				if ( typeStr == "applyAutoWah" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyAutoWah", {"sensitivity", "minFrequency", "maxFrequency", "attack", "release"});

					const auto sensitivity = this->param< float >(instruction, "sensitivity", 3.0F);
					const auto minFreq = this->param< float >(instruction, "minFrequency", 200.0F);
					const auto maxFreq = this->param< float >(instruction, "maxFrequency", 3000.0F);
					const auto attack = this->param< float >(instruction, "attack", 0.01F);
					const auto release = this->param< float >(instruction, "release", 0.1F);

					return this->parametersRead() && synth.applyAutoWah(sensitivity, minFreq, maxFreq, attack, release);
				}

				/* Distortion effects. */
				if ( typeStr == "applyDistortion" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyDistortion", {"gain", "mix", "hardClip"});

					const auto gain = this->param< float >(instruction, "gain", 10.0F);
					const auto mix = this->param< float >(instruction, "mix", 1.0F);
					const auto hardClip = this->param< bool >(instruction, "hardClip", false);

					return this->parametersRead() && synth.applyDistortion(gain, mix, hardClip);
				}

				if ( typeStr == "applyOverdrive" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyOverdrive", {"drive", "tone"});

					const auto drive = this->param< float >(instruction, "drive", 5.0F);
					const auto tone = this->param< float >(instruction, "tone", 0.5F);

					return this->parametersRead() && synth.applyOverdrive(drive, tone);
				}

				if ( typeStr == "applyFuzz" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyFuzz", {"intensity", "octaveUp"});

					const auto intensity = this->param< float >(instruction, "intensity", 10.0F);
					const auto octaveUp = this->param< bool >(instruction, "octaveUp", false);

					return this->parametersRead() && synth.applyFuzz(intensity, octaveUp);
				}

				if ( typeStr == "applyBitCrush" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyBitCrush", {"bits"});

					const auto bits = this->param< int32_t >(instruction, "bits", 8);

					return this->parametersRead() && synth.applyBitCrush(bits);
				}

				if ( typeStr == "applySampleRateReduce" )
				{
					SFXScript::warnUnknownKeys(instruction, "applySampleRateReduce", {"factor"});

					const auto factor = this->param< int32_t >(instruction, "factor", 4);

					return this->parametersRead() && synth.applySampleRateReduce(factor);
				}

				if ( typeStr == "applyRingModulation" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyRingModulation", {"frequency"});

					const auto frequency = this->param< float >(instruction, "frequency", 440.0F);

					return this->parametersRead() && synth.applyRingModulation(frequency);
				}

				/* Modulation effects. */
				if ( typeStr == "applyChorus" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyChorus", {"rate", "depth", "mix"});

					const auto rate = this->param< float >(instruction, "rate", 1.5F);
					const auto depth = this->param< float >(instruction, "depth", 10.0F);
					const auto mix = this->param< float >(instruction, "mix", 0.5F);

					return this->parametersRead() && synth.applyChorus(rate, depth, mix);
				}

				if ( typeStr == "applyFlanger" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyFlanger", {"rate", "depth", "feedback", "mix"});

					const auto rate = this->param< float >(instruction, "rate", 0.5F);
					const auto depth = this->param< float >(instruction, "depth", 5.0F);
					const auto feedback = this->param< float >(instruction, "feedback", 0.7F);
					const auto mix = this->param< float >(instruction, "mix", 0.5F);

					return this->parametersRead() && synth.applyFlanger(rate, depth, feedback, mix);
				}

				if ( typeStr == "applyPhaser" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyPhaser", {"rate", "depth", "stages", "feedback", "mix"});

					const auto rate = this->param< float >(instruction, "rate", 0.5F);
					const auto depth = this->param< float >(instruction, "depth", 0.7F);
					const auto stages = this->param< int32_t >(instruction, "stages", 4);
					const auto feedback = this->param< float >(instruction, "feedback", 0.5F);
					const auto mix = this->param< float >(instruction, "mix", 0.5F);

					return this->parametersRead() && synth.applyPhaser(rate, depth, stages, feedback, mix);
				}

				/* Delay / Reverb. */
				if ( typeStr == "applyDelay" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyDelay", {"delayTime", "feedback", "mix"});

					const auto delayTime = this->param< float >(instruction, "delayTime", 300.0F);
					const auto feedback = this->param< float >(instruction, "feedback", 0.4F);
					const auto mix = this->param< float >(instruction, "mix", 0.5F);

					return this->parametersRead() && synth.applyDelay(delayTime, feedback, mix);
				}

				if ( typeStr == "applyReverb" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyReverb", {"roomSize", "damping", "mix"});

					const auto roomSize = this->param< float >(instruction, "roomSize", 0.5F);
					const auto damping = this->param< float >(instruction, "damping", 0.5F);
					const auto mix = this->param< float >(instruction, "mix", 0.3F);

					return this->parametersRead() && synth.applyReverb(roomSize, damping, mix);
				}

				/* Dynamics. */
				if ( typeStr == "applyCompressor" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyCompressor", {"threshold", "ratio", "attack", "release", "makeupGain"});

					const auto threshold = this->param< float >(instruction, "threshold", -20.0F);
					const auto ratio = this->param< float >(instruction, "ratio", 4.0F);
					const auto attack = this->param< float >(instruction, "attack", 0.01F);
					const auto release = this->param< float >(instruction, "release", 0.1F);
					const auto makeupGain = this->param< float >(instruction, "makeupGain", 0.0F);

					return this->parametersRead() && synth.applyCompressor(threshold, ratio, attack, release, makeupGain);
				}

				if ( typeStr == "applyNoiseGate" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyNoiseGate", {"threshold", "attack", "hold", "release"});

					const auto threshold = this->param< float >(instruction, "threshold", -40.0F);
					const auto attack = this->param< float >(instruction, "attack", 0.001F);
					const auto hold = this->param< float >(instruction, "hold", 0.05F);
					const auto release = this->param< float >(instruction, "release", 0.1F);

					return this->parametersRead() && synth.applyNoiseGate(threshold, attack, hold, release);
				}

				if ( typeStr == "applyPitchShift" )
				{
					SFXScript::warnUnknownKeys(instruction, "applyPitchShift", {"semitones", "mix"});

					const auto semitones = this->param< float >(instruction, "semitones", 0.0F);
					const auto mix = this->param< float >(instruction, "mix", 1.0F);

					return this->parametersRead() && synth.applyPitchShift(semitones, mix);
				}

				/* Utilities. */
				if ( typeStr == "normalize" )
				{
					SFXScript::warnUnknownKeys(instruction, "normalize", {});

					return this->parametersRead() && synth.normalize();
				}

				if ( typeStr == "reverse" )
				{
					SFXScript::warnUnknownKeys(instruction, "reverse", {});

					return this->parametersRead() && synth.reverse();
				}

				Logging::error("SFXScript", std::string{"Unknown instruction type: "} + typeStr);

				return false;
			}

			/**
			 * @brief Reads an optional parameter: absent, its default; present but not readable as T (a string for a number,
			 * 6.5 for an integer...), an error that refuses the instruction (owner decision, 2026-10-17 — it used to be read
			 * as absent: "frequency":"880" played at 440 Hz without a word).
			 * @tparam value_t The parameter's type.
			 * @param node The JSON object holding it.
			 * @param key The parameter's name.
			 * @param fallback The documented default.
			 * @return value_t
			 */
			template< typename value_t >
			[[nodiscard]]
			value_t
			param (const Json::Value & node, const char * key, value_t fallback) noexcept
			{
				if ( !node.isObject() || !node.isMember(key) )
				{
					return fallback;
				}

				if ( const auto value = FastJSON::getValue< value_t >(node, key); value.has_value() )
				{
					return value.value();
				}

				Logging::error("SFXScript", std::string{"The parameter '"} + key + "' has the wrong type !");

				m_parameterError = true;

				return fallback;
			}

			/**
			 * @brief Returns whether an optional array is present with another type ("instructions": 5): an error, like a
			 * parameter of the wrong type — it used to be skipped as absent.
			 * @param node The JSON object holding it.
			 * @param key The array's name.
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			isPresentButNotAnArray (const Json::Value & node, const char * key) noexcept
			{
				if ( !node.isObject() || !node.isMember(key) || node[key].isArray() )
				{
					return false;
				}

				Logging::error("SFXScript", std::string{"'"} + key + "' must be an array !");

				return true;
			}

			/**
			 * @brief Returns whether every parameter read since the last reset had the right type.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			parametersRead () const noexcept
			{
				return !m_parameterError;
			}

			/**
			 * @brief Warns about the keys of a JSON object that are not among the known ones (a typo, such as "bitDepth" for
			 * "bits", used to be ignored silently). A warning, not a refusal (owner decision, 2026-10-17).
			 * @param node The JSON object.
			 * @param context The object's role (an instruction type, "script", "track", "region").
			 * @param knownKeys The keys it reads.
			 */
			static
			void
			warnUnknownKeys (const Json::Value & node, const char * context, std::initializer_list< std::string_view > knownKeys) noexcept
			{
				if ( !node.isObject() )
				{
					return;
				}

				for ( const auto & key : node.getMemberNames() )
				{
					if ( key != "type" && std::ranges::find(knownKeys, std::string_view{key}) == knownKeys.end() )
					{
						Logging::warning("SFXScript", std::string{"'"} + context + "': unknown key '" + key + "', ignored !");
					}
				}
			}

			Wave< precision_t > & m_wave;
			Frequency m_frequency;
			bool m_parameterError{false};
	};
}
