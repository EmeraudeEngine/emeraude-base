/*
 * src/Testing/test_Variant.cpp
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
#include <cstdint>
#include <string>
#include <string_view>

/* Local inclusions. */
#include "Variant.hpp"
#include "Logging/Logging.hpp"
#include "Logging/Severity.hpp"

namespace EmEn::Base
{
	using Type = Variant::Type;

	/* ===== Null / lifecycle ===== */

	TEST(Variant, defaultConstructedIsNull)
	{
		const Variant v;
		EXPECT_TRUE(v.isNull());
		EXPECT_EQ(v.type(), Type::Null);
	}

	TEST(Variant, resetReturnsToNull)
	{
		Variant v{int32_t{42}};
		ASSERT_FALSE(v.isNull());
		v.reset();
		EXPECT_TRUE(v.isNull());
		EXPECT_EQ(v.type(), Type::Null);
	}

	TEST(Variant, copyIsIndependent)
	{
		Variant v{int32_t{42}};
		Variant copy = v;
		EXPECT_EQ(copy.type(), Type::Integer32);
		EXPECT_EQ(copy.asInteger32(), 42);

		v.reset();
		EXPECT_TRUE(v.isNull());
		EXPECT_FALSE(copy.isNull());   /* the copy is unaffected by resetting the source */
		EXPECT_EQ(copy.asInteger32(), 42);
	}

	TEST(Variant, setReplacesValueAndType)
	{
		Variant v{1.0F};
		ASSERT_EQ(v.type(), Type::Float);

		v.set(int16_t{7});
		EXPECT_EQ(v.type(), Type::Integer16);
		EXPECT_EQ(v.asInteger16(), 7);
	}

	/* ===== Type/index invariant + value round-trips ===== */

	TEST(Variant, integerRoundTrips)
	{
		{ const Variant v{int8_t{-8}};    EXPECT_EQ(v.type(), Type::Integer8);          EXPECT_EQ(v.asInteger8(), int8_t{-8}); }
		{ const Variant v{uint8_t{8}};    EXPECT_EQ(v.type(), Type::UnsignedInteger8);  EXPECT_EQ(v.asUnsignedInteger8(), uint8_t{8}); }
		{ const Variant v{int16_t{-16}};  EXPECT_EQ(v.type(), Type::Integer16);         EXPECT_EQ(v.asInteger16(), int16_t{-16}); }
		{ const Variant v{uint16_t{16}};  EXPECT_EQ(v.type(), Type::UnsignedInteger16); EXPECT_EQ(v.asUnsignedInteger16(), uint16_t{16}); }
		{ const Variant v{int32_t{-32}};  EXPECT_EQ(v.type(), Type::Integer32);         EXPECT_EQ(v.asInteger32(), int32_t{-32}); }
		{ const Variant v{uint32_t{32}};  EXPECT_EQ(v.type(), Type::UnsignedInteger32); EXPECT_EQ(v.asUnsignedInteger32(), uint32_t{32}); }
		{ const Variant v{int64_t{-64}};  EXPECT_EQ(v.type(), Type::Integer64);         EXPECT_EQ(v.asInteger64(), int64_t{-64}); }
		{ const Variant v{uint64_t{64}};  EXPECT_EQ(v.type(), Type::UnsignedInteger64); EXPECT_EQ(v.asUnsignedInteger64(), uint64_t{64}); }
	}

	TEST(Variant, floatingAndBoolRoundTrips)
	{
		{ const Variant v{1.5F};  EXPECT_EQ(v.type(), Type::Float);      EXPECT_FLOAT_EQ(v.asFloat(), 1.5F); }
		{ const Variant v{2.5};   EXPECT_EQ(v.type(), Type::Double);     EXPECT_DOUBLE_EQ(v.asDouble(), 2.5); }
		{ const Variant v{3.5L};  EXPECT_EQ(v.type(), Type::LongDouble); EXPECT_EQ(v.asLongDouble(), 3.5L); }
		{ const Variant v{true};  EXPECT_EQ(v.type(), Type::Boolean);    EXPECT_TRUE(v.asBool()); }
		{ const Variant v{false}; EXPECT_EQ(v.type(), Type::Boolean);    EXPECT_FALSE(v.asBool()); }
	}

	TEST(Variant, vectorRoundTrips)
	{
		{
			const Variant v{Math::Vector2F{1.0F, 2.0F}};
			EXPECT_EQ(v.type(), Type::Vector2Float);
			EXPECT_FLOAT_EQ(v.asVector2Float().x(), 1.0F);
			EXPECT_FLOAT_EQ(v.asVector2Float().y(), 2.0F);
		}
		{
			const Variant v{Math::Vector3F{1.0F, 2.0F, 3.0F}};
			EXPECT_EQ(v.type(), Type::Vector3Float);
			EXPECT_FLOAT_EQ(v.asVector3Float().z(), 3.0F);
		}
		{
			const Variant v{Math::Vector4F{1.0F, 2.0F, 3.0F, 4.0F}};
			EXPECT_EQ(v.type(), Type::Vector4Float);
			EXPECT_FLOAT_EQ(v.asVector4Float().w(), 4.0F);
		}
	}

	TEST(Variant, matrixColorAndFrameTypesArePreserved)
	{
		/* These types share the same get_if<T> extraction path proven above; here we pin
		 * the Type/index invariant (the serialization-critical contract) for each. */
		{ const Variant v{Math::Matrix2F{}};        EXPECT_EQ(v.type(), Type::Matrix2Float);        EXPECT_FALSE(v.isNull()); }
		{ const Variant v{Math::Matrix3F{}};        EXPECT_EQ(v.type(), Type::Matrix3Float);        EXPECT_FALSE(v.isNull()); }
		{ const Variant v{Math::Matrix4F{}};        EXPECT_EQ(v.type(), Type::Matrix4Float);        EXPECT_FALSE(v.isNull()); }
		{ const Variant v{Math::CartesianFrameF{}}; EXPECT_EQ(v.type(), Type::CartesianFrameFloat); EXPECT_FALSE(v.isNull()); }
		{ const Variant v{PixelFactory::ColorF{0.1F, 0.2F, 0.3F, 1.0F}}; EXPECT_EQ(v.type(), Type::Color); EXPECT_FALSE(v.isNull()); }
	}

	/* ===== to_cstring naming ===== */

	TEST(Variant, toCstringNames)
	{
		EXPECT_STREQ(Variant::to_cstring(Type::Null), "Null");
		EXPECT_STREQ(Variant::to_cstring(Type::Integer8), "Integer8");
		EXPECT_STREQ(Variant::to_cstring(Type::Float), "Float");
		EXPECT_STREQ(Variant::to_cstring(Type::Vector3Float), "Vector3Float");
		EXPECT_STREQ(Variant::to_cstring(Type::CartesianFrameFloat), "CartesianFrameFloat");
		EXPECT_STREQ(Variant::to_cstring(Type::Color), "Color");
	}

	/* ===== Type-mismatch accessor: exception-free, default value, logged via the hook =====
	 *
	 * Calling the wrong asXxx() is a caller-contract violation. The doctrine: never crash,
	 * never UB (std::get_if guards), return a value-initialised default, and report through
	 * the Logging hook (NOT a raw std::cerr). Run under ASan/UBSan, a mismatched read must
	 * not dereference a null alternative. */
	TEST(Variant, typeMismatchReturnsDefaultAndLogsViaHook)
	{
		struct Capture
		{
			int count{0};
			Severity severity{Severity::Debug};
			std::string tag;
			std::string message;
		};

		Capture capture;

		Logging::setSink([&capture](Severity severity, const char * tag, std::string_view message) noexcept {
			++capture.count;
			capture.severity = severity;
			capture.tag = tag != nullptr ? tag : "";
			capture.message = std::string{message};
		});

		const Variant v{3.14F};   /* holds Float */

		/* Wrong accessor → default, no crash. */
		EXPECT_EQ(v.asInteger32(), 0);
		EXPECT_EQ(capture.count, 1);
		EXPECT_EQ(capture.severity, Severity::Error);
		EXPECT_EQ(capture.tag, std::string{"Variant"});
		EXPECT_NE(capture.message.find("Integer32"), std::string::npos);   /* requested */
		EXPECT_NE(capture.message.find("Float"), std::string::npos);       /* actually held */

		/* The matching accessor must NOT log. */
		capture.count = 0;
		EXPECT_FLOAT_EQ(v.asFloat(), 3.14F);
		EXPECT_EQ(capture.count, 0);

		/* A complex (non-scalar) mismatch must also stay safe and return a default. */
		EXPECT_FLOAT_EQ(v.asVector3Float().x(), 0.0F);
		EXPECT_EQ(capture.count, 1);

		Logging::setSink({});   /* restore the default sink */
	}
}
