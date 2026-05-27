/*
 * src/Hash/Hash.hpp
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
#include <string>

namespace EmEn::Base::Hash
{
	/**
	 * @brief Hash a string using the MD5 algorithm.
	 * @param input A reference to a string.
	 * @return std::string.
	 */
	[[nodiscard]]
	std::string md5 (const std::string & input) noexcept;

	/**
	 * @brief Hash a string using SHA-2 (256 bits) algorithm.
	 * @param input A reference to a string.
	 * @return std::string.
	 */
	[[nodiscard]]
	std::string sha256 (const std::string & input) noexcept;

	/**
	 * @brief Hash a string using SHA-2 (512 bits) algorithm.
	 * @param input A reference to a string.
	 * @return std::string.
	 */
	[[nodiscard]]
	std::string sha512 (const std::string & input) noexcept;

	/**
	 * @brief Hash a string using the CRC-32 algorithm (IEEE 802.3 / zlib / PNG).
	 * @param input A reference to a string.
	 * @return std::string.
	 */
	[[nodiscard]]
	std::string crc32 (const std::string & input) noexcept;
}
