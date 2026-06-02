/*
 * src/WaveFactory/StreamIO.hpp
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
 */

#pragma once

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

/* Local inclusions. */
#include "FileFormatJSON.hpp"
#include "FileFormatMIDI.hpp"
#include "FileFormatSNDFile.hpp"
#include "IO/MemoryStream.hpp"
#include "Logging/Logging.hpp"
#include "Wave.hpp"

namespace EmEn::Base::WaveFactory::StreamIO
{
	/**
	 * @brief Decodes audio data from a memory buffer into a wave.
	 * @note A memory buffer carries no file extension, so the format is selected explicitly. This is
	 * the StreamIO counterpart of FileIO and reaches the same set of formats (the MIDI and JSON
	 * handlers use the soundfont / synthesis frequency from the read options).
	 * @tparam precision_t The sample precision type. Default int16_t.
	 * @param data A reference to the source byte vector.
	 * @param format The sound container format to decode as.
	 * @param wave A reference to the destination wave.
	 * @param options Read options (synthesis frequency, soundfont, etc.).
	 * @return bool
	 */
	template< typename precision_t = int16_t >
	[[nodiscard]]
	bool
	read (const std::vector< std::byte > & data, SoundFileFormat format, Wave< precision_t > & wave, const ReadOptions & options = {}) noexcept
		requires (std::is_arithmetic_v< precision_t >)
	{
		if ( data.empty() )
		{
			Logging::error("WaveFactory::StreamIO", "read(), empty input buffer !");

			return false;
		}

		IO::MemoryStream stream{data};

		switch ( format )
		{
			case SoundFileFormat::Audio :
			{
				FileFormatSNDFile< precision_t > fileFormat;

				return fileFormat.readStream(stream, wave, options);
			}

			case SoundFileFormat::MIDI :
			{
				FileFormatMIDI< precision_t > fileFormat;

				return fileFormat.readStream(stream, wave, options);
			}

			case SoundFileFormat::JSON :
			{
				FileFormatJSON< precision_t > fileFormat;

				return fileFormat.readStream(stream, wave, options);
			}
		}

		Logging::error("WaveFactory::StreamIO", "read(), unhandled format !");

		return false;
	}

	/**
	 * @brief Encodes a wave into a memory buffer.
	 * @note Only libsndfile audio is writable (MIDI and JSON are read-only), so write does not take a
	 * format selector — the output container comes from WriteOptions::format.
	 * @tparam precision_t The sample precision type. Default int16_t.
	 * @param wave A reference to the source wave.
	 * @param output A reference to the destination byte vector. Will be cleared before writing.
	 * @param options Write options (output format, etc.).
	 * @return bool
	 */
	template< typename precision_t = int16_t >
	[[nodiscard]]
	bool
	write (const Wave< precision_t > & wave, std::vector< std::byte > & output, const WriteOptions & options = {}) noexcept
		requires (std::is_arithmetic_v< precision_t >)
	{
		output.clear();

		IO::MemoryStream stream{output};

		FileFormatSNDFile< precision_t > fileFormat;

		return fileFormat.writeStream(stream, wave, options);
	}
}
