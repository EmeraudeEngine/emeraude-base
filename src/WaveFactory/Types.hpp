/*
 * src/WaveFactory/Types.hpp
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
#include <cstdint>
#include <iostream>
#include <stop_token>
#include <utility>

/* Forward declarations. */

struct tsf;

namespace EmEn::Base::WaveFactory
{
	/**
	 * @brief The channel mode enumeration.
	 */
	enum class Channels : std::uint8_t
	{
		Invalid = 0,
		Mono = 1,
		Stereo = 2
	};

	/**
	 * @brief The channel index enumeration.
	 */
	enum class Channel : std::uint8_t
	{
		Mono = 0,
		Left = 0,
		Right = 1
	};

	/**
	 * @brief A PCM sample rate in Hz: any integer rate in [MinimumHz, MaximumHz], or Invalid (0 Hz).
	 * @note A strong type, not a closed enumeration (owner decision, 2026-11-06): a sound file at a non-standard rate
	 * (22255 Hz, a classic Mac rate; 22222, 16007, 12500 Hz…) is a valid wave, and the engine resamples it once to the
	 * playback rate. The ten standard rates stay as named constants. An instance is only built through fromHz()
	 * (validated) or a constant: it is always either Invalid or inside the range.
	 * @note The range is a sanity bound for a rate read from a file, a setting or a device (a trust boundary), not a
	 * playback limit: OpenAL takes any rate, and libsamplerate any ratio in [1/256, 256].
	 */
	class Frequency final
	{
		public:

			/** @brief The lowest accepted rate in Hz. */
			static constexpr std::uint32_t MinimumHz{1000U};
			/** @brief The highest accepted rate in Hz. */
			static constexpr std::uint32_t MaximumHz{384000U};

			/* NOTE: declared here, defined inline constexpr after the class (a class is incomplete inside its own body). */
			static const Frequency Invalid;
			static const Frequency PCM4000Hz;
			static const Frequency PCM8000Hz;
			static const Frequency PCM11025Hz;
			static const Frequency PCM16000Hz;
			static const Frequency PCM22050Hz;
			static const Frequency PCM32000Hz;
			static const Frequency PCM44100Hz;
			static const Frequency PCM48000Hz;
			static const Frequency PCM96000Hz;
			static const Frequency PCM192000Hz;

			/**
			 * @brief Constructs an invalid frequency (0 Hz).
			 */
			constexpr Frequency () noexcept = default;

			/**
			 * @brief Returns the frequency of a rate, or Invalid when the rate is outside [MinimumHz, MaximumHz].
			 * @note The trust boundary: a rate coming from a file, a setting or a device goes through here. The caller
			 * logs the refusal with its own context.
			 * @param hz The rate in Hz.
			 * @return Frequency
			 */
			[[nodiscard]]
			static constexpr
			Frequency
			fromHz (std::int64_t hz) noexcept
			{
				if ( std::cmp_less(hz, MinimumHz) || std::cmp_greater(hz, MaximumHz) )
				{
					return Frequency{};
				}

				return Frequency{static_cast< std::uint32_t >(hz)};
			}

			/**
			 * @brief Returns the rate in Hz (0 when invalid).
			 * @return std::uint32_t
			 */
			[[nodiscard]]
			constexpr
			std::uint32_t
			hz () const noexcept
			{
				return m_hz;
			}

			/**
			 * @brief Returns whether the frequency is a usable rate (not Invalid).
			 * @return bool
			 */
			[[nodiscard]]
			constexpr
			bool
			isValid () const noexcept
			{
				return m_hz != 0U;
			}

			/**
			 * @brief Compares two frequencies.
			 * @param lhs The left operand.
			 * @param rhs The right operand.
			 * @return bool
			 */
			[[nodiscard]]
			friend
			constexpr
			bool
			operator== (const Frequency & lhs, const Frequency & rhs) noexcept = default;

		private:

			/**
			 * @brief Constructs a frequency from a rate already known to be in range.
			 * @param hz The rate in Hz.
			 */
			explicit
			constexpr
			Frequency (std::uint32_t hz) noexcept
				: m_hz{hz}
			{

			}

			std::uint32_t m_hz{0U};
	};

	inline constexpr Frequency Frequency::Invalid{};
	inline constexpr Frequency Frequency::PCM4000Hz{4000U};
	inline constexpr Frequency Frequency::PCM8000Hz{8000U};
	inline constexpr Frequency Frequency::PCM11025Hz{11025U};
	inline constexpr Frequency Frequency::PCM16000Hz{16000U};
	inline constexpr Frequency Frequency::PCM22050Hz{22050U};
	inline constexpr Frequency Frequency::PCM32000Hz{32000U};
	inline constexpr Frequency Frequency::PCM44100Hz{44100U};
	inline constexpr Frequency Frequency::PCM48000Hz{48000U};
	inline constexpr Frequency Frequency::PCM96000Hz{96000U};
	inline constexpr Frequency Frequency::PCM192000Hz{192000U};

	static_assert(Frequency::fromHz(22255) == Frequency::fromHz(22255) && Frequency::fromHz(22255).hz() == 22255U);
	static_assert(!Frequency::fromHz(999).isValid() && !Frequency::fromHz(384001).isValid() && !Frequency::fromHz(-48000).isValid());
	static_assert(Frequency::fromHz(48000) == Frequency::PCM48000Hz && !Frequency::Invalid.isValid());

	/**
	 * @brief Supported audio output container formats.
	 */
	enum class AudioFormat : uint8_t
	{
		WAV,
		FLAC,
		OGG
	};

	/**
	 * @brief The sound container format. Used by StreamIO to select the decode handler when no file
	 * extension is available (FileIO infers it from the extension instead).
	 */
	enum class SoundFileFormat : uint8_t
	{
		Audio,  /**< PCM / compressed audio decoded by libsndfile (WAV, FLAC, OGG, …). */
		MIDI,   /**< Standard MIDI, synthesised through the soundfont — read-only. */
		JSON    /**< Procedural SFX description — read-only. */
	};

	struct Chunk
	{
		size_t offset{0};
		size_t bytes{0};
	};

	/**
	 * @brief Options for reading/decoding audio data.
	 * @note synthesisFrequency and soundfont are used by procedural format handlers (JSON, MIDI).
	 * @note stopToken interrupts a long decode (Ave Robustus II): a handler checks it between two blocks and returns
	 * false once a stop is requested. Default: never stopped.
	 */
	struct ReadOptions
	{
		Frequency synthesisFrequency{Frequency::PCM48000Hz};
		tsf * soundfont{nullptr};
		std::stop_token stopToken;
	};

	/**
	 * @brief Options for writing/encoding audio data.
	 * @note The format field is used by StreamIO to specify the output container
	 * when no file extension is available. FileIO infers format from the file extension.
	 */
	struct WriteOptions
	{
		AudioFormat format{AudioFormat::WAV};
	};

	/**
	 * @brief Converts a channel count number to a corresponding enumeration.
	 * @param channelsCount The channel count.
	 * @return Channels
	 */
	[[nodiscard]]
	inline
	Channels
	toChannels (int channelsCount) noexcept
	{
		switch ( channelsCount )
		{
			case 1 :
				return Channels::Mono;

			case 2 :
				return Channels::Stereo;

			default:
				std::cerr << "toChannels(), " << channelsCount << " channels is not handled !" "\n";

				return Channels::Invalid;
		}
	}
}
