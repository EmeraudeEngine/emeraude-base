/*
 * src/Hash/SHA1.hpp
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
#include <cstddef>
#include <cstdint>

namespace EmEn::Base::Hash
{
	/**
	 * @brief The SHA1 class (FIPS-180-4, 160-bit digest).
	 * @note SHA-1 is cryptographically broken (collisions are practical) and MUST NOT be
	 * used for security purposes. It is provided for interoperability with legacy formats
	 * and non-cryptographic checksums only.
	 */
	class SHA1 final
	{
		public:

			static constexpr auto HashLength = 40UL;

			/**
			 * @brief Constructs the SHA1 hash object.
			 */
			SHA1 () noexcept = default;

			/**
			 * @brief processLogics
			 * @param message
			 * @param length
			 * @return void
			 */
			void update (const uint8_t * message, size_t length) noexcept;

			/**
			 * @brief final
			 * @param digest
			 * @return void
			 */
			void final (std::array< uint8_t, 20 > & digest) noexcept;

			/**
			 * @brief reset
			 * @return void
			 */
			void reset () noexcept;

		private:

			static constexpr auto BlockSize = 512UL / 8UL;

			/**
			 * @brief transform
			 * @param message
			 * @param length
			 * @return void
			 */
			void transform (const uint8_t * message, size_t length) noexcept;

			size_t m_totalLength{0};
			size_t m_length{0};
			std::array< uint8_t, 2 * BlockSize > m_block{};
			std::array< uint32_t, 5 > m_h{
				0x67452301,
				0xEFCDAB89,
				0x98BADCFE,
				0x10325476,
				0xC3D2E1F0
			};
	};
}