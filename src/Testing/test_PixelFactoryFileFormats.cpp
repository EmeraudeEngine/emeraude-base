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
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "Constants.hpp"
#include "IO/MemoryStream.hpp"
#include "PixelFactory/FileFormatHDR.hpp"
#include "PixelFactory/FileFormatJpeg.hpp"
#include "PixelFactory/FileFormatPNG.hpp"
#include "PixelFactory/FileFormatTarga.hpp"
#include "PixelFactory/Font.hpp"
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
/*
 * Radiance HDR (RGBE) codec tests — round-trip precision, the three scanline encodings
 * and hostile-stream hardening, following the fuzz-style tests above.
 */

TEST(PixelFactoryFileFormats, hdrRoundTripPreservesDynamicRange)
{
	/* Values spanning ~13 orders of magnitude, including a real sun disc luminance. */
	constexpr std::array< float, 8 > samples{0.0F, 0.0005F, 0.18F, 1.0F, 100.0F, 8000.0F, 65504.0F, 1.6e9F};

	PixelFactory::Pixmap< float, uint32_t > source;
	ASSERT_TRUE(source.initialize(16, 8, PixelFactory::ChannelMode::RGB));

	auto * data = source.data().data();

	for ( size_t index = 0; index < source.pixelCount(); ++index )
	{
		const auto value = samples[index % samples.size()];

		data[index * 3 + 0] = value;
		data[index * 3 + 1] = value * 0.5F;
		data[index * 3 + 2] = value * 0.25F;
	}

	std::vector< std::byte > buffer;

	{
		IO::MemoryStream output{buffer};
		const PixelFactory::FileFormatHDR< float, uint32_t > format;

		ASSERT_TRUE(format.writeStream(output, source));
	}

	PixelFactory::Pixmap< float, uint32_t > decoded;

	{
		IO::MemoryStream input{std::as_const(buffer)};
		PixelFactory::FileFormatHDR< float, uint32_t > format;

		ASSERT_TRUE(format.readStream(input, decoded));
	}

	ASSERT_EQ(decoded.width(), source.width());
	ASSERT_EQ(decoded.height(), source.height());
	ASSERT_EQ(decoded.channelMode(), PixelFactory::ChannelMode::RGB);

	const auto * decodedData = decoded.data().data();

	for ( size_t index = 0; index < source.pixelCount(); ++index )
	{
		/* RGBE precision: the 8-bit mantissa is shared per pixel, so the absolute error
		 * scales with the pixel's MAX component (1/256 of it, plus the +0.5 decode bias). */
		const auto maxComponent = data[index * 3 + 0];
		const auto tolerance = std::max(maxComponent / 100.0F, 1e-6F);

		for ( size_t channel = 0; channel < 3; ++channel )
		{
			ASSERT_NEAR(decodedData[index * 3 + channel], data[index * 3 + channel], tolerance);
		}
	}
}

namespace
{
	/* Builds a minimal Radiance header for the given dimensions. */
	[[nodiscard]]
	std::vector< std::byte >
	makeHDRHeader (uint32_t width, uint32_t height)
	{
		const std::string text =
			"#?RADIANCE\n"
			"FORMAT=32-bit_rle_rgbe\n"
			"\n"
			"-Y " + std::to_string(height) + " +X " + std::to_string(width) + "\n";

		std::vector< std::byte > buffer(text.size());
		std::transform(text.cbegin(), text.cend(), buffer.begin(), [] (char character) { return static_cast< std::byte >(character); });

		return buffer;
	}
}

TEST(PixelFactoryFileFormats, hdrAdaptiveRLEScanlineDecoded)
{
	/* One 8-pixel scanline in adaptive RLE: header (2, 2, 0, 8) then four component planes,
	 * each a single run of 8. The pixel (128, 64, 32, 130) decodes with f = 2^(130-136). */
	auto buffer = makeHDRHeader(8, 1);

	const auto b = [] (int value) noexcept { return static_cast< std::byte >(static_cast< uint8_t >(value)); };

	for ( const auto byte : {2, 2, 0, 8, /* planes: */ 136, 128, 136, 64, 136, 32, 136, 130} )
	{
		buffer.push_back(b(byte));
	}

	IO::MemoryStream stream{std::as_const(buffer)};
	PixelFactory::FileFormatHDR< float, uint32_t > format;
	PixelFactory::Pixmap< float, uint32_t > pixmap;

	ASSERT_TRUE(format.readStream(stream, pixmap));
	ASSERT_EQ(pixmap.width(), 8U);
	ASSERT_EQ(pixmap.height(), 1U);

	const auto factor = std::ldexp(1.0F, 130 - 136);
	const auto * data = pixmap.data().data();

	for ( size_t x = 0; x < 8; ++x )
	{
		ASSERT_NEAR(data[x * 3 + 0], (128.0F + 0.5F) * factor, 1e-6F);
		ASSERT_NEAR(data[x * 3 + 1], (64.0F + 0.5F) * factor, 1e-6F);
		ASSERT_NEAR(data[x * 3 + 2], (32.0F + 0.5F) * factor, 1e-6F);
	}
}

TEST(PixelFactoryFileFormats, hdrLegacyRLEScanlineDecoded)
{
	/* Width 4 (< 8, so the flat/legacy path): one literal pixel then (1,1,1,3) repeating it. */
	auto buffer = makeHDRHeader(4, 1);

	const auto b = [] (int value) noexcept { return static_cast< std::byte >(static_cast< uint8_t >(value)); };

	for ( const auto byte : {128, 64, 32, 130, /* legacy repeat: */ 1, 1, 1, 3} )
	{
		buffer.push_back(b(byte));
	}

	IO::MemoryStream stream{std::as_const(buffer)};
	PixelFactory::FileFormatHDR< float, uint32_t > format;
	PixelFactory::Pixmap< float, uint32_t > pixmap;

	ASSERT_TRUE(format.readStream(stream, pixmap));

	const auto factor = std::ldexp(1.0F, 130 - 136);
	const auto * data = pixmap.data().data();

	for ( size_t x = 0; x < 4; ++x )
	{
		ASSERT_NEAR(data[x * 3 + 0], (128.0F + 0.5F) * factor, 1e-6F);
	}
}

TEST(PixelFactoryFileFormats, hdrLegacyRLEShiftPastTheCountWidthIsRejected)
{
	/* Found by fuzz_hdr (2026-10-07, 26 million executions): each consecutive legacy repeat record shifts its count 8
	 * bits further, and the fifth shifted a 32-bit count by 32 — undefined behaviour. x86 masks the shift to 0, so this
	 * stream read as a valid 2-pixel scanline. A real file never needs a count past 2^32: refused. */
	auto buffer = makeHDRHeader(2, 1);

	const auto b = [] (int value) noexcept { return static_cast< std::byte >(static_cast< uint8_t >(value)); };

	for ( const auto byte : {128, 64, 32, 130, /* repeats of 0 (shifts 0, 8, 16, 24): */ 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, /* shift 32: */ 1, 1, 1, 1} )
	{
		buffer.push_back(b(byte));
	}

	IO::MemoryStream stream{std::as_const(buffer)};
	PixelFactory::FileFormatHDR< float, uint32_t > format;
	PixelFactory::Pixmap< float, uint32_t > pixmap;

	ASSERT_FALSE(format.readStream(stream, pixmap));
}

TEST(PixelFactoryFileFormats, hdrGarbageRejected)
{
	const std::vector< std::byte > buffer(16, std::byte{0xAB});

	IO::MemoryStream stream{buffer};
	PixelFactory::FileFormatHDR< float, uint32_t > format;
	PixelFactory::Pixmap< float, uint32_t > pixmap;

	ASSERT_FALSE(format.readStream(stream, pixmap));
}

TEST(PixelFactoryFileFormats, hdrTruncatedPixelDataRejected)
{
	/* A valid header claiming 64x64 with no pixel data at all. */
	const auto buffer = makeHDRHeader(64, 64);

	IO::MemoryStream stream{buffer};
	PixelFactory::FileFormatHDR< float, uint32_t > format;
	PixelFactory::Pixmap< float, uint32_t > pixmap;

	ASSERT_FALSE(format.readStream(stream, pixmap));
}

TEST(PixelFactoryFileFormats, hdrHugeDimensionsDoNotOOM)
{
	/* 4 billion pixels declared in a tiny stream must be cancelled, not allocated.
	 * The truncated-read guard fires before the pixmap grows out of control. */
	const auto buffer = makeHDRHeader(65535, 65535);

	IO::MemoryStream stream{buffer};
	PixelFactory::FileFormatHDR< float, uint32_t > format;
	PixelFactory::Pixmap< float, uint32_t > pixmap;

	ASSERT_FALSE(format.readStream(stream, pixmap));
}

/* Ave Robustus II P0 (2026-10-08): Font::readTrueTypeFile() returned on a failed FT_New_Face() without releasing the
 * FreeType library (and on a failed FT_Set_Pixel_Sizes() without releasing the face either). The library and the face
 * are RAII holders now. The refusal is checked here; the absence of the leak is checked by the ASan / LSan run of this
 * suite (the previous code reports a leaked FT_Library there). */
TEST(PixelFactoryFont, corruptTrueTypeFileRefused)
{
	std::error_code error;
	const auto directory = std::filesystem::temp_directory_path(error) / "emeraude_font_test";

	ASSERT_FALSE(error);

	std::filesystem::create_directories(directory, error);

	ASSERT_FALSE(error);

	const auto filepath = directory / "corrupt.ttf";

	{
		std::ofstream file{filepath, std::ios::binary | std::ios::trunc};

		ASSERT_TRUE(file.is_open());

		file << "This is not a TrueType font, only text with a .ttf extension.";
	}

	PixelFactory::Font< uint8_t > font;

	EXPECT_FALSE(font.readFile(filepath, 16, false));

	std::filesystem::remove_all(directory, error);
}

namespace
{
	/** @brief Returns the lowest row holding ink in a glyph cell, or -1 for an empty cell. */
	[[nodiscard]]
	int32_t
	lowestInkRow (const PixelFactory::Pixmap< uint8_t > & glyph)
	{
		for ( auto row = static_cast< int32_t >(glyph.height()) - 1; row >= 0; --row )
		{
			for ( uint32_t column = 0; column < glyph.width(); ++column )
			{
				if ( glyph.data()[(static_cast< size_t >(row) * glyph.width()) + column] > 0 )
				{
					return row;
				}
			}
		}

		return -1;
	}

	/** @brief Writes bytes to a file of the test's temporary directory; returns its path (empty on a failure). */
	[[nodiscard]]
	std::filesystem::path
	writeFontTestFile (const std::string & name, const std::vector< char > & bytes)
	{
		std::error_code error;
		const auto directory = std::filesystem::temp_directory_path(error) / "emeraude_font_test";

		if ( error || (!std::filesystem::create_directories(directory, error) && error) )
		{
			return {};
		}

		const auto filepath = directory / name;
		std::ofstream file{filepath, std::ios::binary | std::ios::trunc};

		if ( !file.is_open() )
		{
			return {};
		}

		file.write(bytes.data(), static_cast< std::streamsize >(bytes.size()));

		return file ? filepath : std::filesystem::path{};
	}
}

/* 2026-10-08 (owner decision: TrueType is kept and finished): the glyph callback loaded each glyph and returned an EMPTY
 * pixmap, so every .ttf was refused. The 256 cells are now rendered: ink where the font has it, none for the space and the
 * control codes, every cell as high as the font size. */
TEST(PixelFactoryFont, trueTypeFontRendersItsGlyphs)
{
	PixelFactory::Font< uint8_t > font;

	ASSERT_TRUE(font.readFile(TrueTypeFont, 32, false));

	const auto * glyphs = font.glyphs(32);

	ASSERT_NE(glyphs, nullptr);
	EXPECT_EQ(glyphs->height(), 32U);
	EXPECT_GT(glyphs->widestChar(), 0U);

	for ( size_t code = 0; code < PixelFactory::ASCIICount; ++code )
	{
		const auto & glyph = glyphs->glyph(static_cast< uint8_t >(code));

		ASSERT_TRUE(glyph.isValid()) << "code " << code;
		EXPECT_EQ(glyph.height(), 32U) << "code " << code;
	}

	for ( const char character : std::string{"AHgpx09#"} )
	{
		EXPECT_GE(lowestInkRow(glyphs->glyph(static_cast< uint8_t >(character))), 0) << "'" << character << "' has no ink";
	}

	EXPECT_EQ(lowestInkRow(glyphs->glyph(' ')), -1);
	EXPECT_EQ(lowestInkRow(glyphs->glyph('\n')), -1);
	EXPECT_EQ(lowestInkRow(glyphs->glyph(127)), -1);
	EXPECT_GT(glyphs->glyph(' ').width(), 0U);
}

/* The glyphs of a line sit on ONE baseline: letters without a descender end on the same row, the underscore below it. */
TEST(PixelFactoryFont, trueTypeGlyphsShareABaseline)
{
	PixelFactory::Font< uint8_t > font;

	ASSERT_TRUE(font.readFile(TrueTypeFont, 32, false));

	const auto * glyphs = font.glyphs(32);

	ASSERT_NE(glyphs, nullptr);

	const auto baselineRow = lowestInkRow(glyphs->glyph('A'));

	ASSERT_GT(baselineRow, 0);

	for ( const char character : std::string{"HEx0"} )
	{
		EXPECT_EQ(lowestInkRow(glyphs->glyph(static_cast< uint8_t >(character))), baselineRow) << "'" << character << "'";
	}

	EXPECT_GT(lowestInkRow(glyphs->glyph('_')), baselineRow);
}

/* fixedWidth: every cell as wide as the widest. */
TEST(PixelFactoryFont, trueTypeFixedWidthCellsAreEqual)
{
	PixelFactory::Font< uint8_t > font;

	ASSERT_TRUE(font.readFile(TrueTypeFont, 24, true));

	const auto * glyphs = font.glyphs(24);

	ASSERT_NE(glyphs, nullptr);
	EXPECT_TRUE(glyphs->isFixedWidth());

	for ( size_t code = 0; code < PixelFactory::ASCIICount; ++code )
	{
		EXPECT_EQ(glyphs->glyph(static_cast< uint8_t >(code)).width(), glyphs->widestChar()) << "code " << code;
	}
}

/* Hostile input at the trust boundary: a size out of [1, 1024], an empty file, a truncated font, a missing file. */
TEST(PixelFactoryFont, trueTypeHostileInputsRefused)
{
	PixelFactory::Font< uint8_t > font;

	EXPECT_FALSE(font.readFile(TrueTypeFont, 0, false));
	EXPECT_FALSE(font.readFile(TrueTypeFont, 1025, false));

	std::ifstream source{TrueTypeFont, std::ios::binary};

	ASSERT_TRUE(source.is_open());

	const std::vector< char > fontBytes{std::istreambuf_iterator< char >{source}, std::istreambuf_iterator< char >{}};

	ASSERT_GT(fontBytes.size(), 1024U);

	const auto emptyFile = writeFontTestFile("empty.ttf", {});
	const auto truncatedFile = writeFontTestFile("truncated.ttf", {fontBytes.begin(), fontBytes.begin() + static_cast< std::ptrdiff_t >(fontBytes.size() / 3)});

	ASSERT_FALSE(emptyFile.empty());
	ASSERT_FALSE(truncatedFile.empty());

	EXPECT_FALSE(font.readFile(emptyFile, 16, false));
	EXPECT_FALSE(font.readFile(truncatedFile, 16, false));
	EXPECT_FALSE(font.readFile(emptyFile.parent_path() / "missing.ttf", 16, false));

	std::error_code error;
	std::filesystem::remove_all(emptyFile.parent_path(), error);
}

namespace
{
	/* A Radiance header with the given resolution line, followed by one flat RGBE pixel (a scanline under 8 pixels is
	 * stored flat): a valid 1x1 picture when the line reads "-Y 1 +X 1". */
	[[nodiscard]]
	std::vector< std::byte >
	makeHDRWithResolutionLine (const std::string & resolutionLine)
	{
		const std::string text =
			"#?RADIANCE\n"
			"FORMAT=32-bit_rle_rgbe\n"
			"\n" +
			resolutionLine + "\n"
			"\x80\x80\x80\x81";

		std::vector< std::byte > buffer(text.size());
		std::transform(text.cbegin(), text.cend(), buffer.begin(), [] (char character) { return static_cast< std::byte >(character); });

		return buffer;
	}
}

/* The resolution line is parsed by std::from_chars() (std::sscanf() is deprecated by the MSVC CRT): the former grammar
 * (optional blanks, trailing text ignored), digits only, and a value past uint64_t refused. */
TEST(PixelFactoryFileFormats, hdrResolutionLineGrammar)
{
	for ( const auto * const accepted : {"-Y 1 +X 1", "-Y1 +X1", "-Y   1  +X  1  ", "-Y 1 +X 1 trailing", "-Y 01 +X 1\r"} )
	{
		const auto buffer = makeHDRWithResolutionLine(accepted);

		IO::MemoryStream stream{buffer};
		PixelFactory::FileFormatHDR< float, uint32_t > format;
		PixelFactory::Pixmap< float, uint32_t > pixmap;

		ASSERT_TRUE(format.readStream(stream, pixmap)) << "line '" << accepted << "'";
		EXPECT_EQ(pixmap.width(), 1U);
		EXPECT_EQ(pixmap.height(), 1U);
	}

	for ( const auto * const refused : {"-Y -1 +X 1", "-Y +1 +X 1", "+X 1 -Y 1", "-Y 1", "-Y 0 +X 1", "-Y 1 +X 0", "-Y x +X 1",
		"-Y 99999999999999999999 +X 1", "-Y 4294967296 +X 1", ""} )
	{
		const auto buffer = makeHDRWithResolutionLine(refused);

		IO::MemoryStream stream{buffer};
		PixelFactory::FileFormatHDR< float, uint32_t > format;
		PixelFactory::Pixmap< float, uint32_t > pixmap;

		EXPECT_FALSE(format.readStream(stream, pixmap)) << "line '" << refused << "'";
	}
}
