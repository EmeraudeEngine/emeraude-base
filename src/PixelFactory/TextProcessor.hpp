/*
 * src/PixelFactory/TextProcessor.hpp
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
#include <cstdint>
#include <string>

/* Local inclusions. */
#include "Font.hpp"
#include "Pixmap.hpp"
#include "Types.hpp"

namespace EmEn::Base::PixelFactory
{
	/**
	 * @brief The text processor is used to write on the targeted pixmap.
	 * @tparam pixel_data_t The pixel component type for the pixmap depth precision. Default uint8_t.
	 * @tparam dimension_t The type of unsigned integer used for pixmap dimension. Default uint32_t.
	 */
	template< typename pixel_data_t = uint8_t, typename dimension_t = uint32_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	class TextProcessor final
	{
		public:

			/**
			 * @brief Constructs a processor on a pixmap.
			 */
			TextProcessor () noexcept = default;

			/**
			 * @brief Constructs a processor on a pixmap.
			 * @param target A writable reference to a pixmap.
			 */
			explicit
			TextProcessor (Pixmap< pixel_data_t > & target) noexcept
				: m_pixmap(&target),
				m_rectangle(target.rectangle())
			{

			}

			/**
			 * @brief Sets the target pixmap to write on.
			 * @param pixmap A reference to a pixmap.
			 * @return bool
			 */
			bool
			setPixmap (Pixmap< pixel_data_t > & pixmap) noexcept
			{
				if ( !pixmap.isValid() )
				{
					return false;
				}

				m_pixmap = &pixmap;
				m_rectangle = pixmap.rectangle();

				/* NOTE: Recalculate text metrics for the new pixmap dimensions. */
				this->updateMetrics();

				return true;
			}

			/**
			 * @brief Sets a rectangle where to write the text onto the pixmap.
			 * @param rectangle A reference to a rectangle.
			 */
			void
			setRectangle (const Math::Space2D::AARectangle< int32_t > & rectangle) noexcept
			{
				m_rectangle = rectangle;

				this->updateMetrics();
			}

			/**
			 * @brief Returns the rectangle where the text is written onto the pixmap.
			 * @return const Math::Space2D::AARectangle< int32_t > &
			 */
			[[nodiscard]]
			const Math::Space2D::AARectangle< int32_t > &
			rectangle () const noexcept
			{
				return m_rectangle;
			}

			/**
			 * @brief Sets a font to write on the pixmap.
			 * @param font A pointer to a font.
			 * @param fontSize The size in the font.
			 */
			void
			setFont (const Font< pixel_data_t > & font, uint32_t fontSize) noexcept
			{
				m_selectedFont = font.glyphs(fontSize);

				this->updateMetrics();
			}

			/**
			 * @brief Sets the font color.
			 * @param color A reference to a color.
			 */
			void
			setFontColor (const Color< float > & color) noexcept
			{
				m_fontColor = color;
			}

			/**
			 * @brief Set the mode to print characters on the pixmap.
			 * @param mode The draw pixel mode.
			 */
			void
			setDrawMode (DrawPixelMode mode) noexcept
			{
				m_mode = mode;
			}

			/**
			 * @brief Returns the mode to print characters on the pixmap.
			 * @return DrawPixelMode
			 */
			[[nodiscard]]
			DrawPixelMode
			setDrawMode () const noexcept
			{
				return m_mode;
			}

			/**
			 * @brief Set space between lines.
			 * @param lineSpace The space.
			 */
			void
			setLineSpace (dimension_t lineSpace) noexcept
			{
				m_textMetrics.lineSpace = lineSpace;

				this->updateMetrics();
			}

			/**
			 * @brief Returns the current lince space.
			 * @return dimension_t
			 */
			[[nodiscard]]
			dimension_t
			lineSpace () const noexcept
			{
				return m_textMetrics.lineSpace;
			}

			/**
			 * @brief Writes a text on the pixmap with the font and color setup.
			 * @param text A reference to a string.
			 * @return bool
			 */
			bool
			write (const std::string & text) noexcept
			{
				if ( m_pixmap == nullptr || !m_rectangle.isValid() )
				{
					std::cerr << "TextProcessor::write(), the area is not valid !" "\n";

					return false;
				}

				if ( m_selectedFont == nullptr )
				{
					std::cerr << "TextProcessor::write(), there is no font selected !" "\n";

					return false;
				}

				/* NOTE: The pen advances in PIXELS: a proportional font by each glyph's own width plus the font's letter
				 * spacing, a fixed-width one by its cell (2026-10-18: every character used to take the widest glyph's
				 * cell, a proportional font came out letter-spaced like a monospaced one). */
				const auto areaWidth = static_cast< dimension_t >(m_rectangle.width());

				/* Text buffer position. */
				size_t currentChar = 0;

				for ( dimension_t currentRow = 0; currentRow < m_textMetrics.maxRows; ++currentRow )
				{
					dimension_t penX = 0;
					/* The character drawn before on this row, for the pair adjustment (none at the row start). */
					char previous = '\0';

					for ( ; currentChar < text.size(); ++currentChar )
					{
						/* NOTE: currentChar < text.size(): the loop condition. */
						const auto ASCIICode = text[currentChar];

						/* NOTE: Check for a new line feed. */
						if ( ASCIICode == '\n' )
						{
							++currentChar;

							break;
						}

						/* NOTE: Check for spaces special cases. */
						if ( ASCIICode == ' ' )
						{
							/* NOTE: Do not print a space at a beginning of a line. */
							if ( penX == 0 )
							{
								continue;
							}

							/* NOTE: The space and the next word go to the next row when they overflow this one, unless the
							 * word is wider than a whole row (it is cut where the row ends instead). */
							const auto wordWidth = this->wordWidth(text, currentChar + 1);

							if ( wordWidth <= areaWidth && penX + this->advance(' ') + wordWidth > areaWidth )
							{
								break;
							}
						}

						/* NOTE: The pair adjustment with the character before (2026-10-27: "AV" closes up). */
						const auto kernedX = this->kernedPen(penX, previous, ASCIICode);

						/* NOTE: A character that overflows the row goes to the next one, unless it starts the row (wider
						 * than the area: drawn clipped). */
						if ( kernedX > 0 && kernedX + this->cellWidth(ASCIICode) > areaWidth )
						{
							break;
						}

						this->blitCharacter(ASCIICode, kernedX, currentRow);

						penX = kernedX + this->advance(ASCIICode);
						previous = ASCIICode;
					}

					/* NOTE: No more text to print. */
					if ( currentChar >= text.size() )
					{
						break;
					}
				}

				return true;
			}

		private:

			/**
			 * @brief Returns the width a character occupies on a row: the fixed-width cell, or the glyph's own width.
			 * @pre A font is selected.
			 * @param ASCIICode The ASCII code of the character.
			 * @return dimension_t
			 */
			[[nodiscard]]
			dimension_t
			cellWidth (char ASCIICode) const noexcept
			{
				if ( m_selectedFont->isFixedWidth() )
				{
					return static_cast< dimension_t >(m_selectedFont->widestChar());
				}

				/* NOTE: The glyph table is indexed by the BYTE value (a char above 127 is negative where char is signed). */
				return static_cast< dimension_t >(m_selectedFont->glyph(static_cast< uint8_t >(ASCIICode)).width());
			}

			/**
			 * @brief Returns how far the pen moves after a character: its cell width plus the font's letter spacing.
			 * @pre A font is selected.
			 * @param ASCIICode The ASCII code of the character.
			 * @return dimension_t
			 */
			[[nodiscard]]
			dimension_t
			advance (char ASCIICode) const noexcept
			{
				return this->cellWidth(ASCIICode) + static_cast< dimension_t >(m_selectedFont->letterSpacing());
			}

			/**
			 * @brief Returns the pen position of a character drawn after another on the same row: moved by the font's
			 * pair adjustment (kerning), never left of the row start. A fixed-width font keeps its grid.
			 * @pre A font is selected.
			 * @param penX The pen position after the previous character.
			 * @param previous The previous character on the row ('\0' at the row start: no adjustment).
			 * @param ASCIICode The character to draw.
			 * @return dimension_t
			 */
			[[nodiscard]]
			dimension_t
			kernedPen (dimension_t penX, char previous, char ASCIICode) const noexcept
			{
				if ( previous == '\0' || m_selectedFont->isFixedWidth() )
				{
					return penX;
				}

				const auto adjustment = m_selectedFont->kerning(static_cast< uint8_t >(previous), static_cast< uint8_t >(ASCIICode));

				if ( adjustment >= 0 )
				{
					return penX + static_cast< dimension_t >(adjustment);
				}

				const auto back = static_cast< dimension_t >(-adjustment);

				return back >= penX ? 0 : penX - back;
			}

			/**
			 * @brief Returns the width of the word starting at an offset, up to the next space, line feed or the end of
			 * the text (the letter spacing after its last character excluded).
			 * @pre A font is selected.
			 * @param text A reference to the text.
			 * @param offset The offset of the word's first character.
			 * @return dimension_t
			 */
			[[nodiscard]]
			dimension_t
			wordWidth (const std::string & text, size_t offset) const noexcept
			{
				dimension_t width = 0;
				char previous = '\0';

				for ( auto index = offset; index < text.size() && text[index] != ' ' && text[index] != '\n'; ++index )
				{
					width = this->kernedPen(width, previous, text[index]) + this->advance(text[index]);
					previous = text[index];
				}

				if ( width > 0 )
				{
					width -= static_cast< dimension_t >(m_selectedFont->letterSpacing());
				}

				return width;
			}

			/**
			 * @brief Blit a character on the target pixmap.
			 * @param ASCIICode The ASCII code of the character.
			 * @param penX The horizontal pen position inside the area, in pixels.
			 * @param row The row inside the area.
			 * @return bool
			 */
			bool
			blitCharacter (char ASCIICode, dimension_t penX, dimension_t row) noexcept
			{
				/* NOTE: The glyph table is indexed by the BYTE value (a char above 127 is negative where char is signed). */
				const auto & glyph = m_selectedFont->glyph(static_cast< uint8_t >(ASCIICode));

				/* Compute the glyph origin on the pixmap. It stays SIGNED: the text area may start left of or above the
				 * pixmap, and blendFreePixel() clips. The pen and row offsets are bounded by the area size (write(),
				 * updateMetrics()), so they fit an int32_t. */
				const auto originX = m_rectangle.left() + static_cast< int32_t >(penX);
				const auto originY = m_rectangle.top() + static_cast< int32_t >(row * m_textMetrics.lineHeight);

				/* NOTE: Use blendFreePixel for bounds-safe pixel operations during resize transitions. */
				for ( dimension_t coordX = 0; coordX < glyph.width(); ++coordX )
				{
					for ( dimension_t coordY = 0; coordY < glyph.height(); ++coordY )
					{
						if constexpr ( std::is_floating_point_v< pixel_data_t > )
						{
							m_pixmap->blendFreePixel(
								originX + static_cast< int32_t >(coordX),
								originY + static_cast< int32_t >(coordY),
								m_fontColor,
								m_mode,
								glyph.pixelElement(coordX, coordY, Channel::Red)
							);
						}
						else
						{
							const auto value = static_cast< float >(glyph.pixelElement(coordX, coordY, Channel::Red)) / static_cast< float >(std::numeric_limits< pixel_data_t >::max());

							m_pixmap->blendFreePixel(
								originX + static_cast< int32_t >(coordX),
								originY + static_cast< int32_t >(coordY),
								m_fontColor,
								m_mode,
								value
							);
						}
					}
				}

				return true;
			}

			/**
			 * @brief Computes line height and line available on the pixmap.
			 */
			void
			updateMetrics () noexcept
			{
				if ( !m_rectangle.isValid() || m_selectedFont == nullptr )
				{
					return;
				}

				m_textMetrics.lineHeight = m_selectedFont->height() + m_textMetrics.lineSpace;

				/* NOTE: AARectangle::height() is never negative. A zero line height fits no row (it used to divide by
				 * zero). Integer division already rounds down. */
				const auto areaHeight = static_cast< dimension_t >(m_rectangle.height());

				m_textMetrics.maxRows = m_textMetrics.lineHeight > 0 ? areaHeight / m_textMetrics.lineHeight : 0;
			}

			Pixmap< pixel_data_t > * m_pixmap{nullptr};
			Math::Space2D::AARectangle< int32_t > m_rectangle;
			const ASCIIGlyphArray< pixel_data_t > * m_selectedFont{nullptr};
			Color< float > m_fontColor = White;
			DrawPixelMode m_mode{DrawPixelMode::Normal};
			TextMetrics< dimension_t > m_textMetrics;
	};
}
