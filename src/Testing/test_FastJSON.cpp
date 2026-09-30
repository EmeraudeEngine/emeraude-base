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

	TEST(FastJSON, nestingExactlyAtStackLimitIsRejectedNotThrown)
	{
		/* Regression (Ave robustus! A.3 — JSON-SFX fuzzer): jsoncpp's CharReader THROWS
		 * Json::RuntimeError when nesting depth REACHES stackLimit, which is std::terminate
		 * under -fno-exceptions. The pre-guard must reject at the limit, not only beyond it.
		 * Depth == limit was the crashing boundary (16 '[' at the default stackLimit of 16). */
		EXPECT_FALSE(getRootFromString(std::string(16, '['), 16, true).has_value());
		EXPECT_FALSE(getRootFromString(std::string(64, '['), 64, true).has_value());
		/* A document strictly under the limit still parses (sanity: the guard is not too eager). */
		EXPECT_TRUE(getRootFromString(R"({"a":{"b":{"c":1}}})", 16, true).has_value());
	}

	TEST(FastJSON, missingFileReturnsNullopt)
	{
		EXPECT_FALSE(getRootFromFile("/nonexistent/emeraude_base_fastjson_test.json", 16, true).has_value());
	}

	TEST(FastJSON, nonObjectNodeAccessorsAreSafe)
	{
		/* Regression (Ave robustus! A.3 — found by the JSON-SFX libFuzzer target): jsoncpp's
		 * isMember()/operator[] THROW Json::LogicError on a non-object value, which is
		 * std::terminate under -fno-exceptions. A top-level JSON array reaching getValue() was
		 * the crash. Every FastJSON key accessor must treat a non-object node as "no such key"
		 * and return nullopt, never reach jsoncpp's throwing path. */
		const Json::Value scalar{42};
		EXPECT_FALSE(getValue< int32_t >(scalar, "x").has_value());
		EXPECT_FALSE(getValue< std::string >(scalar, "x").has_value());
		EXPECT_FALSE(getArray(scalar, "x").has_value());
		EXPECT_FALSE(getObject(scalar, "x").has_value());

		Json::Value array{Json::arrayValue};
		array.append(1);
		array.append(2);
		EXPECT_FALSE(getValue< int32_t >(array, "x").has_value());
		EXPECT_FALSE(getArray(array, "x").has_value());
		EXPECT_FALSE(getObject(array, "x").has_value());
	}

	/* ===== Checked conversions (triad 6c, 2026-09-30): jsoncpp's as*() ABORT on an out-of-range integer or a
	 * non-number (its library throws Json::LogicError, std::terminate here) — every accessor answers nullopt. ===== */

	TEST(FastJSON, outOfRangeIntegersAreRefused)
	{
		const auto root = getRootFromString(R"({"negative":-1,"huge":1e20,"big":4294967296,"small":300,"frac":3.7,"negFrac":-3.7})");
		ASSERT_TRUE(root.has_value());

		EXPECT_FALSE(getValue< uint32_t >(*root, "negative").has_value());
		EXPECT_FALSE(getValue< uint64_t >(*root, "negative").has_value());
		EXPECT_FALSE(getValue< int32_t >(*root, "huge").has_value());
		EXPECT_FALSE(getValue< int64_t >(*root, "huge").has_value());
		EXPECT_FALSE(getValue< uint32_t >(*root, "big").has_value());
		EXPECT_FALSE(getValue< uint8_t >(*root, "small").has_value());   /* no silent wrap to 44 */
		EXPECT_FALSE(getValue< uint32_t >(*root, "negFrac").has_value());

		EXPECT_EQ(getValue< uint64_t >(*root, "big"), 4294967296ULL);
		EXPECT_EQ(getValue< int16_t >(*root, "small"), 300);
		EXPECT_EQ(getValue< int32_t >(*root, "negative"), -1);
		/* A fractional number still truncates toward zero, as before. */
		EXPECT_EQ(getValue< int32_t >(*root, "frac"), 3);
		EXPECT_EQ(getValue< int32_t >(*root, "negFrac"), -3);
	}

	TEST(FastJSON, nonFiniteFloatsAreRefused)
	{
		/* The parser accepts NaN / Infinity (allowSpecialFloats), and 1e999 reads as inf regardless. */
		const auto root = getRootFromString(R"({"nan":NaN,"inf":Infinity,"minusInf":-Infinity,"over":1e999,"wide":1e300,"ok":2.5})", 16, true);
		ASSERT_TRUE(root.has_value());

		for ( const auto * key : {"nan", "inf", "minusInf", "over", "wide"} )
		{
			EXPECT_FALSE(getValue< float >(*root, key).has_value()) << key;
		}

		EXPECT_FALSE(getValue< double >(*root, "nan").has_value());
		EXPECT_FALSE(getValue< double >(*root, "over").has_value());
		EXPECT_DOUBLE_EQ(getValue< double >(*root, "wide").value_or(0.0), 1e300);
		EXPECT_FLOAT_EQ(getValue< float >(*root, "ok").value_or(0.0F), 2.5F);
	}

	TEST(FastJSON, nonNumericElementsAreRefused)
	{
		const auto root = getRootFromString(R"({"letters":["a","b","c"],"mixed":[1,"x",3],"withBool":[1,true,0],"withNaN":[1,NaN,3],"good":[1,2,3],
			"matrix":[1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,"z",1]})", 16, true);
		ASSERT_TRUE(root.has_value());

		using Vector3 = Math::Vector< 3, float >;
		using Matrix4 = Math::Matrix< 4, float >;

		EXPECT_FALSE(getValue< Vector3 >(*root, "letters").has_value());
		EXPECT_FALSE(getValue< Vector3 >(*root, "mixed").has_value());
		EXPECT_FALSE(getValue< Vector3 >(*root, "withBool").has_value());
		EXPECT_FALSE(getValue< Vector3 >(*root, "withNaN").has_value());
		EXPECT_FALSE(getValue< PixelFactory::Color< float > >(*root, "mixed").has_value());
		EXPECT_FALSE(getValue< Matrix4 >(*root, "matrix").has_value());

		const auto good = getValue< Vector3 >(*root, "good");
		ASSERT_TRUE(good.has_value());
		EXPECT_FLOAT_EQ(good->z(), 3.0F);
	}

	TEST(FastJSON, asValueOnBareNodes)
	{
		EXPECT_FALSE(asValue< float >(Json::Value{"1.5"}).has_value());   /* a string is not a number */
		EXPECT_FALSE(asValue< float >(Json::Value{true}).has_value());    /* nor is a boolean */
		EXPECT_FALSE(asValue< bool >(Json::Value{1}).has_value());
		EXPECT_FALSE(asValue< std::string >(Json::Value{3}).has_value());
		EXPECT_FALSE(asValue< uint32_t >(Json::Value{-1}).has_value());
		EXPECT_FALSE(asValue< float >(Json::Value{Json::objectValue}).has_value());
		EXPECT_FALSE(asValue< float >(Json::Value{}).has_value());        /* null: absent, not 0 */

		EXPECT_FLOAT_EQ(asValue< float >(Json::Value{1.5}).value_or(0.0F), 1.5F);
		EXPECT_EQ(asValue< uint32_t >(Json::Value{64.0}), 64U);           /* an integral real */
		EXPECT_EQ(asValue< bool >(Json::Value{false}), false);
		EXPECT_EQ(asValue< std::string >(Json::Value{"hotel"}), std::string{"hotel"});
	}
}
