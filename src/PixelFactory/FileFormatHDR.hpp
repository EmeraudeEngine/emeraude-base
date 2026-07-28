/*
 * src/PixelFactory/FileFormatHDR.hpp
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
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

/* Local inclusions. */
#include "FileFormatInterface.hpp"

namespace EmEn::Base::PixelFactory
{
	/**
	 * @brief Radiance HDR (.hdr, RGBE) file format for reading and writing a pixmap.
	 * @note The RGBE encoding after Greg Ward, "Real Pixels", Graphics Gems II (1991) —
	 * three 8-bit mantissas sharing one 8-bit exponent, giving a huge dynamic range with
	 * ~1% relative precision. The reader handles the adaptive RLE scanlines (the common
	 * layout), flat scanlines and the legacy RLE; the writer emits flat scanlines. The
	 * decoded values are LINEAR radiances (no transfer function), so this format only
	 * makes sense with a floating-point pixmap: reading into an integral pixmap clamps
	 * to [0,1] and quantizes, losing the whole point of the format.
	 * @tparam pixel_data_t The pixel component type for the pixmap depth precision. Default float.
	 * @tparam dimension_t The type of unsigned integer used for pixmap dimension. Default uint32_t.
	 * @extends EmEn::Base::PixelFactory::FileFormatInterface The base IO class.
	 */
	template< typename pixel_data_t = float, typename dimension_t = uint32_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	class FileFormatHDR final : public FileFormatInterface< pixel_data_t, dimension_t >
	{
		public:

			/**
			 * @brief Constructs a Radiance HDR format IO.
			 */
			FileFormatHDR () noexcept = default;

			/** @copydoc EmEn::Base::PixelFactory::FileFormatInterface::readStream() */
			[[nodiscard]]
			bool
			readStream (IO::ByteStream & stream, Pixmap< pixel_data_t, dimension_t > & pixmap) noexcept override
			{
				/* Header: a magic line, then "KEY=value" lines until an empty line. */
				std::string line = readLine(stream);

				if ( line.rfind("#?", 0) != 0 )
				{
					std::cerr << "FileFormatHDR::readStream(), invalid magic (expected '#?RADIANCE' or '#?RGBE') !" "\n";

					return false;
				}

				bool formatDeclared = false;

				while ( true )
				{
					line = readLine(stream);

					if ( line.empty() )
					{
						break;
					}

					if ( line.rfind("FORMAT=", 0) == 0 )
					{
						if ( line != "FORMAT=32-bit_rle_rgbe" )
						{
							std::cerr << "FileFormatHDR::readStream(), unsupported format '" << line << "' !" "\n";

							return false;
						}

						formatDeclared = true;
					}

					/* NOTE: EXPOSURE / COLORCORR / comments are ignored: HDRI sources are
					 * relative anyway, the engine calibrates them photometrically itself. */
				}

				if ( !formatDeclared )
				{
					std::cerr << "FileFormatHDR::readStream(), no 'FORMAT=32-bit_rle_rgbe' declaration !" "\n";

					return false;
				}

				/* Resolution line: only the canonical "-Y height +X width" orientation
				 * (top-left origin, the format's overwhelming standard) is supported. */
				line = readLine(stream);

				dimension_t width = 0;
				dimension_t height = 0;

				{
					unsigned long rows = 0;
					unsigned long columns = 0;

					if ( std::sscanf(line.c_str(), "-Y %lu +X %lu", &rows, &columns) != 2 || rows == 0 || columns == 0 )
					{
						std::cerr << "FileFormatHDR::readStream(), unsupported resolution line '" << line << "' (expected '-Y h +X w') !" "\n";

						return false;
					}

					width = static_cast< dimension_t >(columns);
					height = static_cast< dimension_t >(rows);
				}

				if ( !pixmap.initialize(width, height, ChannelMode::RGB) )
				{
					std::cerr << "FileFormatHDR::readStream(), unable to initialize the pixmap !" "\n";

					return false;
				}

				std::vector< uint8_t > scanline(static_cast< size_t >(width) * 4);

				for ( dimension_t rowIndex = 0; rowIndex < height; ++rowIndex )
				{
					if ( !readScanline(stream, scanline, width) )
					{
						std::cerr << "FileFormatHDR::readStream(), unable to read the scanline " << rowIndex << " !" "\n";

						return false;
					}

					auto * rowData = pixmap.rowPointer(rowIndex);

					for ( dimension_t x = 0; x < width; ++x )
					{
						const auto * rgbe = scanline.data() + (static_cast< size_t >(x) * 4);

						float red = 0.0F;
						float green = 0.0F;
						float blue = 0.0F;

						if ( rgbe[3] != 0 )
						{
							/* NOTE: (mantissa + 0.5) * 2^(e - 136), Ward's original decoding. */
							const auto factor = std::ldexp(1.0F, static_cast< int >(rgbe[3]) - (128 + 8));

							red = (static_cast< float >(rgbe[0]) + 0.5F) * factor;
							green = (static_cast< float >(rgbe[1]) + 0.5F) * factor;
							blue = (static_cast< float >(rgbe[2]) + 0.5F) * factor;
						}

						if constexpr ( std::is_floating_point_v< pixel_data_t > )
						{
							rowData[static_cast< size_t >(x) * 3 + 0] = static_cast< pixel_data_t >(red);
							rowData[static_cast< size_t >(x) * 3 + 1] = static_cast< pixel_data_t >(green);
							rowData[static_cast< size_t >(x) * 3 + 2] = static_cast< pixel_data_t >(blue);
						}
						else
						{
							/* Integral pixmap: clamp to [0,1] and quantize (degraded, see class note). */
							constexpr auto maxValue = static_cast< float >(std::numeric_limits< pixel_data_t >::max());

							rowData[static_cast< size_t >(x) * 3 + 0] = static_cast< pixel_data_t >(std::clamp(red, 0.0F, 1.0F) * maxValue);
							rowData[static_cast< size_t >(x) * 3 + 1] = static_cast< pixel_data_t >(std::clamp(green, 0.0F, 1.0F) * maxValue);
							rowData[static_cast< size_t >(x) * 3 + 2] = static_cast< pixel_data_t >(std::clamp(blue, 0.0F, 1.0F) * maxValue);
						}
					}
				}

				return true;
			}

			/** @copydoc EmEn::Base::PixelFactory::FileFormatInterface::writeStream() */
			[[nodiscard]]
			bool
			writeStream (IO::ByteStream & stream, const Pixmap< pixel_data_t, dimension_t > & pixmap, const WriteOptions & options = {}) const noexcept override
			{
				if ( !pixmap.isValid() )
				{
					std::cerr << "FileFormatHDR::writeStream(), pixmap parameter is invalid !" "\n";

					return false;
				}

				if ( pixmap.channelMode() != ChannelMode::RGB && pixmap.channelMode() != ChannelMode::RGBA )
				{
					std::cerr << "FileFormatHDR::writeStream(), only RGB/RGBA pixmaps can be written (alpha is dropped) !" "\n";

					return false;
				}

				const auto width = pixmap.width();
				const auto height = pixmap.height();

				const std::string header =
					"#?RADIANCE\n"
					"FORMAT=32-bit_rle_rgbe\n"
					"\n"
					"-Y " + std::to_string(height) + " +X " + std::to_string(width) + "\n";

				if ( !stream.write(header.data(), header.size()) )
				{
					std::cerr << "FileFormatHDR::writeStream(), unable to write the header !" "\n";

					return false;
				}

				const auto colorCount = static_cast< size_t >(pixmap.colorCount());

				/* Flat (non-RLE) scanlines: always valid, simple and robust. */
				std::vector< uint8_t > scanline(static_cast< size_t >(width) * 4);

				for ( dimension_t rowIndex = 0; rowIndex < height; ++rowIndex )
				{
					const auto * rowData = pixmap.rowPointer(options.invertYAxis ? (height - 1 - rowIndex) : rowIndex);

					for ( dimension_t x = 0; x < width; ++x )
					{
						float red;
						float green;
						float blue;

						if constexpr ( std::is_floating_point_v< pixel_data_t > )
						{
							red = static_cast< float >(rowData[static_cast< size_t >(x) * colorCount + 0]);
							green = static_cast< float >(rowData[static_cast< size_t >(x) * colorCount + 1]);
							blue = static_cast< float >(rowData[static_cast< size_t >(x) * colorCount + 2]);
						}
						else
						{
							constexpr auto maxValue = static_cast< float >(std::numeric_limits< pixel_data_t >::max());

							red = static_cast< float >(rowData[static_cast< size_t >(x) * colorCount + 0]) / maxValue;
							green = static_cast< float >(rowData[static_cast< size_t >(x) * colorCount + 1]) / maxValue;
							blue = static_cast< float >(rowData[static_cast< size_t >(x) * colorCount + 2]) / maxValue;
						}

						auto * rgbe = scanline.data() + (static_cast< size_t >(x) * 4);

						const auto maxComponent = std::max(red, std::max(green, blue));

						if ( maxComponent < 1e-32F )
						{
							rgbe[0] = rgbe[1] = rgbe[2] = rgbe[3] = 0;
						}
						else
						{
							int exponent = 0;
							const auto normalized = std::frexp(maxComponent, &exponent);
							const auto scale = normalized * 256.0F / maxComponent;

							rgbe[0] = static_cast< uint8_t >(std::max(0.0F, red) * scale);
							rgbe[1] = static_cast< uint8_t >(std::max(0.0F, green) * scale);
							rgbe[2] = static_cast< uint8_t >(std::max(0.0F, blue) * scale);
							rgbe[3] = static_cast< uint8_t >(exponent + 128);
						}
					}

					if ( !stream.write(scanline.data(), scanline.size()) )
					{
						std::cerr << "FileFormatHDR::writeStream(), unable to write the scanline " << rowIndex << " !" "\n";

						return false;
					}
				}

				return true;
			}

		private:

			/**
			 * @brief Reads a text line (up to '\n', not included) from the stream.
			 * @param stream A reference to the input byte stream.
			 * @return std::string
			 */
			[[nodiscard]]
			static
			std::string
			readLine (IO::ByteStream & stream) noexcept
			{
				std::string line;
				char character = 0;

				/* NOTE: A Radiance header line is short; 256 bytes is a sanity bound. */
				while ( line.size() < 256 && stream.read(&character, 1) && character != '\n' )
				{
					line += character;
				}

				return line;
			}

			/**
			 * @brief Reads one scanline of RGBE pixels, handling the three encodings.
			 * @param stream A reference to the input byte stream.
			 * @param scanline The destination buffer (width * 4 bytes).
			 * @param width The scanline width in pixels.
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			readScanline (IO::ByteStream & stream, std::vector< uint8_t > & scanline, dimension_t width) noexcept
			{
				std::array< uint8_t, 4 > head{};

				if ( !stream.read(head.data(), 4) )
				{
					return false;
				}

				/* Adaptive RLE: header (2, 2, widthHigh, widthLow), then the four component
				 * planes in sequence, each RLE-compressed independently. */
				if ( head[0] == 2 && head[1] == 2 && (static_cast< dimension_t >(head[2]) << 8 | head[3]) == width && width >= 8 && width < 32768 )
				{
					for ( size_t component = 0; component < 4; ++component )
					{
						dimension_t x = 0;

						while ( x < width )
						{
							uint8_t code = 0;

							if ( !stream.read(&code, 1) )
							{
								return false;
							}

							if ( code > 128 )
							{
								/* A run: the next byte repeated (code - 128) times. */
								const dimension_t count = code - 128;

								if ( x + count > width )
								{
									return false;
								}

								uint8_t value = 0;

								if ( !stream.read(&value, 1) )
								{
									return false;
								}

								for ( dimension_t i = 0; i < count; ++i )
								{
									scanline[static_cast< size_t >(x + i) * 4 + component] = value;
								}

								x += count;
							}
							else
							{
								/* A literal span of 'code' bytes. */
								const dimension_t count = code;

								if ( count == 0 || x + count > width )
								{
									return false;
								}

								for ( dimension_t i = 0; i < count; ++i )
								{
									uint8_t value = 0;

									if ( !stream.read(&value, 1) )
									{
										return false;
									}

									scanline[static_cast< size_t >(x + i) * 4 + component] = value;
								}

								x += count;
							}
						}
					}

					return true;
				}

				/* Flat or legacy-RLE scanline: the four bytes already read are the first pixel. */
				scanline[0] = head[0];
				scanline[1] = head[1];
				scanline[2] = head[2];
				scanline[3] = head[3];

				dimension_t x = 1;
				size_t repeatShift = 0;

				while ( x < width )
				{
					std::array< uint8_t, 4 > pixel{};

					if ( !stream.read(pixel.data(), 4) )
					{
						return false;
					}

					/* Legacy RLE: (1, 1, 1, count) repeats the previous pixel count << shift times. */
					if ( pixel[0] == 1 && pixel[1] == 1 && pixel[2] == 1 )
					{
						const auto count = static_cast< dimension_t >(pixel[3]) << repeatShift;

						if ( x == 0 || x + count > width )
						{
							return false;
						}

						for ( dimension_t i = 0; i < count; ++i )
						{
							scanline[static_cast< size_t >(x + i) * 4 + 0] = scanline[static_cast< size_t >(x - 1) * 4 + 0];
							scanline[static_cast< size_t >(x + i) * 4 + 1] = scanline[static_cast< size_t >(x - 1) * 4 + 1];
							scanline[static_cast< size_t >(x + i) * 4 + 2] = scanline[static_cast< size_t >(x - 1) * 4 + 2];
							scanline[static_cast< size_t >(x + i) * 4 + 3] = scanline[static_cast< size_t >(x - 1) * 4 + 3];
						}

						x += count;
						repeatShift += 8;
					}
					else
					{
						scanline[static_cast< size_t >(x) * 4 + 0] = pixel[0];
						scanline[static_cast< size_t >(x) * 4 + 1] = pixel[1];
						scanline[static_cast< size_t >(x) * 4 + 2] = pixel[2];
						scanline[static_cast< size_t >(x) * 4 + 3] = pixel[3];

						++x;
						repeatShift = 0;
					}
				}

				return true;
			}
	};
}
