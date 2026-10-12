/*
 * src/BlobTrait.hpp
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
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

/* Local inclusions. */
#include "IO/IO.hpp"

namespace EmEn::Base
{
	/**
	 * @brief Class to ease the creation of a std::string using the std::cout syntax.
	 */
	class BlobTrait
	{
		public:

			/**
			 * @brief Constructs a blob.
			 */
			BlobTrait () = default;

			/**
			 * @brief Constructs a blob with an initial string view.
			 * @param initialString A string view.
			 */
			explicit
			BlobTrait (std::string_view initialString)
				: m_stream{std::string{initialString}}
			{

			}

			/**
			 * @brief Deleted copy constructor.
			 * @param other A reference to a blob.
			 */
			BlobTrait (const BlobTrait & other) = delete;

			/**
			 * @brief Deleted assignment operator.
			 * @param other A reference to a blob.
			 * @return BlobTrait &
			 */
			BlobTrait & operator= (const BlobTrait & other) = delete;

			/**
			 * @brief Deleted move constructor.
			 * @param other A reference to a blob.
			 */
			BlobTrait (BlobTrait && other) noexcept = delete;

			/**
			 * @brief Deleted move assignment operator.
			 * @param other A reference to a blob.
			 * @return BlobTrait &
			 */
			BlobTrait & operator= (BlobTrait && other) noexcept = delete;

			/**
			 * @brief Destructs the blob.
			 */
			~BlobTrait () = default;

			/**
			 * @brief Adds a value to the blob content.
			 * @tparam data_t The type of the data.
			 * @param value A reference to the data.
			 * @return BlobTrait &
			 */
			template< typename data_t >
			BlobTrait &
			operator<< (const data_t & value)
			{
				m_stream << value;

				return *this;
			}

			/**
			 * @brief Adds a boolean value to the blob content.
			 * @param value The value of the boolean.
			 * @return BlobTrait &
			 */
			BlobTrait &
			operator<< (bool value)
			{
				m_stream << ( value ? "True" : "False" );

				return *this;
			}

			/**
			 * @brief Adds a raw pointer value to the blob content.
			 * @param value The value of the pointer.
			 * @return BlobTrait &
			 */
			BlobTrait &
			operator<< (void * value)
			{
				m_stream << '@' << value;

				return *this;
			}

			/**
			 * @brief Adds a raw pointer value to the blob content.
			 * @param value The value of the pointer.
			 * @return BlobTrait &
			 */
			BlobTrait &
			operator<< (const void * value)
			{
				m_stream << '@' << value;

				return *this;
			}

			/**
			 * @brief Adds a filesystem path to the blob content: UTF-8 (IO::toU8String()), quoted as std's operator<<
			 * quotes it.
			 * @note Never std's operator<<, which writes path::string(): on Windows the ANSI code page, which THROWS on a
			 * character outside it (an abort with exceptions off) and writes invalid UTF-8 for the others (rule C,
			 * 2026-10-12). A std::string argument still takes the template above: only a path is quoted.
			 * @param value A reference to a path.
			 * @return BlobTrait &
			 */
			BlobTrait &
			operator<< (const std::filesystem::path & value)
			{
				m_stream << std::quoted(IO::toU8String(value));

				return *this;
			}

			/**
			 * @brief Adds a stream to the blob content.
			 * @param value The value of the pointer.
			 * @return BlobTrait &
			 */
			BlobTrait &
			operator<< (const std::stringstream & value)
			{
				m_stream << value.str();

				return *this;
			}

			/**
			 * @brief Returns the blob content in a string.
			 * @return std::string
			 */
			std::string
			get () const
			{
				return m_stream.str();
			}

		private:

			std::stringstream m_stream;
	};
}
