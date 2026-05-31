/*
 * src/Testing/test_FastJSON.cpp
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

/* STL inclusions. */
#include <array>
#include <string>
#include <string_view>

/* Local inclusions. */
#include "FastJSON.hpp"
#include "Math/Vector.hpp"
#include "PixelFactory/Color.hpp"

namespace EmEn::Base::FastJSON
{
	/* ===== Nominal parsing & typed extraction ===== */

	TEST(FastJSON, parseAndTypedGet)
	{
		const auto root = getRootFromString(R"({"count":42,"ratio":0.5,"flag":true,"name":"hotel"})");
		ASSERT_TRUE(root.has_value());

		EXPECT_EQ(getValue< int32_t >(*root, "count"), 42);
		EXPECT_DOUBLE_EQ(getValue< double >(*root, "ratio").value_or(-1.0), 0.5);
		EXPECT_EQ(getValue< bool >(*root, "flag"), true);
		EXPECT_EQ(getValue< std::string >(*root, "name"), std::string{"hotel"});

		/* Missing key and wrong-type → nullopt, never a throw/UB. */
		EXPECT_FALSE(getValue< int32_t >(*root, "absent").has_value());
		EXPECT_FALSE(getValue< int32_t >(*root, "name").has_value());   /* string, not number */
		EXPECT_FALSE(getValue< std::string >(*root, "count").has_value());
	}

	TEST(FastJSON, arrayObjectVectorColor)
	{
		const auto root = getRootFromString(R"({"obj":{"k":1},"arr":[1,2,3],"vec":[1.0,2.0,3.0],"col":[0.1,0.2,0.3,1.0]})");
		ASSERT_TRUE(root.has_value());

		EXPECT_TRUE(getObject(*root, "obj").has_value());
		EXPECT_FALSE(getObject(*root, "arr").has_value());   /* array, not object */
		EXPECT_TRUE(getArray(*root, "arr").has_value());
		EXPECT_FALSE(getArray(*root, "obj").has_value());

		const auto vec = getValue< Math::Vector< 3, float > >(*root, "vec");
		ASSERT_TRUE(vec.has_value());
		EXPECT_FLOAT_EQ(vec->x(), 1.0F);
		EXPECT_FLOAT_EQ(vec->y(), 2.0F);
		EXPECT_FLOAT_EQ(vec->z(), 3.0F);

		/* A 4-component array parses as a colour (parse path coverage). */
		EXPECT_TRUE(getValue< PixelFactory::Color< float > >(*root, "col").has_value());
	}

	TEST(FastJSON, validatedStringValue)
	{
		const auto root = getRootFromString(R"({"Type":"PBR"})");
		ASSERT_TRUE(root.has_value());

		constexpr std::array< std::string_view, 3 > allowed{"Basic", "Standard", "PBR"};
		EXPECT_EQ(getValidatedStringValue(*root, "Type", allowed), std::string{"PBR"});

		constexpr std::array< std::string_view, 2 > other{"Basic", "Standard"};
		EXPECT_FALSE(getValidatedStringValue(*root, "Type", other).has_value());   /* not in the list */
	}

	TEST(FastJSON, stringifyRoundTrip)
	{
		const auto first = getRootFromString(R"({"a":1,"b":[2,3]})");
		ASSERT_TRUE(first.has_value());

		const auto text = stringify(*first);
		const auto second = getRootFromString(text);
		ASSERT_TRUE(second.has_value());

		EXPECT_EQ(getValue< int32_t >(*second, "a"), 1);
		EXPECT_TRUE(getArray(*second, "b").has_value());
	}

	/* ===== Malformed / hostile input (run under ASan/UBSan: must fail gracefully, never crash) ===== */

	TEST(FastJSON, malformedInputsRejected)
	{
		/* quiet=true to keep the test output clean. Each must return nullopt. */
		EXPECT_FALSE(getRootFromString("", 16, true).has_value());                  /* empty */
		EXPECT_FALSE(getRootFromString(R"({"a":1)", 16, true).has_value());         /* truncated */
		EXPECT_FALSE(getRootFromString("{not json}", 16, true).has_value());        /* invalid */
		EXPECT_FALSE(getRootFromString("[1,2,]", 16, true).has_value());            /* trailing comma off */
		EXPECT_FALSE(getRootFromString(R"({"a":1/*c*/})", 16, true).has_value());   /* comments off */
		EXPECT_FALSE(getRootFromString(R"({"a":1,"a":2})", 16, true).has_value());  /* rejectDupKeys */
		EXPECT_FALSE(getRootFromString("{}trailing", 16, true).has_value());        /* failIfExtra */
		EXPECT_FALSE(getRootFromString("42", 16, true).has_value());                /* strictRoot: bare scalar */
	}

	TEST(FastJSON, deeplyNestedDoesNotOverflow)
	{
		/* A hostile, very deeply nested document must hit the stackLimit and be rejected
		 * gracefully — NOT overflow the stack. Depth far exceeds the limit. */
		const std::string deep = std::string(2000, '[') + std::string(2000, ']');
		EXPECT_FALSE(getRootFromString(deep, 16, true).has_value());
	}

	TEST(FastJSON, missingFileReturnsNullopt)
	{
		EXPECT_FALSE(getRootFromFile("/nonexistent/emeraude_base_fastjson_test.json", 16, true).has_value());
	}
}
