/*
 * src/Testing/StreamDoubles.hpp
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
#include <cstdint>
#include <vector>

/* Local inclusions for inheritances. */
#include "IO/ByteStream.hpp"

/* Local inclusions for usages. */
#include "IO/MemoryStream.hpp"

namespace EmEn::Base::Testing
{
	/**
	 * @brief A memory stream that tells no position, so it cannot seek (ByteStream's defaults): a reader that reads
	 * through a window (IO::ByteStreamBuffer, IO::StreamLineReader) must fall back to reading it whole.
	 */
	class UnseekableStream final : public IO::ByteStream
	{
		public:

			/**
			 * @brief Constructs a stream reading bytes.
			 * @param data A reference to the bytes. They must outlive the stream.
			 */
			explicit
			UnseekableStream (const std::vector< std::byte > & data) noexcept
				: m_inner{data}
			{

			}

			/** @copydoc IO::ByteStream::read() */
			bool
			read (void * data, size_t size) noexcept override
			{
				return m_inner.read(data, size);
			}

			/** @copydoc IO::ByteStream::write() */
			bool
			write (const void * /*data*/, size_t /*size*/) noexcept override
			{
				return false;
			}

			/** @copydoc IO::ByteStream::isOpen() */
			[[nodiscard]]
			bool
			isOpen () const noexcept override
			{
				return m_inner.isOpen();
			}

			/** @copydoc IO::ByteStream::size() */
			[[nodiscard]]
			size_t
			size () const noexcept override
			{
				return m_inner.size();
			}

		private:

			IO::MemoryStream m_inner;
	};

	/**
	 * @brief A memory stream whose reads fail once they would pass a given byte: a read error in the middle of a file.
	 */
	class FailingStream final : public IO::ByteStream
	{
		public:

			/**
			 * @brief Constructs a stream reading bytes.
			 * @param data A reference to the bytes. They must outlive the stream.
			 * @param failAfter The bytes from the start that read; a read reaching past them fails.
			 */
			FailingStream (const std::vector< std::byte > & data, int64_t failAfter) noexcept
				: m_inner{data},
				m_failAfter{failAfter}
			{

			}

			/** @copydoc IO::ByteStream::read() */
			bool
			read (void * data, size_t size) noexcept override
			{
				if ( m_inner.tell() + static_cast< int64_t >(size) > m_failAfter )
				{
					return false;
				}

				return m_inner.read(data, size);
			}

			/** @copydoc IO::ByteStream::write() */
			bool
			write (const void * /*data*/, size_t /*size*/) noexcept override
			{
				return false;
			}

			/** @copydoc IO::ByteStream::isOpen() */
			[[nodiscard]]
			bool
			isOpen () const noexcept override
			{
				return m_inner.isOpen();
			}

			/** @copydoc IO::ByteStream::size() */
			[[nodiscard]]
			size_t
			size () const noexcept override
			{
				return m_inner.size();
			}

			/** @copydoc IO::ByteStream::tell() */
			[[nodiscard]]
			int64_t
			tell () noexcept override
			{
				return m_inner.tell();
			}

			/** @copydoc IO::ByteStream::seek() */
			[[nodiscard]]
			int64_t
			seek (int64_t offset, int whence) noexcept override
			{
				return m_inner.seek(offset, whence);
			}

		private:

			IO::MemoryStream m_inner;
			int64_t m_failAfter;
	};
}
