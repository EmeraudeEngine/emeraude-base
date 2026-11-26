/*
 * src/IO/ByteStreamBuffer.hpp
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
#include <ios>
#include <span>
#include <streambuf> // IWYU pragma: keep (the base class must be complete: only <streambuf> guarantees std::streambuf; IWYU 0.23 credits libstdc++'s <ios>, which happens to include it)
#include <string>
#include <vector>

/* Local inclusions for usages. */
#include "IO/ByteStream.hpp"

namespace EmEn::Base::IO
{
	/**
	 * @brief A read-only std::streambuf over a ByteStream, through a window it refills: a std::istream on it reads a
	 * file without holding it whole.
	 * @note The stream is seen from its position at construction to its end (ByteStream::size()). Reading forward
	 * refills the window; a seek inside the window moves within it, a seek outside asks the ByteStream to seek, and a
	 * position outside the stream is refused (failbit, the position unchanged).
	 * @note It needs a stream that tells its position and seeks to it (FileStream, MemoryStream): isUsable() says so.
	 * A reader falls back to reading the stream whole (IO::MemoryStreamBuffer) otherwise.
	 * @note Over a buffer read whole, it saves the file's size in memory: a 390 MB binary STL peaked at 2902 MiB read
	 * whole, measured 2026-11-26.
	 * @note The ByteStream must outlive this buffer, and nothing else may move its position while the buffer reads it.
	 */
	class ByteStreamBuffer final : public std::streambuf
	{
		public:

			/** @brief The window of a buffer built without a size: 1 MiB. */
			static constexpr size_t DefaultWindowSize{size_t{1024} * 1024};

			/**
			 * @brief Constructs a buffer over a byte stream, from its current position.
			 * @param stream A reference to the stream to read.
			 * @param windowSize The bytes read from the stream at once. Default 1 MiB; 0 is read as 1.
			 */
			explicit
			ByteStreamBuffer (ByteStream & stream, size_t windowSize = DefaultWindowSize) noexcept
				: m_stream{&stream}
			{
				const auto origin = stream.tell();
				const auto size = static_cast< int64_t >(stream.size());

				/* NOTE: Usable when the stream tells its position and answers a seek to it. */
				if ( origin >= 0 && origin <= size && stream.seek(origin, 0) == origin )
				{
					m_origin = origin;
					m_length = size - origin;
					m_window.resize(std::max< size_t >(windowSize, 1));
					m_usable = true;
				}
			}

			/**
			 * @brief Copy constructor.
			 * @param copy A reference to the copied instance.
			 */
			ByteStreamBuffer (const ByteStreamBuffer & copy) noexcept = delete;

			/**
			 * @brief Move constructor.
			 * @param copy A reference to the copied instance.
			 */
			ByteStreamBuffer (ByteStreamBuffer && copy) noexcept = delete;

			/**
			 * @brief Copy assignment.
			 * @param copy A reference to the copied instance.
			 * @return ByteStreamBuffer &
			 */
			ByteStreamBuffer & operator= (const ByteStreamBuffer & copy) noexcept = delete;

			/**
			 * @brief Move assignment.
			 * @param copy A reference to the copied instance.
			 * @return ByteStreamBuffer &
			 */
			ByteStreamBuffer & operator= (ByteStreamBuffer && copy) noexcept = delete;

			/**
			 * @brief Destructs the buffer. The stream is left where the last read or seek put it.
			 */
			~ByteStreamBuffer () override = default;

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

		protected:

			/**
			 * @brief Reads the next window of the stream when the current one is exhausted.
			 * @return int_type The next character, or eof at the end of the stream or on a failed read.
			 */
			int_type
			underflow () noexcept override
			{
				if ( this->gptr() < this->egptr() )
				{
					return traits_type::to_int_type(*this->gptr());
				}

				/* NOTE: The next window starts where the current one ends: the stream's position. */
				const auto windowEnd = m_windowStart + static_cast< int64_t >(this->egptr() - this->eback());

				if ( !m_usable || windowEnd >= m_length )
				{
					return traits_type::eof();
				}

				const auto count = static_cast< size_t >(std::min(static_cast< int64_t >(m_window.size()), m_length - windowEnd));

				if ( !m_stream->read(m_window.data(), count) )
				{
					return traits_type::eof();
				}

				m_windowStart = windowEnd;

				const std::span< char > window{m_window};

				this->setg(window.data(), window.data(), window.subspan(count).data());

				return traits_type::to_int_type(*this->gptr());
			}

			/**
			 * @brief Moves the read position, as std::istream::seekg() and tellg() ask.
			 * @note A position outside [0, size] is refused and leaves the position where it was.
			 * @param offset The offset from the origin.
			 * @param direction The origin: the start, the current position or the end of the stream.
			 * @param which The sequence to move. Only the input one exists.
			 * @return pos_type The new position, or pos_type(off_type(-1)) on a refusal.
			 */
			pos_type
			seekoff (off_type offset, std::ios_base::seekdir direction, std::ios_base::openmode which) noexcept override
			{
				if ( !m_usable || (which & std::ios_base::in) == 0 )
				{
					return {off_type{-1}};
				}

				const auto windowLength = static_cast< int64_t >(this->egptr() - this->eback());
				off_type origin = 0;

				if ( direction == std::ios_base::cur )
				{
					origin = m_windowStart + static_cast< int64_t >(this->gptr() - this->eback());
				}
				else if ( direction == std::ios_base::end )
				{
					origin = m_length;
				}
				else if ( direction != std::ios_base::beg )
				{
					return {off_type{-1}};
				}

				/* NOTE: Compared before adding, so a hostile offset cannot overflow the sum. */
				if ( offset < -origin || offset > m_length - origin )
				{
					return {off_type{-1}};
				}

				const off_type target = origin + offset;
				const std::span< char > window{m_window};

				/* Inside the window: the stream does not move. */
				if ( windowLength > 0 && target >= m_windowStart && target <= m_windowStart + windowLength )
				{
					this->setg(window.data(), window.subspan(static_cast< size_t >(target - m_windowStart)).data(), window.subspan(static_cast< size_t >(windowLength)).data());

					return {target};
				}

				/* Outside: the stream seeks, and the next read refills the window from there. */
				if ( m_stream->seek(m_origin + target, 0) != m_origin + target )
				{
					return {off_type{-1}};
				}

				m_windowStart = target;

				this->setg(window.data(), window.data(), window.data());

				return {target};
			}

			/**
			 * @brief Moves the read position to an absolute position.
			 * @param position The position from the start of the stream.
			 * @param which The sequence to move. Only the input one exists.
			 * @return pos_type The new position, or pos_type(off_type(-1)) on a refusal.
			 */
			pos_type
			seekpos (pos_type position, std::ios_base::openmode which) noexcept override
			{
				return this->seekoff(off_type(position), std::ios_base::beg, which);
			}

		private:

			ByteStream * m_stream;
			std::vector< char > m_window;
			int64_t m_origin{0}; /* The stream's position at construction: position 0 of this buffer. */
			int64_t m_length{0}; /* The bytes from the origin to the end of the stream. */
			int64_t m_windowStart{0}; /* The position of the window's first byte. */
			bool m_usable{false};
	};
}
