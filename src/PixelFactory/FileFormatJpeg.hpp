/*
 * src/PixelFactory/FileFormatJpeg.hpp
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
#include "IO/ByteStream.hpp"
#include "Pixmap.hpp"
#include "Types.hpp"

namespace EmEn::Base::PixelFactory
{
	/**
	 * @brief Class for read and write JPEG format via byte streams.
	 * @note Every libjpeg call lives in FileFormatJpeg.cpp, and this is a requirement rather than a
	 * matter of taste: an inline codec makes every consumer define libjpeg symbols in its OWN binary,
	 * where — on ELF, whose dynamic namespace is flat — they interpose the system libjpeg used by any
	 * library the process loads. See emeraude-base/cmake/HideThirdPartyExports.cmake.
	 * @tparam pixel_data_t The pixel component type for the pixmap depth precision. Default uint8_t.
	 * @tparam dimension_t The type of unsigned integer used for pixmap dimension. Default uint32_t.
	 * @extends EmEn::Base::PixelFactory::FileFormatInterface The base IO class.
	 */
	template< typename pixel_data_t = uint8_t, typename dimension_t = uint32_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	class FileFormatJpeg final : public FileFormatInterface< pixel_data_t, dimension_t >
	{
		public:

			FileFormatJpeg () noexcept = default;

			/**
			 * @copydoc EmEn::Base::PixelFactory::FileFormatInterface::readStream()
			 * @note Defined in FileFormatJpeg.cpp, which explicitly instantiates this class for the
			 * pixel types libjpeg handles (8-bit only — FileIO already guards the dispatch with an
			 * `if constexpr`). An instantiation the file does not list fails at LINK time, and the
			 * linker names the missing symbol.
			 */
			[[nodiscard]]
			bool readStream (IO::ByteStream & stream, Pixmap< pixel_data_t, dimension_t > & pixmap) noexcept override;

			/** @copydoc EmEn::Base::PixelFactory::FileFormatInterface::writeStream() */
			[[nodiscard]]
			bool writeStream (IO::ByteStream & stream, const Pixmap< pixel_data_t, dimension_t > & pixmap, const WriteOptions & options = {}) const noexcept override;
	};
}
