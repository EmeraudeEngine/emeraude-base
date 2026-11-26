/*
 * src/IO/StreamLineReader.hpp
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
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

/* Local inclusions for usages. */
#include "IO/ByteStream.hpp"

namespace EmEn::Base::IO
{
	/**
	 * @brief Walks a ByteStream line by line, as std::getline() does, through a window it refills: a file is read
	 * without being held whole.
	 * @note The cut is IO::LineReader's, std::getline()'s: '\n' ends a line and is not part of it, a '\r' before it IS
	 * kept, a last line without '\n' is returned, and nothing is returned after a final '\n'.
	 * @note ⚠️ A line is a view into the window: it is valid until the next call to next() or rewind() only (the window
	 * is refilled and grows). IO::LineReader, over a text held whole, keeps its lines valid as long as the text.
	 * @note A line longer than the window grows it (doubling): the window holds the longest line, at most the file.
	 * @note It needs a stream that tells its position and seeks to it (FileStream, MemoryStream): isUsable() says so,
	 * and a reader falls back to reading the stream whole (IO::LineReader) otherwise. A failed read of the stream ends
	 * the lines early: hasFailed() tells it from the end of the stream.
	 * @note The ByteStream must outlive the reader, and nothing else may move its position while the reader reads it.
	 */
	class StreamLineReader final
	{
		public:

			/** @brief The window of a reader built without a size: 1 MiB. */
			static constexpr size_t DefaultWindowSize{size_t{1024} * 1024};

			/**
			 * @brief Constructs a reader over a byte stream, from its current position.
			 * @param stream A reference to the stream to read.
			 * @param windowSize The bytes read from the stream at once, at most the stream's length. Default 1 MiB; 0 is read as 1.
			 */
			explicit
			StreamLineReader (ByteStream & stream, size_t windowSize = DefaultWindowSize) noexcept
				: m_stream{&stream}
			{
				const auto origin = stream.tell();
				const auto size = static_cast< int64_t >(stream.size());

				/* NOTE: Usable when the stream tells its position and answers a seek to it. */
				if ( origin >= 0 && origin <= size && stream.seek(origin, 0) == origin )
				{
					m_origin = origin;
					m_length = size - origin;
					/* NOTE: Never larger than the stream: a small file does not cost a whole window. */
					m_window.resize(std::max< size_t >(std::min(windowSize, static_cast< size_t >(m_length)), 1));
					m_usable = true;
				}
			}

			/**
			 * @brief Copy constructor.
			 * @param copy A reference to the copied instance.
			 */
			StreamLineReader (const StreamLineReader & copy) noexcept = delete;

			/**
			 * @brief Move constructor.
			 * @param copy A reference to the copied instance.
			 */
			StreamLineReader (StreamLineReader && copy) noexcept = delete;

			/**
			 * @brief Copy assignment.
			 * @param copy A reference to the copied instance.
			 * @return StreamLineReader &
			 */
			StreamLineReader & operator= (const StreamLineReader & copy) noexcept = delete;

			/**
			 * @brief Move assignment.
			 * @param copy A reference to the copied instance.
			 * @return StreamLineReader &
			 */
			StreamLineReader & operator= (StreamLineReader && copy) noexcept = delete;

			/**
			 * @brief Destructs the reader. The stream is left where the last read or seek put it.
			 */
			~StreamLineReader () = default;

			/**
			 * @brief Returns whether the stream can be read through the window (it tells its position and seeks).
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isUsable () const noexcept
			{
				return m_usable;
			}

			/**
			 * @brief Returns whether a read of the stream failed: the lines stopped before its end.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			hasFailed () const noexcept
			{
				return m_failed;
			}

			/**
			 * @brief Gives the next line.
			 * @param line A reference to the view set on the next line, valid until the next call. Untouched when there
			 * is none.
			 * @return bool False once the stream is exhausted, or when a read of it failed (hasFailed()).
			 */
			[[nodiscard]]
			bool
			next (std::string_view & line) noexcept
			{
				if ( !m_usable )
				{
					return false;
				}

				while ( !m_failed )
				{
					const std::string_view buffered{m_window.data(), m_end};

					if ( const auto newline = buffered.find('\n', m_begin); newline != std::string_view::npos )
					{
						line = buffered.substr(m_begin, newline - m_begin);
						m_begin = newline + 1;

						return true;
					}

					if ( m_read >= m_length )
					{
						if ( m_begin < m_end )
						{
							line = buffered.substr(m_begin);
							m_begin = m_end;

							return true;
						}

						return false;
					}

					m_failed = !this->refill();
				}

				return false;
			}

			/**
			 * @brief Goes back to the start of the stream, for a second pass.
			 * @return bool False when the stream refused the seek (hasFailed() then answers true).
			 */
			[[nodiscard]]
			bool
			rewind () noexcept
			{
				if ( !m_usable || m_stream->seek(m_origin, 0) != m_origin )
				{
					m_failed = true;

					return false;
				}

				m_read = 0;
				m_begin = 0;
				m_end = 0;
				m_failed = false;

				return true;
			}

		private:

			/**
			 * @brief Moves the unread bytes to the front of the window, grows it when a line fills it, and reads the next
			 * bytes of the stream after them.
			 * @return bool False on a failed read.
			 */
			[[nodiscard]]
			bool
			refill () noexcept
			{
				{
					const std::span< char > window{m_window};

					std::ranges::copy(window.subspan(m_begin, m_end - m_begin), window.begin());
				}

				m_end -= m_begin;
				m_begin = 0;

				/* NOTE: A line as long as the window: it grows, the line has to fit. */
				if ( m_end == m_window.size() )
				{
					m_window.resize(m_window.size() * 2);
				}

				const auto count = static_cast< size_t >(std::min(static_cast< int64_t >(m_window.size() - m_end), m_length - m_read));

				if ( !m_stream->read(std::span< char >{m_window}.subspan(m_end).data(), count) )
				{
					return false;
				}

				m_end += count;
				m_read += static_cast< int64_t >(count);

				return true;
			}

			ByteStream * m_stream;
			std::vector< char > m_window;
			int64_t m_origin{0}; /* The stream's position at construction: where a rewind goes back to. */
			int64_t m_length{0}; /* The bytes from the origin to the end of the stream. */
			int64_t m_read{0}; /* The bytes read from the origin so far. */
			size_t m_begin{0}; /* The first unread byte of the window. */
			size_t m_end{0}; /* The end of the bytes held in the window. */
			bool m_usable{false};
			bool m_failed{false};
	};
}
