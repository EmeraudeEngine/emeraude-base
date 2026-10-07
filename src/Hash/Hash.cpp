/*
 * src/Hash/Hash.cpp
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

#include "Hash.hpp"

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

/* Local inclusions. */
#include "CRC32.hpp"
#include "MD5.hpp"
#include "SHA1.hpp"
#include "SHA256.hpp"
#include "SHA512.hpp"

namespace EmEn::Base::Hash
{
	std::string
	_toString (const uint8_t * data, size_t size, size_t stringSize) noexcept
	{
		std::string hash(stringSize, '\0');

		for ( size_t i = 0; i < size; i++ )
		{
			//sprintf(&hash[i * 2], "%02x", data[i]);
			snprintf(&hash[i * 2], 3, "%02x", data[i]);
		}

		return hash;
	}

	std::string
	md5 (const std::string & input) noexcept
	{
		std::array< uint8_t, 16 > digest{0};

		MD5 hash{};
		hash.update(reinterpret_cast< const uint8_t * >(input.data()), input.size());
		hash.final(digest);

		return _toString(digest.data(), digest.size(), MD5::HashLength);
	}

	std::string
	sha1 (const std::string & input) noexcept
	{
		std::array< uint8_t, 20 > digest{0};

		SHA1 hash{};
		hash.update(reinterpret_cast< const uint8_t * >(input.data()), input.size());
		hash.final(digest);

		return _toString(digest.data(), digest.size(), SHA1::HashLength);
	}

	std::string
	sha256 (const std::string & input) noexcept
	{
		std::array< uint8_t, 32 > digest{0};

		SHA256 hash{};
		hash.update(reinterpret_cast< const uint8_t * >(input.data()), input.size());
		hash.final(digest);

		return _toString(digest.data(), digest.size(), SHA256::HashLength);
	}

	std::string
	hmacSha256 (const std::string & key, const std::string & message) noexcept
	{
		/* NOTE: RFC 2104 with B = 64 (the SHA-256 block): H((K ^ opad) || H((K ^ ipad) || message)). */
		constexpr size_t BlockSize{64};

		std::array< uint8_t, BlockSize > paddedKey{0};

		if ( key.size() > BlockSize )
		{
			std::array< uint8_t, 32 > hashedKey{0};

			SHA256 hash{};
			hash.update(reinterpret_cast< const uint8_t * >(key.data()), key.size());
			hash.final(hashedKey);

			std::copy(hashedKey.cbegin(), hashedKey.cend(), paddedKey.begin());
		}
		else
		{
			std::copy(key.cbegin(), key.cend(), reinterpret_cast< char * >(paddedKey.data()));
		}

		std::array< uint8_t, BlockSize > innerPad{0};
		std::array< uint8_t, BlockSize > outerPad{0};

		for ( size_t index = 0; index < BlockSize; index++ )
		{
			innerPad[index] = paddedKey[index] ^ 0x36U;
			outerPad[index] = paddedKey[index] ^ 0x5CU;
		}

		std::array< uint8_t, 32 > innerDigest{0};

		SHA256 inner{};
		inner.update(innerPad.data(), innerPad.size());
		inner.update(reinterpret_cast< const uint8_t * >(message.data()), message.size());
		inner.final(innerDigest);

		std::array< uint8_t, 32 > digest{0};

		SHA256 outer{};
		outer.update(outerPad.data(), outerPad.size());
		outer.update(innerDigest.data(), innerDigest.size());
		outer.final(digest);

		return _toString(digest.data(), digest.size(), SHA256::HashLength);
	}

	std::string
	sha512 (const std::string & input) noexcept
	{
		std::array< uint8_t, 64 > digest{0};

		SHA512 hash{};
		hash.update(reinterpret_cast< const uint8_t * >(input.data()), input.size());
		hash.final(digest);

		return _toString(digest.data(), digest.size(), SHA512::HashLength);
	}

	std::string
	crc32 (const std::string & input) noexcept
	{
		std::array< uint8_t, 4 > digest{0};

		CRC32 hash{};
		hash.update(reinterpret_cast< const uint8_t * >(input.data()), input.size());
		hash.final(digest);

		return _toString(digest.data(), digest.size(), CRC32::HashLength);
	}
}
