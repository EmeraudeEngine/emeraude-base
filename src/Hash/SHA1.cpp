/*
 * src/Hash/SHA1.cpp
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

#include "SHA1.hpp"

/* STL inclusions. */
#include <cstring>

#define SHA1_ROTL(x, n) (((x) << (n)) | ((x) >> ((sizeof(x) << 3) - (n))))
#define SHA1_UNPACK32(x, str)				 \
{											 \
	*((str) + 3) = (uint8_t) ((x)	  );	   \
	*((str) + 2) = (uint8_t) ((x) >>  8);	   \
	*((str) + 1) = (uint8_t) ((x) >> 16);	   \
	*((str) + 0) = (uint8_t) ((x) >> 24);	   \
}
#define SHA1_PACK32(str, x)				   \
{											 \
	*(x) =   ((uint32_t) *((str) + 3)	  )	\
		   | ((uint32_t) *((str) + 2) <<  8)	\
		   | ((uint32_t) *((str) + 1) << 16)	\
		   | ((uint32_t) *((str) + 0) << 24);   \
}

namespace EmEn::Base::Hash
{
	void
	SHA1::transform (const uint8_t * message, size_t length) noexcept
	{
		std::array< uint32_t, 80 > w{0};

		for ( size_t i = 0; i < length; i++ )
		{
			const auto * subBlock = message + (i << 6);

			for ( size_t j = 0; j < 16; j++ )
			{
				SHA1_PACK32(&subBlock[j << 2], &w[j]);
			}

			for ( size_t j = 16; j < w.size(); j++ )
			{
				w[j] = SHA1_ROTL(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
			}

			auto a = m_h[0];
			auto b = m_h[1];
			auto c = m_h[2];
			auto d = m_h[3];
			auto e = m_h[4];

			for ( size_t j = 0; j < w.size(); j++ )
			{
				uint32_t f = 0;
				uint32_t k = 0;

				if ( j < 20 )
				{
					f = (b & c) | (~b & d);
					k = 0x5A827999;
				}
				else if ( j < 40 )
				{
					f = b ^ c ^ d;
					k = 0x6ED9EBA1;
				}
				else if ( j < 60 )
				{
					f = (b & c) | (b & d) | (c & d);
					k = 0x8F1BBCDC;
				}
				else
				{
					f = b ^ c ^ d;
					k = 0xCA62C1D6;
				}

				const auto temp = SHA1_ROTL(a, 5) + f + e + k + w[j];

				e = d;
				d = c;
				c = SHA1_ROTL(b, 30);
				b = a;
				a = temp;
			}

			m_h[0] += a;
			m_h[1] += b;
			m_h[2] += c;
			m_h[3] += d;
			m_h[4] += e;
		}
	}

	void
	SHA1::reset () noexcept
	{
		m_length = 0;
		m_totalLength = 0;

		m_h[0] = 0x67452301;
		m_h[1] = 0xEFCDAB89;
		m_h[2] = 0x98BADCFE;
		m_h[3] = 0x10325476;
		m_h[4] = 0xC3D2E1F0;
	}

	void
	SHA1::update (const uint8_t * message, size_t length) noexcept
	{
		const auto temporaryLength = SHA1::BlockSize - m_length;
		auto remainingLength = length < temporaryLength ? length : temporaryLength;

		memcpy(&m_block[m_length], message, remainingLength);

		if ( m_length + length < SHA1::BlockSize )
		{
			m_length += length;

			return;
		}

		this->transform(m_block.data(), 1);

		auto newLength = length - remainingLength;
		auto blockSize = newLength / SHA1::BlockSize;
		const auto * shiftedMessage = message + remainingLength;

		this->transform(shiftedMessage, blockSize);

		remainingLength = newLength % SHA1::BlockSize;

		memcpy(m_block.data(), &shiftedMessage[blockSize << 6], remainingLength);

		m_length = remainingLength;
		m_totalLength += (blockSize + 1) << 6;
	}

	void
	SHA1::final (std::array< uint8_t, 20 > & digest) noexcept
	{
		auto blockSize = 1 + static_cast< int >((SHA1::BlockSize - 9) < (m_length % SHA1::BlockSize));
		auto len_b = (m_totalLength + m_length) << 3;
		auto pm_length = blockSize << 6;

		memset(m_block.data() + m_length, 0, pm_length - m_length);

		m_block[m_length] = 0x80;

		SHA1_UNPACK32(len_b, m_block.data() + pm_length - 4);

		this->transform(m_block.data(), blockSize);

		for ( auto i = 0; i < 5; i++ )
		{
			SHA1_UNPACK32(m_h[i], &digest[i << 2]);
		}
	}
}