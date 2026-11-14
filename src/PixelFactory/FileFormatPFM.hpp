/*
 * src/PixelFactory/FileFormatPFM.hpp
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
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

/* Local inclusions. */
#include "FileFormatInterface.hpp"
#include "IO/ByteStream.hpp"
#include "Logging/Logging.hpp"
#include "PixelFactory/Pixmap.hpp"
#include "PixelFactory/Types.hpp"

namespace EmEn::Base::PixelFactory
{
	/**
	 * @brief Portable FloatMap (.pfm) file format: the WRITER of a float pixmap, value for value.
	 * @note The format of Paul Debevec's HDR Shop: a text header — "Pf" (one channel) or "PF" (three), the width and
	 * the height, then a scale whose SIGN gives the byte order (negative: little-endian) — followed by the rows from
	 * the BOTTOM one, each component a 32-bit IEEE 754 float. Nothing is encoded: a signed zero, a subnormal, an
	 * infinity, a NaN are written as they are. It is the file for the values a Radiance file cannot keep (RGBE holds
	 * about 1 % of relative precision: useless for a depth, where 0.990 and 0.995 must differ).
	 * @note Writes little-endian whatever the host. Only a pixmap of floats with one or three channels is written: an
	 * alpha channel has no place in the format and is never dropped in silence, another pixel type is never converted.
	 * @note Reading is NOT implemented (nothing in the cascade reads one): readStream() refuses, and says so. A reader
	 * is a parser of untrusted input, to add with its fuzz target the day a caller exists.
	 * @tparam pixel_data_t The pixel component type for the pixmap depth precision. Default float.
	 * @tparam dimension_t The type of unsigned integer used for pixmap dimension. Default uint32_t.
	 * @extends EmEn::Base::PixelFactory::FileFormatInterface The base IO class.
	 */
	template< typename pixel_data_t = float, typename dimension_t = uint32_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	class FileFormatPFM final : public FileFormatInterface< pixel_data_t, dimension_t >
	{
		public:

			/**
			 * @brief Constructs a Portable FloatMap format IO.
			 */
			FileFormatPFM () noexcept = default;

			/** @copydoc EmEn::Base::PixelFactory::FileFormatInterface::readStream() */
			[[nodiscard]]
			bool
			readStream (IO::ByteStream & /*stream*/, Pixmap< pixel_data_t, dimension_t > & /*pixmap*/) noexcept override
			{
				Logging::error("PixelFactory::FileFormatPFM", "FileFormatPFM::readStream(), reading a PFM file is not supported !");

				return false;
			}

			/** @copydoc EmEn::Base::PixelFactory::FileFormatInterface::writeStream() */
			[[nodiscard]]
			bool
			writeStream ([[maybe_unused]] IO::ByteStream & stream, [[maybe_unused]] const Pixmap< pixel_data_t, dimension_t > & pixmap, [[maybe_unused]] const WriteOptions & options = {}) const noexcept override
			{
				if constexpr ( std::is_same_v< pixel_data_t, float > )
				{
					if ( !pixmap.isValid() )
					{
						Logging::error("PixelFactory::FileFormatPFM", "FileFormatPFM::writeStream(), pixmap parameter is invalid !");

						return false;
					}

					const auto colorCount = static_cast< size_t >(pixmap.colorCount());

					if ( pixmap.channelMode() != ChannelMode::Grayscale && pixmap.channelMode() != ChannelMode::RGB )
					{
						Logging::error("PixelFactory::FileFormatPFM", "FileFormatPFM::writeStream(), only a Grayscale or an RGB pixmap can be written (the format has no alpha channel) !");

						return false;
					}

					const auto width = static_cast< size_t >(pixmap.width());
					const auto height = static_cast< size_t >(pixmap.height());

					/* "Pf": one channel, "PF": three. The negative scale states little-endian floats. */
					const std::string header = std::string{colorCount == 1 ? "Pf" : "PF"} + "\n" + std::to_string(width) + " " + std::to_string(height) + "\n-1.0\n";

					if ( !stream.write(header.data(), header.size()) )
					{
						Logging::error("PixelFactory::FileFormatPFM", "FileFormatPFM::writeStream(), unable to write the header !");

						return false;
					}

					return FileFormatPFM::writeRows(stream, std::span< const float >{pixmap.data()}, {.rowValues = width * colorCount, .rowCount = height, .topRowFirst = options.invertYAxis});
				}
				else
				{
					/* NOTE: Nothing is converted in silence: another pixel type is refused. */
					Logging::error("PixelFactory::FileFormatPFM", "FileFormatPFM::writeStream(), a PFM file holds 32-bit floats: only a pixmap of floats can be written !");

					return false;
				}
			}

		private:

			/** @brief The shifts which take the four bytes of a 32-bit pattern, the least significant first. */
			static constexpr std::array< uint32_t, 4 > ByteShifts{0U, 8U, 16U, 24U};

			/** @brief The rows of a pixmap as the file holds them. */
			struct RowLayout
			{
				/** The number of floats in a row: its width times its channels. */
				size_t rowValues;
				size_t rowCount;
				/** The rows in the pixmap's own order; the format's order is the bottom row first. */
				bool topRowFirst;
			};

			/**
			 * @brief Writes the floats of a pixmap row by row, each little-endian.
			 * @param stream A reference to the output byte stream.
			 * @param values The floats of the pixmap, its top row first.
			 * @param layout A reference to the shape of the rows and their order in the file.
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			writeRows (IO::ByteStream & stream, std::span< const float > values, const RowLayout & layout) noexcept
			{
				std::vector< uint8_t > rowBytes(layout.rowValues * sizeof(float));

				/* The file holds the BOTTOM row first; the pixmap's origin is its top-left corner. */
				for ( size_t rowIndex = 0; rowIndex < layout.rowCount; ++rowIndex )
				{
					const auto sourceRow = layout.topRowFirst ? rowIndex : layout.rowCount - 1 - rowIndex;
					auto output = rowBytes.begin();

					for ( const auto value : values.subspan(sourceRow * layout.rowValues, layout.rowValues) )
					{
						/* Byte by byte from the bit pattern: little-endian whatever the host, and no value is touched. */
						const auto bits = std::bit_cast< uint32_t >(value);

						for ( const auto shift : ByteShifts )
						{
							*output = static_cast< uint8_t >((bits >> shift) & 0xFFU);

							++output;
						}
					}

					if ( !stream.write(rowBytes.data(), rowBytes.size()) )
					{
						Logging::error("PixelFactory::FileFormatPFM", "FileFormatPFM::writeStream(), unable to write the row " + std::to_string(sourceRow) + " !");

						return false;
					}
				}

				return true;
			}
	};
}
