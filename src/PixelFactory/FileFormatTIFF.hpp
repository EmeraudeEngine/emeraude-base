/*
 * src/PixelFactory/FileFormatTIFF.hpp
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
#include <cstdint>
#include <cstring>
#include <vector>

/* Third-party inclusions. */
#include <tiffio.h>

/* Local inclusions for inheritances. */
#include "FileFormatInterface.hpp"
#include "Logging/Logging.hpp"

/* Local inclusions for usages. */
#include "IO/ByteStream.hpp"
#include "Pixmap.hpp"
#include "Types.hpp"

namespace EmEn::Base::PixelFactory
{
	/**
	 * @brief Class for reading the TIFF format via byte streams.
	 *
	 * @note Reading only. TIFF is an INTERCHANGE format here — DCC tools and photogrammetry
	 * pipelines emit it, the engine consumes it — and nothing in this project has a reason to
	 * write one: PNG covers lossless output and HDR covers floating point.
	 *
	 * @note ⚠️ Decoding goes through `TIFFReadRGBAImageOriented()` rather than the strip/tile API.
	 * TIFF is less a format than a container: any bit depth, any photometric interpretation, any
	 * of a dozen compressions, strips or tiles, planar or interleaved. That entry point collapses
	 * all of it to 8-bit RGBA, which is what a texture becomes anyway. The cost is explicit: a
	 * 16-bit source is DOWN-CONVERTED to 8 bits per channel. Should a pipeline ever need the full
	 * precision, that is a separate reader, not a flag on this one.
	 *
	 * @tparam pixel_data_t The pixel component type for the pixmap depth precision. Default uint8_t.
	 * @tparam dimension_t The type of unsigned integer used for pixmap dimension. Default uint32_t.
	 * @extends EmEn::Base::PixelFactory::FileFormatInterface The base IO class.
	 */
	template< typename pixel_data_t = uint8_t, typename dimension_t = uint32_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	class FileFormatTIFF final : public FileFormatInterface< pixel_data_t, dimension_t >
	{
		public:

			FileFormatTIFF () noexcept = default;

			/** @copydoc EmEn::Base::PixelFactory::FileFormatInterface::readStream() */
			[[nodiscard]]
			bool
			readStream (IO::ByteStream & stream, Pixmap< pixel_data_t, dimension_t > & pixmap) noexcept override
			{
				pixmap.clear();

				if constexpr ( !std::is_same_v< pixel_data_t, uint8_t > )
				{
					Logging::error("PixelFactory::FileFormatTIFF", "readStream(), TIFF requires an 8-bit pixmap !");

					return false;
				}
				else
				{
					/* libtiff talks to a client through these callbacks, so the stream stays the
					 * single I/O path of the library — no temporary file, no whole-file copy. */
					auto * handle = TIFFClientOpen(
						"ByteStream", "r", reinterpret_cast< thandle_t >(&stream),
						&FileFormatTIFF::readProc,
						&FileFormatTIFF::writeProc,
						&FileFormatTIFF::seekProc,
						&FileFormatTIFF::closeProc,
						&FileFormatTIFF::sizeProc,
						nullptr, nullptr
					);

					if ( handle == nullptr )
					{
						Logging::error("PixelFactory::FileFormatTIFF", "readStream(), data is not a TIFF stream !");

						return false;
					}

					uint32_t width = 0;
					uint32_t height = 0;

					if ( TIFFGetField(handle, TIFFTAG_IMAGEWIDTH, &width) != 1 || TIFFGetField(handle, TIFFTAG_IMAGELENGTH, &height) != 1 || width == 0 || height == 0 )
					{
						Logging::error("PixelFactory::FileFormatTIFF", "readStream(), unable to read the image dimensions !");

						TIFFClose(handle);

						return false;
					}

					/* ⚠️ The guard is not paranoia: TIFF dimensions come straight from the file, and
					 * the RGBA buffer below is width × height × 4 bytes. A crafted or corrupt header
					 * would otherwise ask for an allocation of arbitrary size. */
					if ( !TIFFRGBAImageOK(handle, nullptr) )
					{
						Logging::error("PixelFactory::FileFormatTIFF", "readStream(), this TIFF cannot be read as RGBA !");

						TIFFClose(handle);

						return false;
					}

					std::vector< uint32_t > raster;

					raster.resize(static_cast< size_t >(width) * static_cast< size_t >(height), 0U);

					if ( raster.size() != static_cast< size_t >(width) * static_cast< size_t >(height) )
					{
						Logging::error("PixelFactory::FileFormatTIFF", "readStream(), unable to allocate the raster !");

						TIFFClose(handle);

						return false;
					}

					/* ORIENTATION_TOPLEFT is what makes the output canonical. The plain
					 * TIFFReadRGBAImage() returns the image BOTTOM-UP, which renders as a
					 * vertically mirrored texture — right shape, wrong content, and nothing in the
					 * log to say so. */
					if ( TIFFReadRGBAImageOriented(handle, width, height, raster.data(), ORIENTATION_TOPLEFT, 0) != 1 )
					{
						Logging::error("PixelFactory::FileFormatTIFF", "readStream(), unable to decode the image !");

						TIFFClose(handle);

						return false;
					}

					TIFFClose(handle);

					if ( !pixmap.initialize(width, height, ChannelMode::RGBA) )
					{
						Logging::error("PixelFactory::FileFormatTIFF", "readStream(), unable to allocate the pixmap !");

						return false;
					}

					/* libtiff packs each pixel into a uint32_t with its own ABGR byte order, which
					 * is NOT the memory layout of an RGBA pixmap. The TIFFGet* accessors are the
					 * only portable way to unpack it — a memcpy would produce channel-swapped
					 * colours on one endianness and correct ones on the other. */
					auto & buffer = pixmap.data();

					for ( size_t index = 0; index < raster.size(); ++index )
					{
						const auto packed = raster[index];
						const auto offset = index * 4;

						buffer[offset] = static_cast< pixel_data_t >(TIFFGetR(packed));
						buffer[offset + 1] = static_cast< pixel_data_t >(TIFFGetG(packed));
						buffer[offset + 2] = static_cast< pixel_data_t >(TIFFGetB(packed));
						buffer[offset + 3] = static_cast< pixel_data_t >(TIFFGetA(packed));
					}

					return true;
				}
			}

			/**
			 * @copydoc EmEn::Base::PixelFactory::FileFormatInterface::writeStream()
			 * @note Always fails: this codec reads TIFF, it does not produce it. See the class note.
			 */
			[[nodiscard]]
			bool
			writeStream (IO::ByteStream & /*stream*/, const Pixmap< pixel_data_t, dimension_t > & /*pixmap*/, const WriteOptions & /*options*/ = {}) const noexcept override
			{
				Logging::error("PixelFactory::FileFormatTIFF", "writeStream(), writing TIFF is not supported. Use PNG for lossless output, or HDR for floating point.");

				return false;
			}

		private:

			static
			tmsize_t
			readProc (thandle_t handle, void * data, tmsize_t size) noexcept
			{
				auto * stream = reinterpret_cast< IO::ByteStream * >(handle);

				if ( stream == nullptr || size <= 0 )
				{
					return 0;
				}

				return stream->read(data, static_cast< size_t >(size)) ? size : 0;
			}

			static
			tmsize_t
			writeProc (thandle_t /*handle*/, void * /*data*/, tmsize_t /*size*/) noexcept
			{
				/* Read-only client: libtiff must never believe it can append to this stream. */
				return 0;
			}

			static
			toff_t
			seekProc (thandle_t handle, toff_t offset, int whence) noexcept
			{
				auto * stream = reinterpret_cast< IO::ByteStream * >(handle);

				if ( stream == nullptr )
				{
					return static_cast< toff_t >(-1);
				}

				const auto position = stream->seek(static_cast< int64_t >(offset), whence);

				if ( position < 0 )
				{
					return static_cast< toff_t >(-1);
				}

				return static_cast< toff_t >(position);
			}

			static
			int
			closeProc (thandle_t /*handle*/) noexcept
			{
				/* The stream is owned by the caller and outlives the TIFF handle. */
				return 0;
			}

			static
			toff_t
			sizeProc (thandle_t handle) noexcept
			{
				const auto * stream = reinterpret_cast< const IO::ByteStream * >(handle);

				if ( stream == nullptr )
				{
					return 0;
				}

				return static_cast< toff_t >(stream->size());
			}
	};
}
