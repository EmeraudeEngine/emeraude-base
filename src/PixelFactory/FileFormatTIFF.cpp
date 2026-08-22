/*
 * src/PixelFactory/FileFormatTIFF.cpp
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

#include "FileFormatTIFF.hpp"

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <vector>

/* Third-party inclusions. */
#include <tiffio.h>

/* Local inclusions. */
#include "Logging/Logging.hpp"

namespace EmEn::Base::PixelFactory
{
	namespace
	{
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

		tmsize_t
		writeProc (thandle_t /*handle*/, void * /*data*/, tmsize_t /*size*/) noexcept
		{
			/* Read-only client: libtiff must never believe it can append to this stream. */
			return 0;
		}

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

		int
		closeProc (thandle_t /*handle*/) noexcept
		{
			/* The stream is owned by the caller and outlives the TIFF handle. */
			return 0;
		}

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
	}

	template< typename pixel_data_t, typename dimension_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	bool
	FileFormatTIFF< pixel_data_t, dimension_t >::readStream (IO::ByteStream & stream, Pixmap< pixel_data_t, dimension_t > & pixmap) noexcept
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
				&readProc,
				&writeProc,
				&seekProc,
				&closeProc,
				&sizeProc,
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

	template< typename pixel_data_t, typename dimension_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	bool
	FileFormatTIFF< pixel_data_t, dimension_t >::writeStream (IO::ByteStream & /*stream*/, const Pixmap< pixel_data_t, dimension_t > & /*pixmap*/, const WriteOptions & /*options*/) const noexcept
	{
		Logging::error("PixelFactory::FileFormatTIFF", "writeStream(), writing TIFF is not supported. Use PNG for lossless output, or HDR for floating point.");

		return false;
	}

	/* Explicit instantiations — the ONLY place libtiff symbols enter the binary.
	 * TIFFReadRGBAImageOriented() collapses every TIFF flavour to 8-bit RGBA, and
	 * PixelFactory::FileIO already guards its dispatch with
	 * `if constexpr ( std::is_same_v< pixel_data_t, uint8_t > )`, so no other pixel type can reach
	 * this codec. Add a line here if a consumer ever needs another dimension type; the linker names
	 * exactly what is missing when an instantiation is absent. */
	template class FileFormatTIFF< uint8_t, uint32_t >;
}