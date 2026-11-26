/*
 * src/IO/LineReader.hpp
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
#include <cstddef>
#include <string_view>

namespace EmEn::Base::IO
{
	/**
	 * @brief Walks a text held in memory line by line, as std::getline() does, without copying it: every line is a
	 * std::string_view into the text.
	 * @note The cut is exactly std::getline()'s: '\n' ends a line and is not part of it, a '\r' before it IS kept, a
	 * last line without '\n' is returned, and nothing is returned after a final '\n'. A reader moved from
	 * std::getline() to this one therefore sees the same lines.
	 * @note Over a std::istringstream, it saves the stream's copy of the text and the std::string each line was copied
	 * into (a 312 MB OBJ: −298 MiB and −16 % of the read time, measured 2026-11-26).
	 * @note The text must outlive the reader and every line it returned.
	 */
	class LineReader final
	{
		public:

			/**
			 * @brief Constructs a reader at the start of a text.
			 * @param text The text to walk. Not copied: it must outlive the reader.
			 */
			explicit
			LineReader (std::string_view text) noexcept
				: m_text{text}
			{

			}

			/**
			 * @brief Gives the next line.
			 * @param line A reference to the view set on the next line. Untouched when there is none.
			 * @return bool False once the text is exhausted.
			 */
			[[nodiscard]]
			bool
			next (std::string_view & line) noexcept
			{
				if ( m_position >= m_text.size() )
				{
					return false;
				}

				const auto newline = m_text.find('\n', m_position);
				const auto end = newline == std::string_view::npos ? m_text.size() : newline;

				line = m_text.substr(m_position, end - m_position);
				m_position = newline == std::string_view::npos ? m_text.size() : newline + 1;

				return true;
			}

			/**
			 * @brief Goes back to the start of the text, for a second pass.
			 */
			void
			rewind () noexcept
			{
				m_position = 0;
			}

		private:

			std::string_view m_text;
			size_t m_position{0};
	};
}
