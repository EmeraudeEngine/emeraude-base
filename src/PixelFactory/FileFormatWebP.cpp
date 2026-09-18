/*
 * src/PixelFactory/FileFormatWebP.cpp
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

#include "FileFormatWebP.hpp"

/* STL inclusions. */
#include <cstddef>
#include <iostream>
#include <span>
#include <vector>

/* Third-party inclusions. */
#include <webp/decode.h>
#include <webp/encode.h>

namespace EmEn::Base::PixelFactory
{
	/**
	 * @brief Reads the whole remaining stream into a contiguous buffer.
	 * @note libwebp has no incremental entry point in the simple API: WebPGetInfo() and
	 * WebPDecode*() both want the complete bitstream at once, so there is nothing to stream.
	 * @param stream A reference to the byte stream.
	 * @param buffer A writable reference to the receiving buffer.
	 * @return bool
	 */
	static
	bool
	slurp (IO::ByteStream & stream, std::vector< uint8_t > & buffer) noexcept
	{
		const auto size = stream.size();

		if ( size == 0 )
		{
			std::cerr << "PixelFactory::FileFormatWebP, the stream is empty !" "\n";

			return false;
		}

		buffer.resize(size);

		return stream.read(buffer.data(), size);
	}

	template< typename pixel_data_t, typename dimension_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	bool
	FileFormatWebP< pixel_data_t, dimension_t >::readStream (IO::ByteStream & stream, Pixmap< pixel_data_t, dimension_t > & pixmap) noexcept
	{
		std::vector< uint8_t > encoded;

		if ( !slurp(stream, encoded) )
		{
			return false;
		}

		int width = 0;
		int height = 0;

		if ( WebPGetInfo(encoded.data(), encoded.size(), &width, &height) == 0 )
		{
			std::cerr << "PixelFactory::FileFormatWebP::readStream(), not a valid WebP bitstream !" "\n";

			return false;
		}

		/* ⚠️ The alpha flag comes from the FEATURES, never from the file extension or a guess:
		 * a lossy WebP with an alpha chunk and one without are the same extension, and decoding an
		 * opaque image as RGBA would waste a quarter of the texture while decoding a
		 * transparent one as RGB would drop its mask silently. */
		WebPBitstreamFeatures features{};

		if ( WebPGetFeatures(encoded.data(), encoded.size(), &features) != VP8_STATUS_OK )
		{
			std::cerr << "PixelFactory::FileFormatWebP::readStream(), unable to read the bitstream features !" "\n";

			return false;
		}

		const auto hasAlpha = features.has_alpha != 0;

		uint8_t * decoded = hasAlpha ?
			WebPDecodeRGBA(encoded.data(), encoded.size(), &width, &height) :
			WebPDecodeRGB(encoded.data(), encoded.size(), &width, &height);

		if ( decoded == nullptr )
		{
			std::cerr << "PixelFactory::FileFormatWebP::readStream(), the WebP decoder rejected the bitstream !" "\n";

			return false;
		}

		const auto channelMode = hasAlpha ? ChannelMode::RGBA : ChannelMode::RGB;
		const auto colorCount = hasAlpha ? 4U : 3U;
		const auto count = static_cast< size_t >(width) * static_cast< size_t >(height) * colorCount;

		const auto success = pixmap.initialize(
			static_cast< dimension_t >(width),
			static_cast< dimension_t >(height),
			channelMode,
			std::span< const pixel_data_t >{decoded, count}
		);

		/* ⚠️ Freed with WebPFree(), never with delete/free(): libwebp may be built against a
		 * different allocator than the consumer, and on Windows a DLL boundary makes the mismatch
		 * a crash rather than a leak. */
		WebPFree(decoded);

		if ( !success )
		{
			std::cerr << "PixelFactory::FileFormatWebP::readStream(), unable to allocate the pixmap !" "\n";

			return false;
		}

		return true;
	}

	template< typename pixel_data_t, typename dimension_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	bool
	FileFormatWebP< pixel_data_t, dimension_t >::writeStream (IO::ByteStream & stream, const Pixmap< pixel_data_t, dimension_t > & pixmap, const WriteOptions & /*options*/) const noexcept
	{
		if ( !pixmap.isValid() )
		{
			std::cerr << "PixelFactory::FileFormatWebP::writeStream(), the pixmap is invalid !" "\n";

			return false;
		}

		/* WebP carries no grayscale mode. Refused rather than expanded to RGB behind the caller's
		 * back: a silent 3x size increase is not a service. */
		const auto channelMode = pixmap.channelMode();

		if ( channelMode != ChannelMode::RGB && channelMode != ChannelMode::RGBA )
		{
			std::cerr << "PixelFactory::FileFormatWebP::writeStream(), WebP handles RGB and RGBA only, this pixmap is grayscale !" "\n";

			return false;
		}

		const auto width = static_cast< int >(pixmap.width());
		const auto height = static_cast< int >(pixmap.height());
		const auto colorCount = static_cast< int >(pixmap.colorCount());
		const auto * const pixels = reinterpret_cast< const uint8_t * >(pixmap.data().data());

		uint8_t * encoded = nullptr;

		/* Lossless on purpose — see the header. */
		const auto encodedSize = channelMode == ChannelMode::RGBA ?
			WebPEncodeLosslessRGBA(pixels, width, height, width * colorCount, &encoded) :
			WebPEncodeLosslessRGB(pixels, width, height, width * colorCount, &encoded);

		if ( encodedSize == 0 || encoded == nullptr )
		{
			std::cerr << "PixelFactory::FileFormatWebP::writeStream(), the WebP encoder failed !" "\n";

			return false;
		}

		const auto success = stream.write(encoded, encodedSize);

		WebPFree(encoded);

		return success;
	}

	/* ⚠️ WebP is an 8-BIT format: libwebp has no 16-bit or float entry point at all. Only the
	 * uint8_t instantiation exists, and every caller guards its dispatch with an `if constexpr`
	 * exactly as it does for PNG and JPEG. An unlisted instantiation fails at LINK time. */
	template class FileFormatWebP< uint8_t, uint32_t >;
}
