/*
 * src/WaveFactory/FileFormatSNDFile.hpp
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
#include <type_traits>

/* Local inclusions for inheritances. */
#include "FileFormatInterface.hpp"

/* Local inclusions for usages. */
#include "Logging/Logging.hpp"
#include "Types.hpp"
#include "Wave.hpp"

namespace EmEn::Base::WaveFactory
{
	/**
	 * @brief Class for reading and writing audio formats via libsndfile.
	 * @tparam precision_t The sample precision type. Default int16_t.
	 * @extends EmEn::Base::WaveFactory::FileFormatInterface The base IO class.
	 * @note Supports WAV, FLAC, OGG, AIFF, and many other formats.
	 */
	template< typename precision_t = int16_t >
	requires (std::is_arithmetic_v< precision_t >)
	class FileFormatSNDFile final : public FileFormatInterface< precision_t >
	{
		public:

			FileFormatSNDFile () noexcept = default;

			/** @copydoc EmEn::Base::WaveFactory::FileFormatInterface::readStream() */
			[[nodiscard]]
			bool
			readStream (IO::ByteStream & /*stream*/, Wave< precision_t > & /*wave*/, const ReadOptions & /*options*/) noexcept override
			{
				Logging::error("WaveFactory::FileFormatSNDFile", "readStream(), precision format not handled !");

				return false;
			}

			/** @copydoc EmEn::Base::WaveFactory::FileFormatInterface::writeStream() */
			[[nodiscard]]
			bool
			writeStream (IO::ByteStream & /*stream*/, const Wave< precision_t > & /*wave*/, const WriteOptions & /*options*/) const noexcept override
			{
				Logging::error("WaveFactory::FileFormatSNDFile", "writeStream(), precision format not handled !");

				return false;
			}
	};

	/**
	 * @brief Specialization for int16_t (16-bit PCM audio).
	 * @note Both methods are defined in FileFormatSNDFile.cpp, along with the SF_VIRTUAL_IO
	 * callbacks: libsndfile — and the FLAC/ogg/vorbis/opus archives behind it — must not reach a
	 * consumer's binary, where those symbols would interpose the system libraries used by anything
	 * the process loads. See emeraude-base/cmake/HideThirdPartyExports.cmake. Being a full
	 * specialization rather than a template, it needs no explicit instantiation.
	 */
	template<>
	class FileFormatSNDFile< int16_t > final : public FileFormatInterface< int16_t >
	{
		public:

			FileFormatSNDFile () noexcept = default;

			/** @copydoc EmEn::Base::WaveFactory::FileFormatInterface::readStream() */
			[[nodiscard]]
			bool readStream (IO::ByteStream & stream, Wave< int16_t > & wave, const ReadOptions & options) noexcept override;

			/** @copydoc EmEn::Base::WaveFactory::FileFormatInterface::writeStream() */
			[[nodiscard]]
			bool writeStream (IO::ByteStream & stream, const Wave< int16_t > & wave, const WriteOptions & options) const noexcept override;
	};
}
