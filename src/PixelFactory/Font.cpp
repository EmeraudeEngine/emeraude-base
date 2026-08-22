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
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>

/* Third-party inclusions. */
#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H

namespace EmEn::Base::PixelFactory
{
	template< typename precision_t >
	requires (std::is_arithmetic_v< precision_t >)
	bool
	Font< precision_t >::readTrueTypeFile (const std::filesystem::path & filepath, uint32_t fontSize, bool fixedWidth)
	{
		FT_Library library{};
		FT_Face face{};

		/* Try to init FreeType 2. */
		if ( FT_Init_FreeType(&library) > 0 )
		{
			std::cerr << "[ERROR] Font::readTrueTypeFile(), FreeType 2 init failed !" "\n";

			return false;
		}

		/* Load the font face. Face index 0 (always available). */
		if ( FT_New_Face(library, filepath.string().data(), 0, &face) > 0 )
		{
			std::cerr << "[ERROR] Font::readTrueTypeFile(), Font file " << filepath << " cannot be open !" "\n";

			return false;
		}

		/* Prepare output sizes.
		 * NOTE: 0 means square. */
		if ( FT_Set_Pixel_Sizes(face, 0, static_cast< FT_UInt >(fontSize)) > 0 )
		{
			std::cerr << "[ERROR] Font::readTrueTypeFile(), the size request with this font is not available !" "\n";

			return false;
		}

		auto & glyphs = this->getGlyphArray(fontSize);

		const auto success = glyphs.writeGlyphData([&] (size_t index) {
			/* Gets the correct glyph index inside the font for the iso code. */
			const auto glyphIndex = FT_Get_Char_Index(face, static_cast< FT_ULong >(index));

			/* Gets the glyph loaded.
			 * NOTE: Only one font can be loaded at a time. */
			if ( FT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER) > 0 )
			{
				std::cerr << "[ERROR] Glyph " << glyphIndex << " failed to load !" "\n";

				return Pixmap< precision_t >{};
			}

			//const auto glyphWidth = face->glyph->bitmap.width;
			//const auto glyphHeight = face->glyph->bitmap.rows;
			//const auto size = glyphWidth * glyphHeight;

			/*if ( size > 0 )
			{
				auto newWidth = glyphWidth + 2;

				// Checks for overflow
				newWidth = std::min(newWidth, size);

				Pixmap glyph{};

				if ( glyph.initialize(glyphWidth, glyphHeight, ChannelMode::Grayscale) )
				{
					const auto bufferSize = static_cast< uint32_t >(face->glyph->bitmap.width * face->glyph->bitmap.rows) * sizeof(uint8_t);

					glyph.fill(face->glyph->bitmap.buffer, bufferSize);
				}
				else
				{
					return {};
				}

				const auto offsetY = (m_maxHeight - face->glyph->bitmap_top) - (m_maxHeight / 4);

				if ( offsetY > m_maxHeight )
				{
					return {};
				}

				//int32_t offsetY = 0;
				//if ( face->glyph->bitmap_top != face->glyph->bitmap.rows )
				//	offsetY = (m_maxHeight - face->glyph->bitmap_top) / 2;
				//else
				//	offsetY = m_maxHeight - face->glyph->bitmap.rows;

				auto & currentGlyph = m_glyphs.at(charNum);

				currentGlyph.initialize(newWidth, m_maxHeight, ChannelMode::Grayscale);

				const Processor proc{currentGlyph};
				proc.blit(glyph, {1UL, static_cast< uint32_t >(offsetY), glyph.width(), glyph.height()});

				// Sets the highest width of a char.
				m_maxWidth = std::max< uint32_t >(newWidth, m_maxWidth);
			}
			else
			{
				auto currentGlyph = m_glyphs.at(charNum);

				// Empty char.
				currentGlyph.initialize(m_maxWidth / 2, m_maxHeight, ChannelMode::Grayscale);
				currentGlyph.fill(Color(0.0F, 0.0F, 0.0F));
			}*/

			return Pixmap< precision_t >{};
		}, fixedWidth);

		FT_Done_Face(face);
		FT_Done_FreeType(library);

		return success;
	}

	/* Explicit instantiation — the ONLY place FreeType symbols enter the binary. Fonts are 8-bit
	 * greyscale glyph maps; TextProcessor only ever takes a Font by reference, so no other precision
	 * reaches this method. A missing instantiation fails at LINK time, named by the linker. */
	template bool Font< uint8_t >::readTrueTypeFile (const std::filesystem::path &, uint32_t, bool);
}
