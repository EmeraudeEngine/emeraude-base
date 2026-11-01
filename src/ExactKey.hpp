/*
 * src/ExactKey.hpp
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
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>

namespace EmEn::Base
{
	/**
	 * @brief The EXACT identity of a cached object: the key of a cache that SERVES what it finds (a shader program, a
	 * graphics pipeline), where two different requests must never meet.
	 * @note The key is the byte sequence of every input, in the order they are added: each value in its own fixed width
	 * (an integer, a boolean, an enumeration and a pointer widened to 64 bits; a float by its bits), each text with its
	 * length in front — so two different input lists never give the same bytes. The equality compares those bytes; the
	 * hash only places the key in a hash map. A cache keyed by the hash alone serves one object for another on a
	 * collision (the engine's program cache did, 2026-10-31).
	 * @note Built when the cached object is created or looked up (a cold path): it allocates.
	 * @note Floats are compared by their bits: -0.0 and +0.0 are two keys, and so are two NaN payloads — what a
	 * creation would receive.
	 */
	class ExactKey final
	{
		public:

			/** @brief Starts an empty key. */
			ExactKey () noexcept = default;

			/**
			 * @brief Starts the key of a domain (the class that builds it), so two builders never share one.
			 * @param domain The domain name (a ClassId).
			 */
			explicit
			ExactKey (std::string_view domain) noexcept
			{
				this->add(domain);
			}

			/**
			 * @brief Adds an integer, a boolean or an enumeration value, in a fixed 64-bit width.
			 * @tparam value_t An integral or enumeration type.
			 * @param value The value.
			 * @return ExactKey &
			 */
			template< typename value_t >
			requires std::integral< value_t > || std::is_enum_v< value_t >
			ExactKey &
			add (value_t value) noexcept
			{
				uint64_t widened = 0;

				if constexpr ( std::is_enum_v< value_t > )
				{
					widened = static_cast< uint64_t >(static_cast< std::underlying_type_t< value_t > >(value));
				}
				else
				{
					widened = static_cast< uint64_t >(value);
				}

				this->append(widened);

				return *this;
			}

			/**
			 * @brief Adds a floating point value by its bits, widened to 64 bits.
			 * @tparam value_t float or double.
			 * @param value The value.
			 * @return ExactKey &
			 */
			template< typename value_t >
			requires std::same_as< value_t, float > || std::same_as< value_t, double >
			ExactKey &
			add (value_t value) noexcept
			{
				if constexpr ( std::same_as< value_t, float > )
				{
					this->append(static_cast< uint64_t >(std::bit_cast< uint32_t >(value)));
				}
				else
				{
					this->append(std::bit_cast< uint64_t >(value));
				}

				return *this;
			}

			/**
			 * @brief Adds an object's identity (a Vulkan handle, an effect): its address.
			 * @param pointer The pointer (null is a valid value).
			 * @return ExactKey &
			 */
			ExactKey &
			add (const void * pointer) noexcept
			{
				this->append(static_cast< uint64_t >(std::bit_cast< uintptr_t >(pointer)));

				return *this;
			}

			/**
			 * @brief Refused: a C string would bind to add(const void *) and be keyed by its ADDRESS, not its text.
			 * Pass a std::string_view (its text) or a std::string.
			 */
			ExactKey & add (const char *) noexcept = delete;

			/**
			 * @brief Adds a text, its length first.
			 * @param text The text.
			 * @return ExactKey &
			 */
			ExactKey &
			add (std::string_view text) noexcept
			{
				this->append(static_cast< uint64_t >(text.size()));

				m_bytes.append(text);

				return *this;
			}

			/**
			 * @brief Compares two keys byte for byte.
			 * @param other A reference to the other key.
			 * @return bool
			 */
			[[nodiscard]]
			bool operator== (const ExactKey & other) const noexcept = default;

			/**
			 * @brief Returns the hash of the key, to place it in a hash map.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			hash () const noexcept
			{
				return std::hash< std::string >{}(m_bytes);
			}

			/**
			 * @brief Returns the number of bytes of the key.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			size () const noexcept
			{
				return m_bytes.size();
			}

		private:

			/**
			 * @brief Appends the eight bytes of a value.
			 * @param value The value.
			 */
			void
			append (uint64_t value) noexcept
			{
				const auto bytes = std::bit_cast< std::array< char, sizeof(uint64_t) > >(value);

				m_bytes.append(bytes.data(), bytes.size());
			}

			std::string m_bytes;
	};
}

/**
 * @brief The hash of an ExactKey, for std::unordered_map.
 */
template<>
struct std::hash< EmEn::Base::ExactKey >
{
	[[nodiscard]]
	size_t
	operator() (const EmEn::Base::ExactKey & key) const noexcept
	{
		return key.hash();
	}
};
