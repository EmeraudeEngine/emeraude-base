/*
 * src/PixelFactory/FileFormatTarga.hpp
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

/* Project configuration. */
#include "emeraude_base_config.hpp"

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>

/* Local inclusions for inheritances. */
#include "FileFormatInterface.hpp"

/* Local inclusions for usages. */
#include "IO/ByteStream.hpp"
#include "Logging/Logging.hpp"
#include "Processor.hpp"

namespace EmEn::Base::PixelFactory
{
	/**
	 * @brief Class for read and write Targa format via byte streams.
	 * @tparam pixel_data_t The pixel component type for the pixmap depth precision. Default uint8_t.
	 * @tparam dimension_t The type of unsigned integer used for pixmap dimension. Default uint32_t.
	 * @extends EmEn::Base::PixelFactory::FileFormatInterface The base IO class.
	 */
	template< typename pixel_data_t = uint8_t, typename dimension_t = uint32_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	class FileFormatTarga final : public FileFormatInterface< pixel_data_t, dimension_t >
	{
		public:

			FileFormatTarga () noexcept = default;

			/** @copydoc EmEn::Base::PixelFactory::FileFormatInterface::readStream() */
			[[nodiscard]]
			bool
			readStream (IO::ByteStream & stream, Pixmap< pixel_data_t, dimension_t > & pixmap) noexcept override
			{
				pixmap.clear();

				if constexpr ( !std::is_same_v< pixel_data_t, uint8_t > )
				{
					Logging::error("PixelFactory::FileFormatTarga", "readStream(), Targa requires an 8-bit pixmap !");

					return false;
				}
				else
				{
					Header fileHeader{};

					/* Read the TARGA header, field by field. */
					for ( const auto & field : FileFormatTarga::headerFields(fileHeader) )
					{
						if ( !stream.read(field.data, field.bytes) )
						{
							Logging::error("PixelFactory::FileFormatTarga", "readStream(), unable to read the Targa header !");

							return false;
						}
					}

					/* Guard the declared dimensions against the stream payload BEFORE allocating the pixmap.
					 * width/height are 16-bit, so an unvalidated header can request width*height*channels up
					 * to ~17 GB (found by fuzz_targa: a 25-byte file declaring 65535x65535 -> OOM). Even a
					 * maximally RLE-compressed Targa expands by at most ~128:1, so a pixel count exceeding the
					 * remaining bytes by more than that factor cannot be backed by this stream -> cancel the load. */
					{
						constexpr uint64_t headerBytes = 18;
						constexpr uint64_t maxRLEExpansion = 128;

						const uint64_t streamSize = stream.size();

						if ( streamSize < headerBytes )
						{
							Logging::error("PixelFactory::FileFormatTarga", "readStream(), stream smaller than its own header !");

							return false;
						}

						const uint64_t pixelCount = static_cast< uint64_t >(fileHeader.width) * static_cast< uint64_t >(fileHeader.height);
						const uint64_t payloadBytes = streamSize - headerBytes;

						if ( fileHeader.width == 0 || fileHeader.height == 0 || pixelCount > payloadBytes * maxRLEExpansion )
						{
							Logging::error("PixelFactory::FileFormatTarga", "readStream(), declared dimensions exceed the stream payload !");

							return false;
						}
					}

					if constexpr ( PixelFactoryDebugEnabled )
					{
						std::cout <<
							"[TARGA_DEBUG] Reading header.\n" <<
							"\t" "idCharCount : " << static_cast< int >(fileHeader.idCharCount) << "\n"
							"\t" "colorMapType : " << static_cast< int >(fileHeader.colorMapType) << "\n"
							"\t" "imageTypeCode : " << static_cast< int >(fileHeader.imageTypeCode) << "\n"
							"\t" "colorMapOrigin : " << static_cast< int >(fileHeader.colorMapOrigin) << "\n"
							"\t" "colorMapLength : " << static_cast< int >(fileHeader.colorMapLength) << "\n"
							"\t" "colorMapEntrySize : " << static_cast< int >(fileHeader.colorMapEntrySize) << "\n"
							"\t" "xOrigin : " << static_cast< int >(fileHeader.xOrigin) << "\n"
							"\t" "yOrigin : " << static_cast< int >(fileHeader.yOrigin) << "\n"
							"\t" "width : " << static_cast< int >(fileHeader.width) << "\n"
							"\t" "height : " << static_cast< int >(fileHeader.height) << "\n"
							"\t" "imagePixelSize : " << static_cast< int >(fileHeader.imagePixelSize) << "\n"
							"\t" "imageDescriptorByte : " << static_cast< int >(fileHeader.imageDescriptorByte) << '\n';
					}

					/* The image kind and its pixel encoding (TGA 2.0). Every variant the format defines for these three
					 * kinds is decoded; until 2026-10-17 the colour-mapped, 15/16-bit and 16-bit grayscale ones were
					 * ACCEPTED and decoded as garbage (owner decision: implement them). */
					ImageKind kind{};
					auto RLE = false;

					switch ( fileHeader.imageTypeCode )
					{
						case 9 :
							RLE = true;

							[[fallthrough]];

						case 1 :
							kind = ImageKind::ColorMapped;
							break;

						case 10 :
							RLE = true;

							[[fallthrough]];

						case 2 :
							kind = ImageKind::TrueColor;
							break;

						case 11 :
							RLE = true;

							[[fallthrough]];

						case 3 :
							kind = ImageKind::Grayscale;
							break;

						case 0 :
							Logging::error("PixelFactory::FileFormatTarga", "readStream(), no pixel data !");

							return false;

						default :
							Logging::error("PixelFactory::FileFormatTarga", "readStream(), unhandled type of Targa file !");

							return false;
					}

					const auto depth = fileHeader.imagePixelSize;
					const auto alphaBits = static_cast< uint8_t >(fileHeader.imageDescriptorByte & 0x0FU);

					if ( !FileFormatTarga::isDepthValid(kind, depth) )
					{
						Logging::error("PixelFactory::FileFormatTarga", "readStream(), unsupported pixel depth !");

						return false;
					}

					if ( fileHeader.colorMapType > 1 )
					{
						Logging::error("PixelFactory::FileFormatTarga", "readStream(), unknown colour map type !");

						return false;
					}

					if ( kind == ImageKind::ColorMapped && (fileHeader.colorMapType != 1 || fileHeader.colorMapLength == 0 || !FileFormatTarga::isTrueColorDepthValid(fileHeader.colorMapEntrySize)) )
					{
						Logging::error("PixelFactory::FileFormatTarga", "readStream(), a colour-mapped image without a valid colour map !");

						return false;
					}

					/* The identification field: skipped. */
					if ( fileHeader.idCharCount > 0 )
					{
						std::array< uint8_t, 255 > identification{};

						if ( !stream.read(identification.data(), fileHeader.idCharCount) )
						{
							Logging::error("PixelFactory::FileFormatTarga", "readStream(), unable to read the Targa identification !");

							return false;
						}
					}

					/* The colour map: decoded for a colour-mapped image, SKIPPED for the others (the specification allows one
					 * beside true-colour or grayscale data; it used to be read as pixels). */
					std::vector< std::array< uint8_t, 4 > > palette;

					if ( fileHeader.colorMapType == 1 && fileHeader.colorMapLength > 0 )
					{
						const auto entryBytes = static_cast< size_t >((fileHeader.colorMapEntrySize + 7U) / 8U);
						std::vector< uint8_t > colorMapData(static_cast< size_t >(fileHeader.colorMapLength) * entryBytes);

						if ( !colorMapData.empty() && !stream.read(colorMapData.data(), colorMapData.size()) )
						{
							Logging::error("PixelFactory::FileFormatTarga", "readStream(), unable to read the colour map !");

							return false;
						}

						if ( kind == ImageKind::ColorMapped )
						{
							palette.resize(fileHeader.colorMapLength);

							for ( size_t index = 0; index < palette.size(); ++index )
							{
								palette[index] = FileFormatTarga::decodeTrueColor(&colorMapData[index * entryBytes], fileHeader.colorMapEntrySize, alphaBits);
							}
						}
					}

					const auto width = static_cast< size_t >(fileHeader.width);
					const auto height = static_cast< size_t >(fileHeader.height);
					const auto totalPixels = width * height;
					const auto pixelBytes = static_cast< size_t >((depth + 7U) / 8U);

					/* Uncompressed data cannot exceed the stream: refused before any allocation (the ×128 guard above only
					 * bounds an RLE stream). */
					if ( !RLE && totalPixels * pixelBytes > stream.size() - 18U )
					{
						Logging::error("PixelFactory::FileFormatTarga", "readStream(), the uncompressed pixel data exceeds the stream !");

						return false;
					}

					const auto channelMode = FileFormatTarga::channelModeOf(kind, kind == ImageKind::ColorMapped ? fileHeader.colorMapEntrySize : depth, alphaBits);

					if ( !pixmap.initialize(fileHeader.width, fileHeader.height, channelMode) )
					{
						return false;
					}

					/* Every pixel lands at its final place: the descriptor says the order of the file (bit 5: rows top to
					 * bottom, bit 4: columns right to left — the specification's flags; this reader used to decide the
					 * vertical order from yOrigin, which loaded a top-left file with yOrigin 0 upside down). */
					const auto channels = pixmap.template colorCount< size_t >();
					const bool topToBottom = (fileHeader.imageDescriptorByte & 0x20U) != 0;
					const bool rightToLeft = (fileHeader.imageDescriptorByte & 0x10U) != 0;
					auto & pixmapData = pixmap.data();

					const auto store = [&] (size_t pixelIndex, const uint8_t * raw) noexcept -> bool {
						std::array< uint8_t, 4 > rgba{};

						switch ( kind )
						{
							case ImageKind::TrueColor :
								rgba = FileFormatTarga::decodeTrueColor(raw, depth, alphaBits);
								break;

							case ImageKind::Grayscale :
								/* 8 bits: gray; 16 bits: gray then alpha — already in the pixmap's channel order. */
								rgba = {raw[0], depth == 16 ? raw[1] : uint8_t{255}, 0, 0};
								break;

							case ImageKind::ColorMapped :
							{
								const auto index = static_cast< size_t >(depth == 16 ? (raw[0] | (raw[1] << 8U)) : raw[0]);

								/* The first entry index (colorMapOrigin) offsets the indices; one outside the map is a corrupt file. */
								if ( index < fileHeader.colorMapOrigin || index - fileHeader.colorMapOrigin >= palette.size() )
								{
									return false;
								}

								rgba = palette[index - fileHeader.colorMapOrigin];
							}
								break;
						}

						const auto column = pixelIndex % width;
						const auto row = pixelIndex / width;
						const auto x = rightToLeft ? width - 1 - column : column;
						const auto y = topToBottom ? row : height - 1 - row;
						const auto offset = ((y * width) + x) * channels;

						std::copy_n(rgba.cbegin(), channels, &pixmapData[offset]);

						return true;
					};

					if ( RLE )
					{
						/* A packet: one header byte, then one pixel repeated (bit 7 set) or 1 to 128 raw pixels. */
						std::array< uint8_t, 128 * 4 > packet{};
						size_t pixelIndex = 0;

						while ( pixelIndex < totalPixels )
						{
							uint8_t packetHeader = 0;

							if ( !stream.read(&packetHeader, 1) )
							{
								Logging::error("PixelFactory::FileFormatTarga", "readStream(), unable to read RLE packet header !");

								return false;
							}

							const bool isRunPacket = (packetHeader & 0x80U) != 0;
							const auto count = static_cast< size_t >(packetHeader & 0x7FU) + 1U;
							const auto bytesToRead = isRunPacket ? pixelBytes : count * pixelBytes;

							if ( !stream.read(packet.data(), bytesToRead) )
							{
								Logging::error("PixelFactory::FileFormatTarga", "readStream(), unable to read RLE pixel data !");

								return false;
							}

							for ( size_t index = 0; index < count && pixelIndex < totalPixels; ++index, ++pixelIndex )
							{
								if ( !store(pixelIndex, isRunPacket ? packet.data() : packet.data() + (index * pixelBytes)) )
								{
									Logging::error("PixelFactory::FileFormatTarga", "readStream(), a colour index outside the colour map !");

									pixmap.clear();

									return false;
								}
							}
						}
					}
					else
					{
						std::vector< uint8_t > rawData(totalPixels * pixelBytes);

						if ( !stream.read(rawData.data(), rawData.size()) )
						{
							Logging::error("PixelFactory::FileFormatTarga", "readStream(), unable to read the Targa data !");

							pixmap.clear();

							return false;
						}

						for ( size_t pixelIndex = 0; pixelIndex < totalPixels; ++pixelIndex )
						{
							if ( !store(pixelIndex, &rawData[pixelIndex * pixelBytes]) )
							{
								Logging::error("PixelFactory::FileFormatTarga", "readStream(), a colour index outside the colour map !");

								pixmap.clear();

								return false;
							}
						}
					}

					return true;
				}
			}

			/** @copydoc EmEn::Base::PixelFactory::FileFormatInterface::writeStream() */
			[[nodiscard]]
			bool
			writeStream (IO::ByteStream & stream, const Pixmap< pixel_data_t, dimension_t > & pixmap, const WriteOptions & options = {}) const noexcept override
			{
				if ( !pixmap.isValid() )
				{
					std::cerr << "FileFormatTarga::writeStream(), pixmap parameter is invalid !" "\n";

					return false;
				}

				const auto useRLE = options.targa.rleCompression;

				Header fileHeader{};

				fileHeader.idCharCount = 0;
				/* TGA default: bottom-left origin. */
				fileHeader.yOrigin = 0;
				fileHeader.width = static_cast< uint16_t >(pixmap.width());
				fileHeader.height = static_cast< uint16_t >(pixmap.height());
				fileHeader.imagePixelSize = static_cast< uint8_t >(pixmap.bitPerPixel());
				fileHeader.imageDescriptorByte = (pixmap.channelMode() == ChannelMode::RGBA || pixmap.channelMode() == ChannelMode::GrayscaleAlpha) ? 8 : 0;

				switch ( pixmap.channelMode() )
				{
					case ChannelMode::Grayscale :
						fileHeader.imageTypeCode = useRLE ? 11 : 3;
						break;

					case ChannelMode::RGB :
					case ChannelMode::RGBA :
						fileHeader.imageTypeCode = useRLE ? 10 : 2;
						break;

					case ChannelMode::GrayscaleAlpha :
						std::cerr << "FileFormatTarga::writeStream(), unhandled color channel format to write a Targa image." "\n";

						return false;
				}

				/* An out-of-range channel mode (cast from data) reaches no case above and leaves the
				 * type at 0, which is TGA's "no image data". */
				if ( fileHeader.imageTypeCode == 0 )
				{
					std::cerr << "FileFormatTarga::writeStream(), unhandled color channel format to write a Targa image." "\n";

					return false;
				}


				if constexpr ( PixelFactoryDebugEnabled )
				{
					std::cout <<
						"[TARGA_DEBUG] Writing header.\n" <<
						"\t" "idCharCount : " << static_cast< int >(fileHeader.idCharCount) << "\n"
						"\t" "colorMapType : " << static_cast< int >(fileHeader.colorMapType) << "\n"
						"\t" "imageTypeCode : " << static_cast< int >(fileHeader.imageTypeCode) << "\n"
						"\t" "colorMapOrigin : " << static_cast< int >(fileHeader.colorMapOrigin) << "\n"
						"\t" "colorMapLength : " << static_cast< int >(fileHeader.colorMapLength) << "\n"
						"\t" "colorMapEntrySize : " << static_cast< int >(fileHeader.colorMapEntrySize) << "\n"
						"\t" "xOrigin : " << static_cast< int >(fileHeader.xOrigin) << "\n"
						"\t" "yOrigin : " << static_cast< int >(fileHeader.yOrigin) << "\n"
						"\t" "width : " << static_cast< int >(fileHeader.width) << "\n"
						"\t" "height : " << static_cast< int >(fileHeader.height) << "\n"
						"\t" "imagePixelSize : " << static_cast< int >(fileHeader.imagePixelSize) << "\n"
						"\t" "imageDescriptorByte : " << static_cast< int >(fileHeader.imageDescriptorByte) << '\n';
				}

				/* Write the Targa header, field by field. */
				for ( const auto & field : FileFormatTarga::headerFields(fileHeader) )
				{
					if ( !stream.write(field.data, field.bytes) )
					{
						std::cerr << "FileFormatTarga::writeStream(), unable to write the Targa header !" "\n";

						return false;
					}
				}

				/* Prepare pixmap: RGB->BGR and orientation for TGA convention. */
				Pixmap< pixel_data_t, dimension_t > processedPixmap;

				if ( pixmap.colorCount() > 1 )
				{
					processedPixmap = Processor< pixel_data_t, dimension_t >::swapChannels(pixmap);

					/* TGA expects bottom-left origin. Flip unless input is already bottom-left. */
					if ( !options.invertYAxis )
					{
						processedPixmap = Processor< pixel_data_t, dimension_t >::mirror(processedPixmap, MirrorMode::X);
					}
				}
				else
				{
					if ( options.invertYAxis )
					{
						processedPixmap = pixmap;
					}
					else
					{
						processedPixmap = Processor< pixel_data_t, dimension_t >::mirror(pixmap, MirrorMode::X);
					}
				}

				/* Write data. */
				if ( useRLE )
				{
					if ( !writeRLEData(stream, processedPixmap) )
					{
						std::cerr << "FileFormatTarga::writeStream(), unable to write the Targa RLE data !" "\n";

						return false;
					}
				}
				else
				{
					if ( !stream.write(processedPixmap.data().data(), processedPixmap.bytes()) )
					{
						std::cerr << "FileFormatTarga::writeStream(), unable to write the Targa data !" "\n";

						return false;
					}
				}

				return true;
			}

		private:

			/**
			 * @brief Writes pixmap data using RLE compression.
			 * @param stream Output byte stream.
			 * @param pixmap The source pixmap to compress.
			 * @return bool True if successful.
			 */
			[[nodiscard]]
			static
			bool
			writeRLEData (IO::ByteStream & stream, const Pixmap< pixel_data_t, dimension_t > & pixmap) noexcept
			{
				const auto & pixmapData = pixmap.data();
				const auto totalPixels = static_cast< size_t >(pixmap.width()) * static_cast< size_t >(pixmap.height());
				const auto bytesPerPixel = pixmap.template colorCount< size_t >();
				size_t currentPixel = 0;

				while ( currentPixel < totalPixels )
				{
					size_t runLength = 1;
					constexpr size_t maxRunLength{128};
					const auto currentOffset = currentPixel * bytesPerPixel;

					/* Check for RLE run (repeated pixels). */
					bool isRLE = false;

					if ( currentPixel + 1 < totalPixels )
					{
						const auto nextOffset = (currentPixel + 1) * bytesPerPixel;
						bool pixelsMatch = true;

						for ( size_t c = 0; c < bytesPerPixel; ++c )
						{
							if ( pixmapData[currentOffset + c] != pixmapData[nextOffset + c] )
							{
								pixelsMatch = false;
								break;
							}
						}

						if ( pixelsMatch )
						{
							isRLE = true;

							while ( runLength < maxRunLength && currentPixel + runLength < totalPixels )
							{
								const auto checkOffset = (currentPixel + runLength) * bytesPerPixel;
								bool stillMatching = true;

								for ( size_t c = 0; c < bytesPerPixel; ++c )
								{
									if ( pixmapData[currentOffset + c] != pixmapData[checkOffset + c] )
									{
										stillMatching = false;
										break;
									}
								}

								if ( !stillMatching )
								{
									break;
								}

								++runLength;
							}
						}
					}

					if ( isRLE )
					{
						const auto packetHeader = static_cast< uint8_t >(0x80 | (runLength - 1));

						if ( !stream.write(&packetHeader, 1) )
						{
							return false;
						}

						if ( !stream.write(&pixmapData[currentOffset], bytesPerPixel) )
						{
							return false;
						}
					}
					else
					{
						while ( runLength < maxRunLength && currentPixel + runLength < totalPixels )
						{
							const auto checkOffset = (currentPixel + runLength) * bytesPerPixel;
							const auto nextCheckOffset = (currentPixel + runLength + 1) * bytesPerPixel;

							if ( currentPixel + runLength + 1 >= totalPixels )
							{
								++runLength;
								break;
							}

							bool nextPixelsMatch = true;

							for ( size_t c = 0; c < bytesPerPixel; ++c )
							{
								if ( pixmapData[checkOffset + c] != pixmapData[nextCheckOffset + c] )
								{
									nextPixelsMatch = false;
									break;
								}
							}

							if ( nextPixelsMatch )
							{
								break;
							}

							++runLength;
						}

						const auto packetHeader = static_cast< uint8_t >(runLength - 1);

						if ( !stream.write(&packetHeader, 1) )
						{
							return false;
						}

						if ( !stream.write(&pixmapData[currentOffset], bytesPerPixel * runLength) )
						{
							return false;
						}
					}

					currentPixel += runLength;
				}

				return true;
			}

			/** @brief The three kinds of Targa image data. */
			enum class ImageKind : uint8_t
			{
				ColorMapped,
				TrueColor,
				Grayscale
			};

			/**
			 * @brief Returns whether a true-colour depth (an image's or a colour map entry's) is one the format defines.
			 * @param depth The depth in bits.
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			isTrueColorDepthValid (uint8_t depth) noexcept
			{
				return depth == 15 || depth == 16 || depth == 24 || depth == 32;
			}

			/**
			 * @brief Returns whether a pixel depth is valid for a kind of image: an index of 8 or 16 bits, a true colour of
			 * 15 / 16 / 24 / 32 bits, a gray of 8 bits or gray + alpha of 16 bits.
			 * @param kind The kind of image.
			 * @param depth The depth in bits.
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			isDepthValid (ImageKind kind, uint8_t depth) noexcept
			{
				switch ( kind )
				{
					case ImageKind::ColorMapped :
						return depth == 8 || depth == 16;

					case ImageKind::TrueColor :
						return FileFormatTarga::isTrueColorDepthValid(depth);

					case ImageKind::Grayscale :
						return depth == 8 || depth == 16;
				}

				return false;
			}

			/**
			 * @brief Returns the pixmap channel mode of a kind of image.
			 * @param kind The kind of image.
			 * @param colorDepth The depth of the colour: the pixel's (true colour, grayscale) or the colour map entry's.
			 * @param alphaBits The attribute (alpha) bits of the image descriptor.
			 * @return ChannelMode
			 */
			[[nodiscard]]
			static
			ChannelMode
			channelModeOf (ImageKind kind, uint8_t colorDepth, uint8_t alphaBits) noexcept
			{
				if ( kind == ImageKind::Grayscale )
				{
					return colorDepth == 16 ? ChannelMode::GrayscaleAlpha : ChannelMode::Grayscale;
				}

				if ( colorDepth == 32 || (colorDepth == 16 && alphaBits > 0) )
				{
					return ChannelMode::RGBA;
				}

				return ChannelMode::RGB;
			}

			/**
			 * @brief Decodes one true-colour value (BGR, BGRA, or A1R5G5B5 little-endian) to R, G, B, A.
			 * @note 5-bit channels expand to 8 bits by bit replication (0 -> 0, 31 -> 255). The alpha bit of a 16-bit value
			 * counts only when the descriptor declares an alpha bit; a 15-bit value is opaque.
			 * @param raw The bytes of the value.
			 * @param depth Its depth in bits (15, 16, 24, 32).
			 * @param alphaBits The attribute (alpha) bits of the image descriptor.
			 * @return std::array< uint8_t, 4 >
			 */
			[[nodiscard]]
			static
			std::array< uint8_t, 4 >
			decodeTrueColor (const uint8_t * raw, uint8_t depth, uint8_t alphaBits) noexcept
			{
				switch ( depth )
				{
					case 15 :
					case 16 :
					{
						const auto value = static_cast< uint32_t >(raw[0] | (raw[1] << 8U));
						const auto expand = [] (uint32_t channel) noexcept {
							return static_cast< uint8_t >((channel << 3U) | (channel >> 2U));
						};
						const bool transparent = depth == 16 && alphaBits > 0 && (value & 0x8000U) == 0;

						return {expand((value >> 10U) & 0x1FU), expand((value >> 5U) & 0x1FU), expand(value & 0x1FU), transparent ? uint8_t{0} : uint8_t{255}};
					}

					case 24 :
						return {raw[2], raw[1], raw[0], 255};

					case 32 :
						return {raw[2], raw[1], raw[0], raw[3]};

					default :
						return {0, 0, 0, 255};
				}
			}

			struct Header
			{
				uint8_t idCharCount = 0;
				uint8_t colorMapType = 0;
				uint8_t imageTypeCode = 0;
				uint16_t colorMapOrigin = 0;
				uint16_t colorMapLength = 0;
				uint8_t colorMapEntrySize = 0;
				uint16_t xOrigin = 0;
				uint16_t yOrigin = 0;
				uint16_t width = 0;
				uint16_t height = 0;
				uint8_t imagePixelSize = 0;
				uint8_t imageDescriptorByte = 0;
			};

			/** @brief One header field: where it lives and how many bytes the file stores for it. */
			struct HeaderField
			{
				void * data;
				uint32_t bytes;
			};

			/**
			 * @brief Returns the 12 header fields in file order (the file stores them packed, field by field).
			 * @param header A writable reference to the header.
			 * @return std::array< HeaderField, 12 >
			 */
			[[nodiscard]]
			static
			std::array< HeaderField, 12 >
			headerFields (Header & header) noexcept
			{
				return {{
					{&header.idCharCount, 1},
					{&header.colorMapType, 1},
					{&header.imageTypeCode, 1},
					{&header.colorMapOrigin, 2},
					{&header.colorMapLength, 2},
					{&header.colorMapEntrySize, 1},
					{&header.xOrigin, 2},
					{&header.yOrigin, 2},
					{&header.width, 2},
					{&header.height, 2},
					{&header.imagePixelSize, 1},
					{&header.imageDescriptorByte, 1}
				}};
			}
	};
}
