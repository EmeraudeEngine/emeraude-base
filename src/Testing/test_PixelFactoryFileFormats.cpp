/*
 * src/Testing/test_PixelFactoryFileFormats.cpp
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

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* STL inclusions. */
#include <cstdint>
#include <vector>

/* Local inclusions. */
#include "IO/MemoryStream.hpp"
#include "PixelFactory/FileFormatJpeg.hpp"
#include "PixelFactory/FileFormatPNG.hpp"
#include "PixelFactory/FileFormatTarga.hpp"
#include "PixelFactory/Pixmap.hpp"

using namespace EmEn::Base;

/*
 * Hardening tests for the PixelFactory image parsers (Ave robustus! A.3 — fuzz_png/jpeg/targa).
 * They feed hostile / truncated byte streams through readStream() and assert the load is cancelled
 * (returns false) without aborting, exiting or OOM-ing the process. Verified under ASan/UBSan/LSan.
 */

namespace
{
	/* Builds an 18-byte Targa header with the given image type, dimensions and pixel depth. */
	[[nodiscard]]
	std::vector< std::byte >
	makeTargaHeader (uint8_t imageTypeCode, uint16_t width, uint16_t height, uint8_t pixelDepth)
	{
		const auto b = [] (int value) noexcept { return static_cast< std::byte >(static_cast< uint8_t >(value)); };

		return {
			b(0),                                              // idCharCount
			b(0),                                              // colorMapType
			b(imageTypeCode),                                  // imageTypeCode
			b(0), b(0),                                        // colorMapOrigin
			b(0), b(0),                                        // colorMapLength
			b(0),                                              // colorMapEntrySize
			b(0), b(0),                                        // xOrigin
			b(0), b(0),                                        // yOrigin
			b(width & 0xFF), b((width >> 8) & 0xFF),           // width  (little-endian)
			b(height & 0xFF), b((height >> 8) & 0xFF),         // height (little-endian)
			b(pixelDepth),                                     // imagePixelSize
			b(0)                                               // imageDescriptorByte
		};
	}
}

TEST(PixelFactoryFileFormats, targaHugeDimensionsDoNotOOM)
{
	/* 65535x65535 RGB declared in an 18-byte file -> ~12 GB pixmap if unguarded (fuzz_targa OOM). */
	const auto buffer = makeTargaHeader(2, 0xFFFF, 0xFFFF, 24);

	IO::MemoryStream stream{buffer};
	PixelFactory::FileFormatTarga< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	ASSERT_FALSE(format.readStream(stream, pixmap));
}

TEST(PixelFactoryFileFormats, targaZeroDimensionsRejected)
{
	const auto buffer = makeTargaHeader(2, 0, 0, 24);

	IO::MemoryStream stream{buffer};
	PixelFactory::FileFormatTarga< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	ASSERT_FALSE(format.readStream(stream, pixmap));
}

TEST(PixelFactoryFileFormats, targaTruncatedHeaderRejected)
{
	const std::vector< std::byte > buffer(4, std::byte{0});  // shorter than the 18-byte header

	IO::MemoryStream stream{buffer};
	PixelFactory::FileFormatTarga< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	ASSERT_FALSE(format.readStream(stream, pixmap));
}

TEST(PixelFactoryFileFormats, pngValidSignatureGarbageChunkDoesNotAbort)
{
	/* A valid 8-byte PNG signature (so png_check_sig passes) followed by a bogus chunk: libPNG
	 * raises a fatal error inside png_read_info. With a returning error handler this aborted the
	 * process (fuzz_png); the load must now be cancelled (returns false) via setjmp/longjmp. */
	const auto b = [] (int value) noexcept { return static_cast< std::byte >(static_cast< uint8_t >(value)); };

	const std::vector< std::byte > buffer{
		b(0x89), b('P'), b('N'), b('G'), b('\r'), b('\n'), b(0x1A), b('\n'),  // PNG signature
		b(0xFF)                                                               // truncated/garbage chunk
	};

	IO::MemoryStream stream{buffer};
	PixelFactory::FileFormatPNG< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	ASSERT_FALSE(format.readStream(stream, pixmap));
}

TEST(PixelFactoryFileFormats, targaUnsupportedPixelDepthRejected)
{
	/* imagePixelSize = 64 -> bytesPerPixel = 8, which overflowed the 4-byte stack pixel buffer in
	 * the decode loop (fuzz_targa stack-buffer-overflow). Small dimensions + a little payload pass
	 * the dimension guard so we reach (and exercise) the pixel-depth guard. */
	auto buffer = makeTargaHeader(2, 1, 1, 64);
	buffer.resize(buffer.size() + 16, std::byte{0});  // payload so the dimension guard passes

	IO::MemoryStream stream{buffer};
	PixelFactory::FileFormatTarga< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	ASSERT_FALSE(format.readStream(stream, pixmap));
}

TEST(PixelFactoryFileFormats, jpegGarbageDoesNotExitProcess)
{
	/* libjpeg's default error_exit calls exit() on a bad marker (fuzz_jpeg killed the process on a
	 * single byte). The custom error manager longjmps instead, so the load is cancelled cleanly. */
	const std::vector< std::byte > buffer(8, std::byte{0xFF});  // no valid JPEG SOI / markers

	IO::MemoryStream stream{buffer};
	PixelFactory::FileFormatJpeg< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	ASSERT_FALSE(format.readStream(stream, pixmap));
}