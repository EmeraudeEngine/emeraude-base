/*
 * src/IO/MemoryStreamBuffer.hpp
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
#include <ios>
#include <span>
#include <streambuf> // IWYU pragma: keep (the base class must be complete: only <streambuf> guarantees std::streambuf; IWYU 0.23 credits libstdc++'s <ios>, which happens to include it)

namespace EmEn::Base::IO
{
	/**
	 * @brief A read-only std::streambuf over a block of memory it does not own: a std::istream built on it reads the
	 * bytes in place.
	 * @note It replaces a std::istringstream built over a buffer already read: that constructor COPIES its string, so a
	 * format reader held the file twice (a 312 MB OBJ: 2 × 298 MiB of text, measured 2026-11-26).
	 * @note The memory must outlive this buffer and every stream built on it. Reading and seeking only (seekg() and
	 * tellg() inside the block); there is no put area, so a write fails as on any input-only buffer.
	 */
	class MemoryStreamBuffer final : public std::streambuf
	{
		public:

			/**
			 * @brief Constructs a buffer over a block of memory.
			 * @param memory The bytes to read. Not copied: they must outlive the buffer.
			 */
			explicit
			MemoryStreamBuffer (std::span< char > memory) noexcept
				: m_memory{memory}
			{
				this->setg(m_memory.data(), m_memory.data(), m_memory.subspan(m_memory.size()).data());
			}

			/**
			 * @brief Copy constructor.
			 * @param copy A reference to the copied instance.
			 */
			MemoryStreamBuffer (const MemoryStreamBuffer & copy) noexcept = delete;

			/**
			 * @brief Move constructor.
			 * @param copy A reference to the copied instance.
			 */
			MemoryStreamBuffer (MemoryStreamBuffer && copy) noexcept = delete;

			/**
			 * @brief Copy assignment.
			 * @param copy A reference to the copied instance.
			 * @return MemoryStreamBuffer &
			 */
			MemoryStreamBuffer & operator= (const MemoryStreamBuffer & copy) noexcept = delete;

			/**
			 * @brief Move assignment.
			 * @param copy A reference to the copied instance.
			 * @return MemoryStreamBuffer &
			 */
			MemoryStreamBuffer & operator= (MemoryStreamBuffer && copy) noexcept = delete;

			/**
			 * @brief Destructs the buffer. The memory is not touched.
			 */
			~MemoryStreamBuffer () override = default;

		protected:

			/**
			 * @brief Moves the read position, as std::istream::seekg() and tellg() ask.
			 * @note A position outside [0, size] is refused and leaves the position where it was.
			 * @param offset The offset from the origin.
			 * @param direction The origin: the start, the current position or the end of the block.
			 * @param which The sequence to move. Only the input one exists.
			 * @return pos_type The new position, or pos_type(off_type(-1)) on a refusal.
			 */
			pos_type
			seekoff (off_type offset, std::ios_base::seekdir direction, std::ios_base::openmode which) noexcept override
			{
				if ( (which & std::ios_base::in) == 0 )
				{
					return {off_type{-1}};
				}

				const auto size = static_cast< off_type >(m_memory.size());
				off_type origin = 0;

				if ( direction == std::ios_base::cur )
				{
					origin = this->gptr() - this->eback();
				}
				else if ( direction == std::ios_base::end )
				{
					origin = size;
				}
				else if ( direction != std::ios_base::beg )
				{
					return {off_type{-1}};
				}

				/* NOTE: Compared before adding, so a hostile offset cannot overflow the sum. */
				if ( offset < -origin || offset > size - origin )
				{
					return {off_type{-1}};
				}

				const off_type target = origin + offset;

				this->setg(m_memory.data(), m_memory.subspan(static_cast< size_t >(target)).data(), m_memory.subspan(m_memory.size()).data());

				return {target};
			}

			/**
			 * @brief Moves the read position to an absolute position.
			 * @param position The position from the start of the block.
			 * @param which The sequence to move. Only the input one exists.
			 * @return pos_type The new position, or pos_type(off_type(-1)) on a refusal.
			 */
			pos_type
			seekpos (pos_type position, std::ios_base::openmode which) noexcept override
			{
				return this->seekoff(off_type(position), std::ios_base::beg, which);
			}

		private:

			std::span< char > m_memory;
	};
}
