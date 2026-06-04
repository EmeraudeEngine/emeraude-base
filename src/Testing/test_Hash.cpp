/*
 * src/Testing/test_Hash.cpp
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

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "Hash/Hash.hpp"
#include "Hash/Types.hpp"

using namespace EmEn::Base;

TEST(Hash, md5)
{
	ASSERT_EQ(Hash::md5("TestString"), "5b56f40f8828701f97fa4511ddcd25fb");
}

TEST(Hash, sha1)
{
	ASSERT_EQ(Hash::sha1("TestString"), "d598b03bee8866ae03b54cb6912efdfef107fd6d");
}

TEST(Hash, sha1KnownAnswer)
{
	/* Ave robustus! (Axis B): FIPS-180-4 known-answer vectors. */
	ASSERT_EQ(Hash::sha1("abc"), "a9993e364706816aba3e25717850c26c9cd0d89d");
	ASSERT_EQ(Hash::sha1(""), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

TEST(Hash, sha256)
{
	ASSERT_EQ(Hash::sha256("TestString"), "6dd79f2770a0bb38073b814a5ff000647b37be5abbde71ec9176c6ce0cb32a27");
}

TEST(Hash, sha512)
{
	ASSERT_EQ(Hash::sha512("TestString"), "69dfd91314578f7f329939a7ea6be4497e6fe3909b9c8f308fe711d29d4340d90d77b7fdf359b7d0dbeed940665274f7ca514cd067895fdf59de0cf142b62336");
}

TEST(Hash, sha512KnownAnswer)
{
	/* Ave robustus! (Axis B): FIPS-180-4 known-answer vectors, not just a custom string. */
	ASSERT_EQ(Hash::sha512("abc"),
		"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
	ASSERT_EQ(Hash::sha512(""),
		"cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
}

TEST(Hash, typeEnumRoundTrip)
{
	/* Ave robustus! (Axis B): the HashType enum must match the implemented algorithms — every
	 * value (CRC32, MD5, SHA1, SHA256, SHA512) is now backed by a real implementation.
	 * Round-trip: to_HashType(to_string(x)) == x for every value. */
	for ( const auto value : {Hash::HashType::Undefined, Hash::HashType::CRC32, Hash::HashType::MD5, Hash::HashType::SHA1, Hash::HashType::SHA256, Hash::HashType::SHA512} )
	{
		EXPECT_EQ(Hash::to_HashType(Hash::to_string(value)), value);
	}

	EXPECT_EQ(Hash::to_string(Hash::HashType::SHA512), "SHA512");

	/* SHA1 is now implemented (Hash/SHA1) — it must resolve again. */
	EXPECT_EQ(Hash::to_HashType("SHA1"), Hash::HashType::SHA1);
	EXPECT_EQ(Hash::to_string(Hash::HashType::SHA1), "SHA1");
}
