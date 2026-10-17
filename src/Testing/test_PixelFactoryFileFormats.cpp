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
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "IO/IO.hpp"
#include "TemporaryPath.hpp"
#include "Constants.hpp"
#include "IO/MemoryStream.hpp"
#include "PixelFactory/FileFormatHDR.hpp"
#include "PixelFactory/FileFormatJpeg.hpp"
#include "PixelFactory/FileFormatPNG.hpp"
#include "PixelFactory/FileFormatTarga.hpp"
#include "PixelFactory/FileFormatTIFF.hpp"
#include "PixelFactory/FileFormatWebP.hpp"
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

	/* Builds a minimal little-endian baseline TIFF: uncompressed 8-bit RGB, one strip. 'pixels' is appended as the strip;
	 * 'declaredStripBytes' is what the header claims (a lie makes a truncated file). */
	[[nodiscard]]
	std::vector< std::byte >
	makeTIFF (uint32_t width, uint32_t height, const std::vector< uint8_t > & pixels, uint32_t declaredStripBytes)
	{
		std::vector< std::byte > file;

		const auto u8 = [&file] (uint32_t value) { file.push_back(static_cast< std::byte >(value & 0xFFU)); };
		const auto u16 = [&u8] (uint32_t value) { u8(value); u8(value >> 8U); };
		const auto u32 = [&u16] (uint32_t value) { u16(value & 0xFFFFU); u16(value >> 16U); };

		constexpr uint32_t EntryCount{10};
		constexpr uint32_t IFDOffset{8};
		constexpr uint32_t BitsPerSampleOffset{IFDOffset + 2 + (EntryCount * 12) + 4};
		constexpr uint32_t StripOffset{BitsPerSampleOffset + 6};

		/* An IFD entry: tag, type (3 SHORT, 4 LONG), count, value or offset. */
		const auto entry = [&u16, &u32] (uint32_t tag, uint32_t type, uint32_t count, uint32_t value) {
			u16(tag);
			u16(type);
			u32(count);
			u32(value);
		};

		u8('I'); u8('I'); u16(42); u32(IFDOffset);
		u16(EntryCount);
		entry(256, 4, 1, width);                /* ImageWidth */
		entry(257, 4, 1, height);               /* ImageLength */
		entry(258, 3, 3, BitsPerSampleOffset);  /* BitsPerSample: 8, 8, 8 */
		entry(259, 3, 1, 1);                    /* Compression: none */
		entry(262, 3, 1, 2);                    /* PhotometricInterpretation: RGB */
		entry(273, 4, 1, StripOffset);          /* StripOffsets */
		entry(277, 3, 1, 3);                    /* SamplesPerPixel */
		entry(278, 4, 1, height);               /* RowsPerStrip */
		entry(279, 4, 1, declaredStripBytes);   /* StripByteCounts */
		entry(284, 3, 1, 1);                    /* PlanarConfiguration: chunky */
		u32(0);                                 /* No next IFD. */
		u16(8); u16(8); u16(8);

		for ( const auto value : pixels )
		{
			u8(value);
		}

		return file;
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

	/* ⚠️ std::as_const: MemoryStream's NON-const vector constructor is the WRITE mode — the read then failed at once and
	 * this test passed without ever reaching the pixel-depth guard (until 2026-10-17). */
	IO::MemoryStream stream{std::as_const(buffer)};
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
	const auto directory = EmEn::Base::Testing::uniqueTemporaryPath("emeraude_font_test");

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
		const auto directory = EmEn::Base::Testing::uniqueTemporaryPath("emeraude_font_test");

		if ( error || (!std::filesystem::create_directories(directory, error) && error) )
		{
			return {};
		}

		const auto filepath = directory / IO::u8path(name);
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

/* Hostile input at the trust boundary: a size out of [1, 256], an empty file, a truncated font, a missing file. */
TEST(PixelFactoryFont, trueTypeHostileInputsRefused)
{
	PixelFactory::Font< uint8_t > font;

	EXPECT_FALSE(font.readFile(TrueTypeFont, 0, false));
	EXPECT_TRUE(font.readFile(TrueTypeFont, 256, false));
	EXPECT_FALSE(font.readFile(TrueTypeFont, 257, false));

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

/* The TIFF reader (libtiff through a ByteStream): 0 % covered until 2026-10-17. */
TEST(PixelFactoryFileFormats, tiffSmallRGBDecodesTopLeftInRGBA)
{
	/* 2x2: red, green / blue, white — row 0 first in the file. */
	const std::vector< uint8_t > pixels{255, 0, 0,  0, 255, 0,  0, 0, 255,  255, 255, 255};
	const auto file = makeTIFF(2, 2, pixels, static_cast< uint32_t >(pixels.size()));

	IO::MemoryStream stream{file};
	PixelFactory::FileFormatTIFF< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	ASSERT_TRUE(format.readStream(stream, pixmap));
	ASSERT_EQ(pixmap.width(), 2U);
	ASSERT_EQ(pixmap.height(), 2U);

	const std::vector< uint8_t > expected{255, 0, 0, 255,  0, 255, 0, 255,  0, 0, 255, 255,  255, 255, 255, 255};
	const auto & data = pixmap.data();

	ASSERT_EQ(data.size(), expected.size());
	EXPECT_TRUE(std::equal(expected.cbegin(), expected.cend(), data.cbegin()));
}

TEST(PixelFactoryFileFormats, tiffGarbageRejected)
{
	std::vector< std::byte > garbage(256);

	for ( size_t index = 0; index < garbage.size(); ++index )
	{
		garbage[index] = static_cast< std::byte >((index * 37U + 11U) & 0xFFU);
	}

	IO::MemoryStream stream{std::as_const(garbage)};
	PixelFactory::FileFormatTIFF< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	ASSERT_FALSE(format.readStream(stream, pixmap));
}

TEST(PixelFactoryFileFormats, tiffTruncatedPixelDataRejected)
{
	/* 2x2 RGB declares 12 bytes of strip, the file holds 3. */
	const auto file = makeTIFF(2, 2, {255, 0, 0}, 12);

	IO::MemoryStream stream{file};
	PixelFactory::FileFormatTIFF< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	ASSERT_FALSE(format.readStream(stream, pixmap));
}

TEST(PixelFactoryFileFormats, tiffHugeDimensionsDoNotOOM)
{
	/* 100000x100000 declared in a few bytes: a 40 GB RGBA raster if unguarded. */
	const auto file = makeTIFF(100000, 100000, {0, 0, 0}, 3);

	IO::MemoryStream stream{file};
	PixelFactory::FileFormatTIFF< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	ASSERT_FALSE(format.readStream(stream, pixmap));
}

TEST(PixelFactoryFileFormats, tiffWriteRefused)
{
	PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

	ASSERT_TRUE(pixmap.initialize(2, 2, PixelFactory::ChannelMode::RGBA));

	std::vector< std::byte > output;
	IO::MemoryStream stream{output};
	const PixelFactory::FileFormatTIFF< uint8_t, uint32_t > format;

	ASSERT_FALSE(format.writeStream(stream, pixmap, {}));
}

/* The WebP reader / writer (libwebp, lossless writing): 0 % covered until 2026-10-17. */
namespace
{
	/* Encodes a pixmap into a WebP byte stream with the engine's own writer. */
	[[nodiscard]]
	std::vector< std::byte >
	encodeWebP (const PixelFactory::Pixmap< uint8_t, uint32_t > & pixmap)
	{
		std::vector< std::byte > encoded;
		IO::MemoryStream output{encoded};
		const PixelFactory::FileFormatWebP< uint8_t, uint32_t > format;

		if ( !format.writeStream(output, pixmap, {}) )
		{
			encoded.clear();
		}

		return encoded;
	}
}

TEST(PixelFactoryFileFormats, webpLosslessRoundTripRGB)
{
	const std::vector< uint8_t > pixels{255, 0, 0,  0, 255, 0,  0, 0, 255,  10, 20, 30,  200, 100, 50,  1, 2, 3};
	PixelFactory::Pixmap< uint8_t, uint32_t > source;

	ASSERT_TRUE(source.initialize(3, 2, PixelFactory::ChannelMode::RGB, std::span< const uint8_t >{pixels}));

	const auto encoded = encodeWebP(source);

	ASSERT_FALSE(encoded.empty());

	IO::MemoryStream input{encoded};
	PixelFactory::FileFormatWebP< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > decoded;

	ASSERT_TRUE(format.readStream(input, decoded));
	ASSERT_EQ(decoded.width(), 3U);
	ASSERT_EQ(decoded.height(), 2U);
	ASSERT_EQ(decoded.channelMode(), PixelFactory::ChannelMode::RGB);
	EXPECT_TRUE(std::equal(pixels.cbegin(), pixels.cend(), decoded.data().cbegin(), decoded.data().cend()));
}

TEST(PixelFactoryFileFormats, webpLosslessRoundTripKeepsAlpha)
{
	const std::vector< uint8_t > pixels{255, 0, 0, 0,  0, 255, 0, 128,  0, 0, 255, 255,  9, 9, 9, 7};
	PixelFactory::Pixmap< uint8_t, uint32_t > source;

	ASSERT_TRUE(source.initialize(2, 2, PixelFactory::ChannelMode::RGBA, std::span< const uint8_t >{pixels}));

	const auto encoded = encodeWebP(source);

	ASSERT_FALSE(encoded.empty());

	IO::MemoryStream input{encoded};
	PixelFactory::FileFormatWebP< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > decoded;

	ASSERT_TRUE(format.readStream(input, decoded));
	ASSERT_EQ(decoded.channelMode(), PixelFactory::ChannelMode::RGBA);
	/* Lossless keeps every channel — but a fully transparent pixel's colour is not part of the contract. */
	const auto & data = decoded.data();

	ASSERT_EQ(data.size(), pixels.size());
	EXPECT_EQ(data[3], 0);
	EXPECT_TRUE(std::equal(pixels.cbegin() + 4, pixels.cend(), data.cbegin() + 4));
}

TEST(PixelFactoryFileFormats, webpGarbageEmptyAndTruncatedRejected)
{
	PixelFactory::FileFormatWebP< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > decoded;

	{
		std::vector< std::byte > garbage(128);

		for ( size_t index = 0; index < garbage.size(); ++index )
		{
			garbage[index] = static_cast< std::byte >((index * 53U + 7U) & 0xFFU);
		}

		IO::MemoryStream input{std::as_const(garbage)};

		EXPECT_FALSE(format.readStream(input, decoded));
	}

	{
		const std::vector< std::byte > empty;
		IO::MemoryStream input{empty};

		EXPECT_FALSE(format.readStream(input, decoded));
	}

	{
		std::vector< uint8_t > pixels(16U * 16U * 3U);

		for ( size_t index = 0; index < pixels.size(); ++index )
		{
			pixels[index] = static_cast< uint8_t >((index * 131U) & 0xFFU);
		}

		PixelFactory::Pixmap< uint8_t, uint32_t > source;

		ASSERT_TRUE(source.initialize(16, 16, PixelFactory::ChannelMode::RGB, std::span< const uint8_t >{pixels}));

		auto encoded = encodeWebP(source);

		ASSERT_GT(encoded.size(), 32U);

		encoded.resize(encoded.size() / 2);

		IO::MemoryStream input{std::as_const(encoded)};

		EXPECT_FALSE(format.readStream(input, decoded));
	}
}

TEST(PixelFactoryFileFormats, webpWriteRefusesGrayscaleAndInvalidPixmaps)
{
	const PixelFactory::FileFormatWebP< uint8_t, uint32_t > format;

	{
		PixelFactory::Pixmap< uint8_t, uint32_t > gray;

		ASSERT_TRUE(gray.initialize(2, 2, PixelFactory::ChannelMode::Grayscale));

		std::vector< std::byte > output;
		IO::MemoryStream stream{output};

		EXPECT_FALSE(format.writeStream(stream, gray, {}));
	}

	{
		const PixelFactory::Pixmap< uint8_t, uint32_t > invalid;
		std::vector< std::byte > output;
		IO::MemoryStream stream{output};

		EXPECT_FALSE(format.writeStream(stream, invalid, {}));
	}
}

/* The JPEG reader / writer (libjpeg-turbo): 31 % covered until 2026-10-17. */
namespace
{
	/* A smooth RGB or grayscale gradient: what JPEG compresses well, so the round trip stays close. */
	[[nodiscard]]
	PixelFactory::Pixmap< uint8_t, uint32_t >
	makeGradient (uint32_t size, PixelFactory::ChannelMode channelMode)
	{
		PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

		if ( !pixmap.initialize(size, size, channelMode) )
		{
			return pixmap;
		}

		const auto colorCount = static_cast< size_t >(channelMode);
		auto & data = pixmap.data();

		for ( uint32_t y = 0; y < size; ++y )
		{
			for ( uint32_t x = 0; x < size; ++x )
			{
				const auto offset = ((static_cast< size_t >(y) * size) + x) * colorCount;

				for ( size_t channel = 0; channel < colorCount; ++channel )
				{
					/* At most 2 x 31 + 2 x 31 + 3 x 30 = 214 for a 32 pixel side: no wrap, so no hard 255 -> 0 edge
					 * (where JPEG rings) in the gradient. */
					data[offset + channel] = static_cast< uint8_t >((x * 2U) + (y * 2U) + (channel * 30U));
				}
			}
		}

		return pixmap;
	}

	/* Encodes a pixmap with the engine's JPEG writer; empty on refusal. */
	[[nodiscard]]
	std::vector< std::byte >
	encodeJpeg (const PixelFactory::Pixmap< uint8_t, uint32_t > & pixmap, const PixelFactory::WriteOptions & options = {})
	{
		std::vector< std::byte > encoded;
		IO::MemoryStream output{encoded};
		const PixelFactory::FileFormatJpeg< uint8_t, uint32_t > format;

		if ( !format.writeStream(output, pixmap, options) )
		{
			encoded.clear();
		}

		return encoded;
	}

	/* The largest absolute difference between two buffers of the same size. */
	[[nodiscard]]
	int
	maximumDifference (const std::vector< uint8_t > & first, const std::vector< uint8_t > & second)
	{
		int maximum = 0;

		for ( size_t index = 0; index < std::min(first.size(), second.size()); ++index )
		{
			maximum = std::max(maximum, std::abs(static_cast< int >(first[index]) - static_cast< int >(second[index])));
		}

		return maximum;
	}
}

TEST(PixelFactoryFileFormats, jpegRoundTripRGBStaysClose)
{
	const auto source = makeGradient(32, PixelFactory::ChannelMode::RGB);

	PixelFactory::WriteOptions options{};
	options.jpeg.quality = 95;

	const auto encoded = encodeJpeg(source, options);

	ASSERT_FALSE(encoded.empty());

	IO::MemoryStream input{encoded};
	PixelFactory::FileFormatJpeg< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > decoded;

	ASSERT_TRUE(format.readStream(input, decoded));
	ASSERT_EQ(decoded.width(), 32U);
	ASSERT_EQ(decoded.height(), 32U);
	ASSERT_EQ(decoded.channelMode(), PixelFactory::ChannelMode::RGB);
	EXPECT_LE(maximumDifference(source.data(), decoded.data()), 24);
}

TEST(PixelFactoryFileFormats, jpegRoundTripGrayscaleAndProgressive)
{
	const auto source = makeGradient(24, PixelFactory::ChannelMode::Grayscale);

	PixelFactory::WriteOptions options{};
	options.jpeg.quality = 90;
	options.jpeg.progressive = true;

	const auto encoded = encodeJpeg(source, options);

	ASSERT_FALSE(encoded.empty());

	IO::MemoryStream input{encoded};
	PixelFactory::FileFormatJpeg< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > decoded;

	ASSERT_TRUE(format.readStream(input, decoded));
	ASSERT_EQ(decoded.channelMode(), PixelFactory::ChannelMode::Grayscale);
	EXPECT_LE(maximumDifference(source.data(), decoded.data()), 24);
}

TEST(PixelFactoryFileFormats, jpegWriteRefusesAlphaAndInvalidPixmaps)
{
	EXPECT_TRUE(encodeJpeg(makeGradient(8, PixelFactory::ChannelMode::RGBA)).empty());
	EXPECT_TRUE(encodeJpeg(PixelFactory::Pixmap< uint8_t, uint32_t >{}).empty());
}

TEST(PixelFactoryFileFormats, jpegEmptyStreamRejected)
{
	const std::vector< std::byte > empty;
	IO::MemoryStream input{empty};
	PixelFactory::FileFormatJpeg< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > decoded;

	EXPECT_FALSE(format.readStream(input, decoded));
}

TEST(PixelFactoryFileFormats, jpegTruncatedDoesNotAbort)
{
	auto encoded = encodeJpeg(makeGradient(32, PixelFactory::ChannelMode::RGB));

	ASSERT_GT(encoded.size(), 64U);

	encoded.resize(encoded.size() / 2);

	IO::MemoryStream input{std::as_const(encoded)};
	PixelFactory::FileFormatJpeg< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > decoded;

	const auto loaded = format.readStream(input, decoded);

	std::cout << "[          ] truncated JPEG: readStream() returned " << (loaded ? "true" : "false") << "\n";
}

TEST(PixelFactoryFileFormats, jpegHugeDimensionsDoNotOOM)
{
	/* A valid 8x8 JPEG whose SOF header is rewritten to declare 65000x65000: a 12.7 GB RGB pixmap if unguarded. */
	auto encoded = encodeJpeg(makeGradient(8, PixelFactory::ChannelMode::RGB));

	ASSERT_FALSE(encoded.empty());

	bool patched = false;

	for ( size_t index = 0; index + 8 < encoded.size(); ++index )
	{
		const auto marker = static_cast< uint8_t >(encoded[index + 1]);

		if ( static_cast< uint8_t >(encoded[index]) == 0xFF && (marker == 0xC0 || marker == 0xC2) )
		{
			/* FF Cx, length (2), precision (1), height (2), width (2), big-endian. */
			encoded[index + 5] = std::byte{0xFD};
			encoded[index + 6] = std::byte{0xE8};
			encoded[index + 7] = std::byte{0xFD};
			encoded[index + 8] = std::byte{0xE8};
			patched = true;

			break;
		}
	}

	ASSERT_TRUE(patched);

	IO::MemoryStream input{std::as_const(encoded)};
	PixelFactory::FileFormatJpeg< uint8_t, uint32_t > format;
	PixelFactory::Pixmap< uint8_t, uint32_t > decoded;

	ASSERT_FALSE(format.readStream(input, decoded));
}

/* The Targa reader, every variant of TGA 2.0 (owner decision 2026-10-17: implement the colour-mapped, 15 / 16-bit and
 * 16-bit grayscale ones, which were decoded as garbage; the orientation follows the descriptor's bits 4 and 5, not
 * yOrigin). Files built byte by byte. */
namespace
{
	/* The header fields of a Targa file (the byte sections are arguments of makeTarga()). */
	struct TargaSpec
	{
		uint8_t type{2};
		uint16_t width{2};
		uint16_t height{2};
		uint8_t depth{24};
		uint8_t descriptor{0};
		uint8_t colorMapType{0};
		uint16_t colorMapOrigin{0};
		uint16_t colorMapLength{0};
		uint8_t colorMapEntrySize{0};
		uint16_t yOrigin{0};
	};

	/* A Targa file: the 18-byte header, the identification field, the colour map, then the pixel data as given. */
	[[nodiscard]]
	std::vector< std::byte >
	makeTarga (const TargaSpec & spec, const std::vector< uint8_t > & pixels, const std::vector< uint8_t > & colorMap = {}, const std::vector< uint8_t > & identification = {})
	{
		std::vector< std::byte > file;

		const auto u8 = [&file] (uint32_t value) { file.push_back(static_cast< std::byte >(value & 0xFFU)); };
		const auto u16 = [&u8] (uint32_t value) { u8(value); u8(value >> 8U); };

		u8(static_cast< uint32_t >(identification.size()));
		u8(spec.colorMapType);
		u8(spec.type);
		u16(spec.colorMapOrigin);
		u16(spec.colorMapLength);
		u8(spec.colorMapEntrySize);
		u16(0);
		u16(spec.yOrigin);
		u16(spec.width);
		u16(spec.height);
		u8(spec.depth);
		u8(spec.descriptor);

		for ( const auto value : identification ) { u8(value); }
		for ( const auto value : colorMap ) { u8(value); }
		for ( const auto value : pixels ) { u8(value); }

		return file;
	}

	/* Decodes a Targa file; an empty pixmap on refusal. */
	[[nodiscard]]
	PixelFactory::Pixmap< uint8_t, uint32_t >
	readTarga (const std::vector< std::byte > & file)
	{
		IO::MemoryStream input{file};
		PixelFactory::FileFormatTarga< uint8_t, uint32_t > format;
		PixelFactory::Pixmap< uint8_t, uint32_t > pixmap;

		if ( !format.readStream(input, pixmap) )
		{
			pixmap.clear();
		}

		return pixmap;
	}

	/* The pixmap's bytes, top-left first. */
	[[nodiscard]]
	std::vector< uint8_t >
	bytesOf (const PixelFactory::Pixmap< uint8_t, uint32_t > & pixmap)
	{
		return {pixmap.data().cbegin(), pixmap.data().cend()};
	}
}

TEST(PixelFactoryFileFormats, targaOrientationFollowsTheDescriptorBits)
{
	/* File order: first pixel (B,G,R) = red, then green / blue, white. */
	const std::vector< uint8_t > pixels{0, 0, 255,  0, 255, 0,  255, 0, 0,  255, 255, 255};

	/* Bottom-left (bit 5 clear): the FIRST file row is the BOTTOM row. */
	{
		const auto pixmap = readTarga(makeTarga({.descriptor = 0x00}, pixels));

		ASSERT_EQ(pixmap.channelMode(), PixelFactory::ChannelMode::RGB);
		EXPECT_EQ(bytesOf(pixmap), (std::vector< uint8_t >{0, 0, 255,  255, 255, 255,  255, 0, 0,  0, 255, 0}));
	}

	/* Top-left (bit 5 set) with yOrigin 0 — the case loaded upside down until 2026-10-17. */
	{
		const auto pixmap = readTarga(makeTarga({.descriptor = 0x20}, pixels));

		EXPECT_EQ(bytesOf(pixmap), (std::vector< uint8_t >{255, 0, 0,  0, 255, 0,  0, 0, 255,  255, 255, 255}));
	}

	/* Top-left with yOrigin = height (another writer's habit): the same image. */
	{
		const auto pixmap = readTarga(makeTarga({.descriptor = 0x20, .yOrigin = 2}, pixels));

		EXPECT_EQ(bytesOf(pixmap), (std::vector< uint8_t >{255, 0, 0,  0, 255, 0,  0, 0, 255,  255, 255, 255}));
	}

	/* Top-left, right-to-left (bit 4): the columns are mirrored. */
	{
		const auto pixmap = readTarga(makeTarga({.descriptor = 0x30}, pixels));

		EXPECT_EQ(bytesOf(pixmap), (std::vector< uint8_t >{0, 255, 0,  255, 0, 0,  255, 255, 255,  0, 0, 255}));
	}
}

TEST(PixelFactoryFileFormats, targaTrueColorDepths)
{
	/* 32 bits: BGRA -> RGBA. */
	{
		const auto pixmap = readTarga(makeTarga({.width = 1, .height = 1, .depth = 32, .descriptor = 0x28}, {10, 20, 30, 40}));

		ASSERT_EQ(pixmap.channelMode(), PixelFactory::ChannelMode::RGBA);
		EXPECT_EQ(bytesOf(pixmap), (std::vector< uint8_t >{30, 20, 10, 40}));
	}

	/* 16 bits with one alpha bit: A1R5G5B5, little-endian. 0x7C00 = red, alpha bit clear (transparent);
	 * 0x83E0 = green, alpha bit set (opaque). */
	{
		const auto pixmap = readTarga(makeTarga({.width = 2, .height = 1, .depth = 16, .descriptor = 0x21}, {0x00, 0x7C,  0xE0, 0x83}));

		ASSERT_EQ(pixmap.channelMode(), PixelFactory::ChannelMode::RGBA);
		EXPECT_EQ(bytesOf(pixmap), (std::vector< uint8_t >{255, 0, 0, 0,  0, 255, 0, 255}));
	}

	/* 15 bits: no alpha, RGB; 0x001F = blue, 0x0210 = (0, 16, 16) -> (0, 132, 132) by bit replication. */
	{
		const auto pixmap = readTarga(makeTarga({.width = 2, .height = 1, .depth = 15, .descriptor = 0x20}, {0x1F, 0x00,  0x10, 0x02}));

		ASSERT_EQ(pixmap.channelMode(), PixelFactory::ChannelMode::RGB);
		EXPECT_EQ(bytesOf(pixmap), (std::vector< uint8_t >{0, 0, 255,  0, 132, 132}));
	}
}

TEST(PixelFactoryFileFormats, targaColorMappedImages)
{
	/* A 24-bit palette of three entries (BGR), the first index being 1: indices 1, 2, 3. */
	const std::vector< uint8_t > palette{0, 0, 255,  0, 255, 0,  255, 0, 0};

	/* Raw (type 1). */
	{
		const auto pixmap = readTarga(makeTarga({.type = 1, .width = 3, .height = 1, .depth = 8, .descriptor = 0x20,
			.colorMapType = 1, .colorMapOrigin = 1, .colorMapLength = 3, .colorMapEntrySize = 24}, {3, 1, 2}, palette));

		ASSERT_EQ(pixmap.channelMode(), PixelFactory::ChannelMode::RGB);
		EXPECT_EQ(bytesOf(pixmap), (std::vector< uint8_t >{0, 0, 255,  255, 0, 0,  0, 255, 0}));
	}

	/* RLE (type 9): a run of 2 x index 2, then a raw packet of 1 x index 3. */
	{
		const auto pixmap = readTarga(makeTarga({.type = 9, .width = 3, .height = 1, .depth = 8, .descriptor = 0x20,
			.colorMapType = 1, .colorMapOrigin = 1, .colorMapLength = 3, .colorMapEntrySize = 24}, {0x81, 2,  0x00, 3}, palette));

		EXPECT_EQ(bytesOf(pixmap), (std::vector< uint8_t >{0, 255, 0,  0, 255, 0,  0, 0, 255}));
	}

	/* A 32-bit palette makes an RGBA image. */
	{
		const auto pixmap = readTarga(makeTarga({.type = 1, .width = 1, .height = 1, .depth = 8, .descriptor = 0x28,
			.colorMapType = 1, .colorMapLength = 1, .colorMapEntrySize = 32}, {0}, {1, 2, 3, 4}));

		ASSERT_EQ(pixmap.channelMode(), PixelFactory::ChannelMode::RGBA);
		EXPECT_EQ(bytesOf(pixmap), (std::vector< uint8_t >{3, 2, 1, 4}));
	}
}

TEST(PixelFactoryFileFormats, targaColorMappedRefusals)
{
	const std::vector< uint8_t > palette{0, 0, 255,  0, 255, 0};

	/* An index outside the map (the map holds indices 0 and 1). */
	EXPECT_FALSE(readTarga(makeTarga({.type = 1, .width = 2, .height = 1, .depth = 8,
		.colorMapType = 1, .colorMapLength = 2, .colorMapEntrySize = 24}, {1, 5}, palette)).isValid());

	/* An index below the first entry index. */
	EXPECT_FALSE(readTarga(makeTarga({.type = 1, .width = 2, .height = 1, .depth = 8,
		.colorMapType = 1, .colorMapOrigin = 4, .colorMapLength = 2, .colorMapEntrySize = 24}, {4, 0}, palette)).isValid());

	/* A colour-mapped image without a colour map. */
	EXPECT_FALSE(readTarga(makeTarga({.type = 1, .width = 2, .height = 1, .depth = 8}, {0, 1})).isValid());

	/* A colour map entry size the format does not define. */
	EXPECT_FALSE(readTarga(makeTarga({.type = 1, .width = 1, .height = 1, .depth = 8,
		.colorMapType = 1, .colorMapLength = 1, .colorMapEntrySize = 12}, {0}, {0, 0})).isValid());
}

TEST(PixelFactoryFileFormats, targaGrayscaleImages)
{
	/* 8 bits, raw (type 3). */
	{
		const auto pixmap = readTarga(makeTarga({.type = 3, .width = 2, .height = 1, .depth = 8, .descriptor = 0x20}, {7, 200}));

		ASSERT_EQ(pixmap.channelMode(), PixelFactory::ChannelMode::Grayscale);
		EXPECT_EQ(bytesOf(pixmap), (std::vector< uint8_t >{7, 200}));
	}

	/* 16 bits = gray + alpha, RLE (type 11): a run of 3 x (gray 50, alpha 128). */
	{
		const auto pixmap = readTarga(makeTarga({.type = 11, .width = 3, .height = 1, .depth = 16, .descriptor = 0x28}, {0x82, 50, 128}));

		ASSERT_EQ(pixmap.channelMode(), PixelFactory::ChannelMode::GrayscaleAlpha);
		EXPECT_EQ(bytesOf(pixmap), (std::vector< uint8_t >{50, 128,  50, 128,  50, 128}));
	}
}

TEST(PixelFactoryFileFormats, targaSkipsTheIdentificationAndAnAttachedColorMap)
{
	/* A true-colour image MAY carry a colour map (the specification): skipped, never read as pixels. And an ID field. */
	const auto pixmap = readTarga(makeTarga({.width = 1, .height = 1, .depth = 24, .descriptor = 0x20,
		.colorMapType = 1, .colorMapLength = 2, .colorMapEntrySize = 24}, {30, 20, 10}, {9, 9, 9,  9, 9, 9}, {'E', 'M', 'E', 'N'}));

	ASSERT_TRUE(pixmap.isValid());
	EXPECT_EQ(bytesOf(pixmap), (std::vector< uint8_t >{10, 20, 30}));
}

TEST(PixelFactoryFileFormats, targaRefusesUndefinedVariants)
{
	/* A true-colour depth of 8 bits, a grayscale one of 24, an unknown type, an unknown colour map type. */
	EXPECT_FALSE(readTarga(makeTarga({.type = 2, .width = 1, .height = 1, .depth = 8}, {0})).isValid());
	EXPECT_FALSE(readTarga(makeTarga({.type = 3, .width = 1, .height = 1, .depth = 24}, {0, 0, 0})).isValid());
	EXPECT_FALSE(readTarga(makeTarga({.type = 7, .width = 1, .height = 1, .depth = 24}, {0, 0, 0})).isValid());
	EXPECT_FALSE(readTarga(makeTarga({.width = 1, .height = 1, .depth = 24, .colorMapType = 2}, {0, 0, 0})).isValid());
}

TEST(PixelFactoryFileFormats, targaUncompressedDataLargerThanTheStreamRefused)
{
	/* 100 x 100 x 3 bytes declared, 300 bytes given: within the RLE bound (x128), refused before the allocation. */
	EXPECT_FALSE(readTarga(makeTarga({.width = 100, .height = 100, .depth = 24}, std::vector< uint8_t >(300, 0))).isValid());
}

TEST(PixelFactoryFileFormats, targaWriterRoundTrips)
{
	const std::vector< uint8_t > rgb{255, 0, 0,  0, 255, 0,  0, 0, 255,  9, 8, 7,  1, 1, 1,  1, 1, 1};
	const std::vector< uint8_t > rgba{255, 0, 0, 10,  0, 255, 0, 20,  0, 0, 255, 30,  9, 8, 7, 40};
	const std::vector< uint8_t > gray{1, 2, 3, 3, 3, 3};

	const auto roundTrip = [] (uint32_t width, uint32_t height, PixelFactory::ChannelMode mode, const std::vector< uint8_t > & pixels, bool rle) {
		PixelFactory::Pixmap< uint8_t, uint32_t > source;

		if ( !source.initialize(width, height, mode, std::span< const uint8_t >{pixels}) )
		{
			return false;
		}

		PixelFactory::WriteOptions options{};
		options.targa.rleCompression = rle;

		std::vector< std::byte > encoded;
		IO::MemoryStream output{encoded};
		const PixelFactory::FileFormatTarga< uint8_t, uint32_t > format;

		if ( !format.writeStream(output, source, options) )
		{
			return false;
		}

		const auto decoded = readTarga(std::as_const(encoded));

		return decoded.isValid() && decoded.channelMode() == mode && bytesOf(decoded) == pixels;
	};

	for ( const bool rle : {false, true} )
	{
		EXPECT_TRUE(roundTrip(3, 2, PixelFactory::ChannelMode::RGB, rgb, rle)) << "RGB, RLE " << rle;
		EXPECT_TRUE(roundTrip(2, 2, PixelFactory::ChannelMode::RGBA, rgba, rle)) << "RGBA, RLE " << rle;
		EXPECT_TRUE(roundTrip(3, 2, PixelFactory::ChannelMode::Grayscale, gray, rle)) << "Grayscale, RLE " << rle;
	}
}
