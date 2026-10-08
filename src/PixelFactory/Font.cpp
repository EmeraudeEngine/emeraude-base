/*
 * src/PixelFactory/Font.cpp
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

#include "Font.hpp"

/* STL inclusions. */
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

/* Third-party inclusions. */
#include "ft2build.h"
#include FT_FREETYPE_H

/* Local inclusions. */
#include "IO/IO.hpp"

namespace EmEn::Base::PixelFactory
{
	namespace
	{
		/** @brief Releases a FreeType library handle (RAII, Ave Robustus II rule 1). */
		struct FreeTypeLibraryDeleter final
		{
			void
			operator() (FT_Library library) const noexcept
			{
				FT_Done_FreeType(library);
			}
		};

		/** @brief Releases a FreeType face handle (RAII). */
		struct FreeTypeFaceDeleter final
		{
			void
			operator() (FT_Face face) const noexcept
			{
				FT_Done_Face(face);
			}
		};

		using FreeTypeLibrary = std::unique_ptr< FT_LibraryRec_, FreeTypeLibraryDeleter >;
		using FreeTypeFace = std::unique_ptr< FT_FaceRec_, FreeTypeFaceDeleter >;

		/** @brief The largest TrueType font size read (pixels, the height of ONE glyph cell — owner decision 2026-10-08:
		 * ample for any title). 256 cells of up to 8 × size × size elements: ~10 MB for a usual font, 128 MiB at worst. */
		constexpr uint32_t MaxTrueTypeFontSize{256};

		/** @brief The widest glyph cell accepted, in font sizes (a hostile outline could ask for far more). */
		constexpr uint32_t MaxTrueTypeCellWidthFactor{8};

		/**
		 * @brief Reads a big-endian 32-bit integer.
		 * @pre offset + 4 <= data.size().
		 * @param data A reference to the bytes.
		 * @param offset The offset.
		 * @return uint32_t
		 */
		[[nodiscard]]
		uint32_t
		readBigEndian32 (const std::vector< FT_Byte > & data, size_t offset) noexcept
		{
			return (static_cast< uint32_t >(data[offset]) << 24U) | (static_cast< uint32_t >(data[offset + 1]) << 16U) | (static_cast< uint32_t >(data[offset + 2]) << 8U) | static_cast< uint32_t >(data[offset + 3]);
		}

		/**
		 * @brief Checks that every table an sfnt font (TrueType, OpenType) declares lies inside the file.
		 * @note FreeType is lenient with a truncated file: the face opens, and a glyph whose data is cut renders EMPTY, so
		 * a truncated font loads as an invisible one. The sfnt table directory (offset 12: 16-byte records of tag,
		 * checksum, offset, length) tells where each table ends; a collection ('ttcf') points at its first font's
		 * directory. Reference: OpenType specification, "Organization of an OpenType Font"
		 * (https://learn.microsoft.com/en-us/typography/opentype/spec/otff).
		 * @param data A reference to the whole file.
		 * @return bool
		 */
		[[nodiscard]]
		bool
		sfntTablesInsideFile (const std::vector< FT_Byte > & data) noexcept
		{
			constexpr size_t SfntHeaderSize{12};
			constexpr size_t TableRecordSize{16};

			size_t directory = 0;

			if ( data.size() >= 16 && data[0] == 't' && data[1] == 't' && data[2] == 'c' && data[3] == 'f' )
			{
				directory = readBigEndian32(data, 12);
			}

			if ( directory > data.size() || data.size() - directory < SfntHeaderSize )
			{
				return false;
			}

			const auto tableCount = (static_cast< size_t >(data[directory + 4]) << 8U) | static_cast< size_t >(data[directory + 5]);

			if ( (data.size() - directory - SfntHeaderSize) / TableRecordSize < tableCount )
			{
				return false;
			}

			for ( size_t table = 0; table < tableCount; ++table )
			{
				const auto record = directory + SfntHeaderSize + (table * TableRecordSize);
				const auto end = static_cast< uint64_t >(readBigEndian32(data, record + 8)) + static_cast< uint64_t >(readBigEndian32(data, record + 12));

				if ( end > data.size() )
				{
					return false;
				}
			}

			return true;
		}

		/**
		 * @brief Returns whether a code of the 256-entry table is a control code (C0, DEL, C1): it gets an empty cell, never
		 * the font's missing-glyph box.
		 * @param code The character code (Latin-1).
		 * @return bool
		 */
		[[nodiscard]]
		bool
		isControlCode (size_t code) noexcept
		{
			return code < 32 || (code >= 127 && code < 160);
		}

		/**
		 * @brief Returns the coverage (0 to 255) of one pixel of a rendered FreeType bitmap.
		 * @pre The bitmap is FT_PIXEL_MODE_GRAY or FT_PIXEL_MODE_MONO, x < width, y < rows (y = 0 is the TOP row).
		 * @note A negative pitch stores the rows bottom-up (FreeType "bitmap flow"); a GRAY bitmap may use fewer than 256
		 * levels (num_grays).
		 * @param bitmap A reference to the bitmap.
		 * @param x The column.
		 * @param y The row from the top.
		 * @return uint8_t
		 */
		[[nodiscard]]
		uint8_t
		bitmapCoverage (const FT_Bitmap & bitmap, uint32_t x, uint32_t y) noexcept
		{
			const auto pitch = static_cast< int64_t >(bitmap.pitch);
			const auto rowBytes = static_cast< size_t >(pitch < 0 ? -pitch : pitch);
			const auto memoryRow = static_cast< size_t >(pitch >= 0 ? y : bitmap.rows - 1U - y);
			const auto * row = bitmap.buffer + (memoryRow * rowBytes);

			if ( bitmap.pixel_mode == FT_PIXEL_MODE_MONO )
			{
				return (row[x >> 3U] & (0x80U >> (x & 7U))) != 0U ? 255U : 0U;
			}

			const auto value = static_cast< uint32_t >(row[x]);

			if ( bitmap.num_grays == 256U || bitmap.num_grays < 2U )
			{
				return static_cast< uint8_t >(value);
			}

			return static_cast< uint8_t >(std::min< uint32_t >((value * 255U) / (static_cast< uint32_t >(bitmap.num_grays) - 1U), 255U));
		}

		/**
		 * @brief Converts a coverage (0 to 255) to a pixmap element.
		 * @tparam precision_t The pixmap element type.
		 * @param coverage The coverage.
		 * @return precision_t
		 */
		template< typename precision_t >
		[[nodiscard]]
		precision_t
		coverageToElement (uint8_t coverage) noexcept
		{
			if constexpr ( std::is_floating_point_v< precision_t > )
			{
				return static_cast< precision_t >(coverage) / static_cast< precision_t >(255);
			}
			else
			{
				return static_cast< precision_t >((static_cast< uint64_t >(coverage) * static_cast< uint64_t >(std::numeric_limits< precision_t >::max())) / 255U);
			}
		}
	}

	template< typename precision_t >
	requires (std::is_arithmetic_v< precision_t >)
	bool
	Font< precision_t >::readTrueTypeFile (const std::filesystem::path & filepath, uint32_t fontSize, bool fixedWidth)
	{
		/* NOTE: A trust boundary (a file, a size from data): checked in every build. */
		if ( fontSize == 0 || fontSize > MaxTrueTypeFontSize )
		{
			std::cerr << "[ERROR] Font::readTrueTypeFile(), the font size " << fontSize << " is out of [1, " << MaxTrueTypeFontSize << "] !" "\n";

			return false;
		}

		/* NOTE: The file is read through IO (UTF-8 paths on every platform) and handed to FreeType from memory. Declaration
		 * order matters: the face reads this buffer and is destroyed BEFORE it, and BEFORE the library that created it. */
		std::vector< FT_Byte > fontData;

		if ( !IO::fileGetContents(filepath, fontData) || fontData.empty() || fontData.size() > static_cast< size_t >(std::numeric_limits< FT_Long >::max()) )
		{
			std::cerr << "[ERROR] Font::readTrueTypeFile(), font file " << filepath << " cannot be read !" "\n";

			return false;
		}

		FreeTypeLibrary library;
		FreeTypeFace face;

		/* Try to init FreeType 2. */
		{
			FT_Library rawLibrary = nullptr;

			if ( FT_Init_FreeType(&rawLibrary) != 0 )
			{
				std::cerr << "[ERROR] Font::readTrueTypeFile(), FreeType 2 init failed !" "\n";

				return false;
			}

			library.reset(rawLibrary);
		}

		/* Load the font face. Face index 0 (always available). */
		{
			FT_Face rawFace = nullptr;

			if ( FT_New_Memory_Face(library.get(), fontData.data(), static_cast< FT_Long >(fontData.size()), 0, &rawFace) != 0 )
			{
				std::cerr << "[ERROR] Font::readTrueTypeFile(), font file " << filepath << " is not a usable font !" "\n";

				return false;
			}

			face.reset(rawFace);
		}

		if ( FT_IS_SFNT(face.get()) && !sfntTablesInsideFile(fontData) )
		{
			std::cerr << "[ERROR] Font::readTrueTypeFile(), font file " << filepath << " is truncated (a table ends past the end of the file) !" "\n";

			return false;
		}

		/* NOTE: The font size is the LINE: ascender minus descender = fontSize pixels (FT_SIZE_REQUEST_TYPE_REAL_DIM), so
		 * every glyph fits the fontSize-high cell the glyph array and TextProcessor use — as a pixmap font's cell does.
		 * A nominal (em) size would put the ascenders and descenders of most fonts outside the cell.
		 * Reference: FreeType, "Glyph Conventions" (https://freetype.org/freetype2/docs/glyphs/glyphs-3.html). */
		{
			FT_Size_RequestRec request{};
			request.type = FT_SIZE_REQUEST_TYPE_REAL_DIM;
			request.width = static_cast< FT_Long >(fontSize) * 64;
			request.height = static_cast< FT_Long >(fontSize) * 64;

			if ( FT_Request_Size(face.get(), &request) != 0 )
			{
				std::cerr << "[ERROR] Font::readTrueTypeFile(), the size " << fontSize << " is not available with this font !" "\n";

				return false;
			}
		}

		/* The baseline, in rows from the top of the cell: the scaled ascender (26.6, rounded up). */
		const auto ascender = static_cast< int64_t >(face->size->metrics.ascender);
		const auto baseline = std::clamp< int64_t >((ascender + 63) / 64, 0, static_cast< int64_t >(fontSize));
		const auto maxCellWidth = fontSize * MaxTrueTypeCellWidthFactor;

		/* An empty cell (a control code, a glyph without ink) is as wide as the space; at least one column. */
		uint32_t spaceWidth = std::max< uint32_t >(fontSize / 4U, 1U);

		if ( const auto spaceIndex = FT_Get_Char_Index(face.get(), ' '); spaceIndex != 0 && FT_Load_Glyph(face.get(), spaceIndex, FT_LOAD_DEFAULT) == 0 )
		{
			const auto advance = static_cast< int64_t >(face->glyph->advance.x);

			spaceWidth = static_cast< uint32_t >(std::clamp< int64_t >((advance + 32) / 64, 1, static_cast< int64_t >(maxCellWidth)));
		}

		/* Render the 256 cells (Latin-1 codes through the face's Unicode charmap), each as wide as its glyph's advance. */
		/* NOTE: One allocation of 256 cells on a cold path (the indices are checked by the vector, not a std::array). */
		std::vector< Pixmap< precision_t > > cells(ASCIICount);
		uint32_t widestCell = 0;

		for ( size_t code = 0; code < ASCIICount; ++code )
		{
			if ( isControlCode(code) )
			{
				cells[code] = Pixmap< precision_t >{spaceWidth, fontSize, ChannelMode::Grayscale};
				widestCell = std::max(widestCell, spaceWidth);

				continue;
			}

			/* NOTE: A code the font lacks maps to glyph 0, the font's missing-glyph box: a visible hole, on purpose. */
			const auto glyphIndex = FT_Get_Char_Index(face.get(), static_cast< FT_ULong >(code));

			if ( FT_Load_Glyph(face.get(), glyphIndex, FT_LOAD_RENDER) != 0 )
			{
				std::cerr << "[ERROR] Font::readTrueTypeFile(), glyph " << glyphIndex << " (code " << code << ") failed to render !" "\n";

				return false;
			}

			const auto * slot = face->glyph;
			const auto & bitmap = slot->bitmap;

			if ( bitmap.width > 0 && bitmap.rows > 0 && bitmap.pixel_mode != FT_PIXEL_MODE_GRAY && bitmap.pixel_mode != FT_PIXEL_MODE_MONO )
			{
				std::cerr << "[ERROR] Font::readTrueTypeFile(), glyph " << glyphIndex << " has an unhandled pixel mode (" << static_cast< int >(bitmap.pixel_mode) << ") !" "\n";

				return false;
			}

			/* Horizontal placement: a negative left bearing (an italic overhang) shifts the glyph right, inside the cell. */
			const auto left = static_cast< int64_t >(slot->bitmap_left);
			const auto shift = left < 0 ? -left : 0;
			const auto drawX = left + shift;
			const auto advance = std::max< int64_t >((static_cast< int64_t >(slot->advance.x) + 32) / 64, 0);
			const auto cellWidth = std::max< int64_t >({advance + shift, drawX + static_cast< int64_t >(bitmap.width), 1});

			if ( std::cmp_greater(cellWidth, maxCellWidth) )
			{
				std::cerr << "[ERROR] Font::readTrueTypeFile(), glyph " << glyphIndex << " is " << cellWidth << " pixels wide (more than " << maxCellWidth << ") !" "\n";

				return false;
			}

			Pixmap< precision_t > cell{static_cast< uint32_t >(cellWidth), fontSize, ChannelMode::Grayscale};
			auto & elements = cell.data();

			/* Vertical placement: the bitmap's top row sits bitmap_top rows above the baseline. Rows outside the cell are
			 * clipped (a glyph taller than the font's ascender + descender). */
			const auto drawY = baseline - static_cast< int64_t >(slot->bitmap_top);

			for ( uint32_t row = 0; row < bitmap.rows; ++row )
			{
				const auto cellY = drawY + static_cast< int64_t >(row);

				if ( cellY < 0 || std::cmp_greater_equal(cellY, fontSize) )
				{
					continue;
				}

				for ( uint32_t column = 0; column < bitmap.width; ++column )
				{
					const auto cellX = drawX + static_cast< int64_t >(column);
					const auto coverage = bitmapCoverage(bitmap, column, row);

					elements[(static_cast< size_t >(cellY) * static_cast< size_t >(cellWidth)) + static_cast< size_t >(cellX)] = coverageToElement< precision_t >(coverage);
				}
			}

			widestCell = std::max(widestCell, static_cast< uint32_t >(cellWidth));
			cells[code] = std::move(cell);
		}

		auto & glyphs = this->getGlyphArray(fontSize);

		return glyphs.writeGlyphData([&cells, widestCell, fontSize, fixedWidth] (size_t index) {
			auto & cell = cells[index];

			if ( !fixedWidth || cell.width() == widestCell )
			{
				return std::move(cell);
			}

			/* A fixed-width font: every cell as wide as the widest, the glyph centred in it. */
			Pixmap< precision_t > padded{widestCell, fontSize, ChannelMode::Grayscale};
			const auto offsetX = (widestCell - cell.width()) / 2U;

			for ( uint32_t row = 0; row < fontSize; ++row )
			{
				for ( uint32_t column = 0; column < cell.width(); ++column )
				{
					padded.data()[(static_cast< size_t >(row) * widestCell) + offsetX + column] = cell.data()[(static_cast< size_t >(row) * cell.width()) + column];
				}
			}

			return padded;
		}, fixedWidth);
	}

	/* Explicit instantiation — the ONLY place FreeType symbols enter the binary. Fonts are 8-bit
	 * greyscale glyph maps; TextProcessor only ever takes a Font by reference, so no other precision
	 * reaches this method. A missing instantiation fails at LINK time, named by the linker. */
	template bool Font< uint8_t >::readTrueTypeFile (const std::filesystem::path &, uint32_t, bool);
}
