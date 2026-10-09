/*
 * src/Testing/test_MathVector.cpp
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

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <type_traits>

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "Math/Base.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base::Math;

using MathTypeList = testing::Types< int, float, double >;

template< typename >
struct MathVector
	: testing::Test
{

};

TYPED_TEST_SUITE(MathVector, MathTypeList);

/* Helper function for floating-point comparison */
template< typename T >
constexpr
T
epsilon ()
{
	if constexpr ( std::is_floating_point_v< T > )
	{
		return static_cast< T >(1e-5);
	}
	else
	{
		return T{0};
	}
}

template< typename T >
bool
nearEqual(T a, T b)
{
	if constexpr ( std::is_floating_point_v< T > )
	{
		return std::abs(a - b) < epsilon< T >();
	}
	else
	{
		return a == b;
	}
}

// ============================================================================
// CONSTRUCTION AND INITIALIZATION TESTS
// ============================================================================

TYPED_TEST(MathVector, Vector2DefaultConstruction)
{
	/* Default constructor should initialize to zero */
	const auto vec = Vector< 2, TypeParam >{};

	ASSERT_EQ(vec[X], TypeParam{0});
	ASSERT_EQ(vec[Y], TypeParam{0});
}

TYPED_TEST(MathVector, Vector3DefaultConstruction)
{
	/* Default constructor should initialize to zero */
	const auto vec = Vector< 3, TypeParam >{};

	ASSERT_EQ(vec[X], TypeParam{0});
	ASSERT_EQ(vec[Y], TypeParam{0});
	ASSERT_EQ(vec[Z], TypeParam{0});
}

TYPED_TEST(MathVector, Vector4DefaultConstruction)
{
	/* Default constructor should initialize to zero */
	const auto vec = Vector< 4, TypeParam >{};

	ASSERT_EQ(vec[X], TypeParam{0});
	ASSERT_EQ(vec[Y], TypeParam{0});
	ASSERT_EQ(vec[Z], TypeParam{0});
	ASSERT_EQ(vec[W], TypeParam{0});
}

TYPED_TEST(MathVector, Vector2ParametricConstruction)
{
	/* Constructor with explicit values */
	const auto vec = Vector< 2, TypeParam >{TypeParam{3}, TypeParam{4}};

	ASSERT_EQ(vec[X], TypeParam{3});
	ASSERT_EQ(vec[Y], TypeParam{4});
}

TYPED_TEST(MathVector, Vector3ParametricConstruction)
{
	/* Constructor with explicit values */
	const auto vec = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};

	ASSERT_EQ(vec[X], TypeParam{1});
	ASSERT_EQ(vec[Y], TypeParam{2});
	ASSERT_EQ(vec[Z], TypeParam{3});
}

TYPED_TEST(MathVector, Vector4ParametricConstruction)
{
	/* Constructor with explicit values */
	const auto vec = Vector< 4, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}, TypeParam{4}};

	ASSERT_EQ(vec[X], TypeParam{1});
	ASSERT_EQ(vec[Y], TypeParam{2});
	ASSERT_EQ(vec[Z], TypeParam{3});
	ASSERT_EQ(vec[W], TypeParam{4});
}

TYPED_TEST(MathVector, Vector2CopyConstruction)
{
	/* Copy constructor should create identical vector */
	const auto original = Vector< 2, TypeParam >{TypeParam{5}, TypeParam{7}};
	const auto copy = original;

	ASSERT_EQ(copy[X], original[X]);
	ASSERT_EQ(copy[Y], original[Y]);
}

TYPED_TEST(MathVector, SwizzleVec3ToVec2)
{
	/* Swizzle: Vec3 -> Vec2 should keep X and Y */
	const auto vec3 = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto vec2 = Vector< 2, TypeParam >{vec3};

	ASSERT_EQ(vec2[X], TypeParam{1});
	ASSERT_EQ(vec2[Y], TypeParam{2});
}

TYPED_TEST(MathVector, SwizzleVec2ToVec3)
{
	/* Swizzle: Vec2 -> Vec3 should extend with provided Z or 0 */
	const auto vec2 = Vector< 2, TypeParam >{TypeParam{1}, TypeParam{2}};
	const auto vec3 = Vector< 3, TypeParam >{vec2, TypeParam{5}};

	ASSERT_EQ(vec3[X], TypeParam{1});
	ASSERT_EQ(vec3[Y], TypeParam{2});
	ASSERT_EQ(vec3[Z], TypeParam{5});
}

TYPED_TEST(MathVector, SwizzleVec3ToVec4)
{
	/* Swizzle: Vec3 -> Vec4 should extend with provided W or 0 */
	const auto vec3 = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto vec4 = Vector< 4, TypeParam >{vec3, TypeParam{1}};

	ASSERT_EQ(vec4[X], TypeParam{1});
	ASSERT_EQ(vec4[Y], TypeParam{2});
	ASSERT_EQ(vec4[Z], TypeParam{3});
	ASSERT_EQ(vec4[W], TypeParam{1});
}

TYPED_TEST(MathVector, ResetVector)
{
	/* reset() should zero all components */
	auto vec = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	vec.reset();

	ASSERT_EQ(vec[X], TypeParam{0});
	ASSERT_EQ(vec[Y], TypeParam{0});
	ASSERT_EQ(vec[Z], TypeParam{0});
}

// ============================================================================
// ARITHMETIC OPERATIONS TESTS
// ============================================================================

TYPED_TEST(MathVector, VectorAddition)
{
	/* Vector + Vector component-wise addition */
	const auto a = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto b = Vector< 3, TypeParam >{TypeParam{4}, TypeParam{5}, TypeParam{6}};
	const auto result = a + b;

	ASSERT_EQ(result[X], TypeParam{5});
	ASSERT_EQ(result[Y], TypeParam{7});
	ASSERT_EQ(result[Z], TypeParam{9});
}

TYPED_TEST(MathVector, VectorSubtraction)
{
	/* Vector - Vector component-wise subtraction */
	const auto a = Vector< 3, TypeParam >{TypeParam{5}, TypeParam{7}, TypeParam{9}};
	const auto b = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto result = a - b;

	ASSERT_EQ(result[X], TypeParam{4});
	ASSERT_EQ(result[Y], TypeParam{5});
	ASSERT_EQ(result[Z], TypeParam{6});
}

TYPED_TEST(MathVector, ScalarMultiplication)
{
	/* Vector * scalar should scale all components */
	const auto vec = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto result = vec * TypeParam{2};

	ASSERT_EQ(result[X], TypeParam{2});
	ASSERT_EQ(result[Y], TypeParam{4});
	ASSERT_EQ(result[Z], TypeParam{6});
}

TYPED_TEST(MathVector, ComponentWiseMultiplication)
{
	/* Vector * Vector component-wise multiplication */
	const auto a = Vector< 3, TypeParam >{TypeParam{2}, TypeParam{3}, TypeParam{4}};
	const auto b = Vector< 3, TypeParam >{TypeParam{5}, TypeParam{6}, TypeParam{7}};
	const auto result = a * b;

	ASSERT_EQ(result[X], TypeParam{10});
	ASSERT_EQ(result[Y], TypeParam{18});
	ASSERT_EQ(result[Z], TypeParam{28});
}

TYPED_TEST(MathVector, ScalarDivision)
{
	/* Vector / scalar should divide all components */
	const auto vec = Vector< 3, TypeParam >{TypeParam{10}, TypeParam{20}, TypeParam{30}};
	const auto result = vec / TypeParam{2};

	ASSERT_EQ(result[X], TypeParam{5});
	ASSERT_EQ(result[Y], TypeParam{10});
	ASSERT_EQ(result[Z], TypeParam{15});
}

TYPED_TEST(MathVector, UnaryPlus)
{
	/* Unary + should return identical vector */
	const auto vec = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto result = +vec;

	ASSERT_EQ(result[X], vec[X]);
	ASSERT_EQ(result[Y], vec[Y]);
	ASSERT_EQ(result[Z], vec[Z]);
}

TYPED_TEST(MathVector, UnaryMinus)
{
	/* Unary - should negate all components */
	const auto vec = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto result = -vec;

	ASSERT_EQ(result[X], TypeParam{-1});
	ASSERT_EQ(result[Y], TypeParam{-2});
	ASSERT_EQ(result[Z], TypeParam{-3});
}

TYPED_TEST(MathVector, CompoundAddition)
{
	/* += operator should modify in place */
	auto vec = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto other = Vector< 3, TypeParam >{TypeParam{4}, TypeParam{5}, TypeParam{6}};
	vec += other;

	ASSERT_EQ(vec[X], TypeParam{5});
	ASSERT_EQ(vec[Y], TypeParam{7});
	ASSERT_EQ(vec[Z], TypeParam{9});
}

TYPED_TEST(MathVector, CompoundSubtraction)
{
	/* -= operator should modify in place */
	auto vec = Vector< 3, TypeParam >{TypeParam{5}, TypeParam{7}, TypeParam{9}};
	const auto other = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	vec -= other;

	ASSERT_EQ(vec[X], TypeParam{4});
	ASSERT_EQ(vec[Y], TypeParam{5});
	ASSERT_EQ(vec[Z], TypeParam{6});
}

TYPED_TEST(MathVector, CompoundScalarMultiplication)
{
	/* *= operator with scalar should modify in place */
	auto vec = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	vec *= TypeParam{3};

	ASSERT_EQ(vec[X], TypeParam{3});
	ASSERT_EQ(vec[Y], TypeParam{6});
	ASSERT_EQ(vec[Z], TypeParam{9});
}

TYPED_TEST(MathVector, CompoundDivision)
{
	/* /= operator should modify in place */
	auto vec = Vector< 3, TypeParam >{TypeParam{10}, TypeParam{20}, TypeParam{30}};
	vec /= TypeParam{2};

	ASSERT_EQ(vec[X], TypeParam{5});
	ASSERT_EQ(vec[Y], TypeParam{10});
	ASSERT_EQ(vec[Z], TypeParam{15});
}

TYPED_TEST(MathVector, ScalarDivisionByZero)
{
	/* volatile: a constant zero divisor is folded at compile time and rejected by MSVC (C4723). */
	const volatile TypeParam zero = 0;
	const auto vec = Vector< 3, TypeParam >{TypeParam{10}, TypeParam{0}, TypeParam{-30}};
	const auto result = vec / zero;
	auto compound = vec;
	compound /= zero;

	if constexpr ( std::is_integral_v< TypeParam > )
	{
		ASSERT_EQ(result, (Vector< 3, TypeParam >{}));
		ASSERT_EQ(compound, vec);
	}
	else
	{
		ASSERT_TRUE(std::isinf(result[X]) && result[X] > 0);
		ASSERT_TRUE(std::isnan(result[Y]));
		ASSERT_TRUE(std::isinf(result[Z]) && result[Z] < 0);
		ASSERT_TRUE(std::isinf(compound[X]) && compound[X] > 0);
		ASSERT_TRUE(std::isnan(compound[Y]));
		ASSERT_TRUE(std::isinf(compound[Z]) && compound[Z] < 0);
	}
}

TYPED_TEST(MathVector, ScalarDivisionBySubEpsilonDivisor)
{
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		/* Half the type epsilon: a power of two, so a valid and exactly representable divisor. */
		const auto divisor = std::numeric_limits< TypeParam >::epsilon() / 2;
		const auto vec = Vector< 3, TypeParam >{divisor, divisor * 2, divisor * -4};
		const auto result = vec / divisor;
		auto compound = vec;
		compound /= divisor;

		ASSERT_EQ(result[X], TypeParam{1});
		ASSERT_EQ(result[Y], TypeParam{2});
		ASSERT_EQ(result[Z], TypeParam{-4});
		ASSERT_EQ(compound, result);
	}
}

// ============================================================================
// COMPARISON OPERATIONS TESTS
// ============================================================================

TYPED_TEST(MathVector, EqualityComparison)
{
	/* Two vectors with same values should be equal */
	const auto a = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto b = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};

	ASSERT_TRUE(a == b);
}

TYPED_TEST(MathVector, InequalityComparison)
{
	/* Two vectors with different values should be inequal */
	const auto a = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto b = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{4}};

	ASSERT_TRUE(a != b);
}

TYPED_TEST(MathVector, IndexAccessor)
{
	/* [] operator should provide read/write access to components */
	auto vec = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};

	ASSERT_EQ(vec[0], TypeParam{1});
	ASSERT_EQ(vec[1], TypeParam{2});
	ASSERT_EQ(vec[2], TypeParam{3});

	vec[1] = TypeParam{10};
	ASSERT_EQ(vec[1], TypeParam{10});
}

// ============================================================================
// MATHEMATICAL OPERATIONS TESTS - LENGTH AND NORMALIZATION
// ============================================================================

TYPED_TEST(MathVector, LengthSquared3_4_5Triangle)
{
	/* Classic 3-4-5 right triangle: length² should be 25 */
	const auto vec = Vector< 2, TypeParam >{TypeParam{3}, TypeParam{4}};
	const auto lenSq = vec.lengthSquared();

	ASSERT_EQ(lenSq, TypeParam{25});
}

TYPED_TEST(MathVector, Length3_4_5Triangle)
{
	/* Classic 3-4-5 right triangle: length should be 5 */
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		const auto vec = Vector< 2, TypeParam >{TypeParam{3}, TypeParam{4}};
		const auto len = vec.length();

		ASSERT_TRUE(nearEqual(len, TypeParam{5}));
	}
}

TYPED_TEST(MathVector, UnitVectorLength)
{
	/* Unit vectors should have length 1 */
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		const auto unitX = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{0}, TypeParam{0}};
		const auto unitY = Vector< 3, TypeParam >{TypeParam{0}, TypeParam{1}, TypeParam{0}};
		const auto unitZ = Vector< 3, TypeParam >{TypeParam{0}, TypeParam{0}, TypeParam{1}};

		ASSERT_TRUE(nearEqual(unitX.length(), TypeParam{1}));
		ASSERT_TRUE(nearEqual(unitY.length(), TypeParam{1}));
		ASSERT_TRUE(nearEqual(unitZ.length(), TypeParam{1}));
	}
}

TYPED_TEST(MathVector, NormalizeVector)
{
	/* Normalization should produce unit vector */
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		auto vec = Vector< 3, TypeParam >{TypeParam{3}, TypeParam{4}, TypeParam{0}};
		vec.normalize();

		const auto len = vec.length();
		ASSERT_TRUE(nearEqual(len, TypeParam{1}));

		/* Direction should be preserved */
		ASSERT_TRUE(nearEqual(vec[X], static_cast< TypeParam >(0.6)));  // 3/5
		ASSERT_TRUE(nearEqual(vec[Y], static_cast< TypeParam >(0.8)));  // 4/5
		ASSERT_TRUE(nearEqual(vec[Z], TypeParam{0}));
	}
}

TYPED_TEST(MathVector, NormalizedVector)
{
	/* normalized() should return unit vector without modifying original */
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		const auto vec = Vector< 3, TypeParam >{TypeParam{3}, TypeParam{4}, TypeParam{0}};
		const auto normalized = vec.normalized();

		/* Original should be unchanged */
		ASSERT_EQ(vec[X], TypeParam{3});
		ASSERT_EQ(vec[Y], TypeParam{4});

		/* Result should be unit length */
		const auto len = normalized.length();
		ASSERT_TRUE(nearEqual(len, TypeParam{1}));
	}
}

/* 2026-10-09 (base item vector-normalized-absolute-epsilon): an ABSOLUTE epsilon on the squared length refused every
 * vector shorter than ~3.5e-4 (float) — normalized() answered zero, normalize() left it unchanged. The normal of a small
 * triangle was zero, and the mesh decimator's quadrics with it. Any non-zero finite vector normalizes now, whatever its
 * scale (a squared length that underflows or overflows included); only a zero or non-finite vector is refused. */
TYPED_TEST(MathVector, NormalizeAnyScale)
{
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		const auto check = [] (TypeParam scale) {
			const Vector< 3, TypeParam > source{TypeParam{3} * scale, TypeParam{4} * scale, TypeParam{0}};
			const auto unit = source.normalized();

			EXPECT_TRUE(nearEqual(unit[X], static_cast< TypeParam >(0.6))) << "scale " << scale << " x " << unit[X];
			EXPECT_TRUE(nearEqual(unit[Y], static_cast< TypeParam >(0.8))) << "scale " << scale << " y " << unit[Y];
			EXPECT_EQ(unit[Z], TypeParam{0}) << "scale " << scale;

			auto inPlace = source;
			inPlace.normalize();

			EXPECT_TRUE(nearEqual(inPlace[X], static_cast< TypeParam >(0.6))) << "scale " << scale;
			EXPECT_TRUE(nearEqual(inPlace[Y], static_cast< TypeParam >(0.8))) << "scale " << scale;
		};

		/* Short (the former threshold), tiny (the squared length underflows), huge (it overflows). */
		check(static_cast< TypeParam >(1e-5));
		check(static_cast< TypeParam >(1e-25));
		check(std::numeric_limits< TypeParam >::denorm_min() * TypeParam{8});
		check(static_cast< TypeParam >(1e30));
		check(std::numeric_limits< TypeParam >::max() / TypeParam{8});

		/* The unit normal of a triangle of area 1e-6 (its cross product is 2e-6 long). */
		const Vector< 3, TypeParam > p0{TypeParam{0}, TypeParam{0}, TypeParam{0}};
		const Vector< 3, TypeParam > p1{static_cast< TypeParam >(2e-3), TypeParam{0}, TypeParam{0}};
		const Vector< 3, TypeParam > p2{TypeParam{0}, static_cast< TypeParam >(1e-3), TypeParam{0}};
		const auto normal = Vector< 3, TypeParam >::crossProduct(p1 - p0, p2 - p0).normalized();

		EXPECT_TRUE(nearEqual(normal[Z], TypeParam{1})) << normal[Z];
	}
}

/* A zero or non-finite vector is refused: normalized() answers the zero vector, normalize() leaves it unchanged. */
TYPED_TEST(MathVector, NormalizeRefusesZeroAndNonFinite)
{
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		const Vector< 3, TypeParam > zero{};

		EXPECT_TRUE(zero.normalized().isZero());

		const Vector< 3, TypeParam > notANumber{std::numeric_limits< TypeParam >::quiet_NaN(), TypeParam{1}, TypeParam{0}};
		const Vector< 3, TypeParam > infinite{std::numeric_limits< TypeParam >::infinity(), TypeParam{1}, TypeParam{0}};

		EXPECT_TRUE(notANumber.normalized().isZero());
		EXPECT_TRUE(infinite.normalized().isZero());

		auto unchanged = infinite;
		unchanged.normalize();

		EXPECT_EQ(unchanged[X], std::numeric_limits< TypeParam >::infinity());
		EXPECT_EQ(unchanged[Y], TypeParam{1});
	}
}

// ============================================================================
// DOT PRODUCT AND CROSS PRODUCT TESTS
// ============================================================================

TYPED_TEST(MathVector, DotProductOrthogonal)
{
	/* Dot product of orthogonal vectors should be 0 */
	const auto vecX = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{0}, TypeParam{0}};
	const auto vecY = Vector< 3, TypeParam >{TypeParam{0}, TypeParam{1}, TypeParam{0}};

	const auto dot = Vector< 3, TypeParam >::dotProduct(vecX, vecY);
	ASSERT_EQ(dot, TypeParam{0});
}

TYPED_TEST(MathVector, DotProductParallel)
{
	/* Dot product of parallel unit vectors should be 1 */
	const auto a = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{0}, TypeParam{0}};
	const auto b = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{0}, TypeParam{0}};

	const auto dot = Vector< 3, TypeParam >::dotProduct(a, b);
	ASSERT_EQ(dot, TypeParam{1});
}

TYPED_TEST(MathVector, DotProductAntiparallel)
{
	/* Dot product of antiparallel unit vectors should be -1 */
	const auto a = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{0}, TypeParam{0}};
	const auto b = Vector< 3, TypeParam >{TypeParam{-1}, TypeParam{0}, TypeParam{0}};

	const auto dot = Vector< 3, TypeParam >::dotProduct(a, b);
	ASSERT_EQ(dot, TypeParam{-1});
}

TYPED_TEST(MathVector, DotProductCommutative)
{
	/* Dot product should be commutative: a·b = b·a */
	const auto a = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto b = Vector< 3, TypeParam >{TypeParam{4}, TypeParam{5}, TypeParam{6}};

	const auto dot1 = Vector< 3, TypeParam >::dotProduct(a, b);
	const auto dot2 = Vector< 3, TypeParam >::dotProduct(b, a);

	ASSERT_EQ(dot1, dot2);
}

TYPED_TEST(MathVector, CrossProduct3DBasicAxes)
{
	/* Cross product of basis vectors should follow right-hand rule */
	const auto vecX = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{0}, TypeParam{0}};
	const auto vecY = Vector< 3, TypeParam >{TypeParam{0}, TypeParam{1}, TypeParam{0}};
	const auto vecZ = Vector< 3, TypeParam >{TypeParam{0}, TypeParam{0}, TypeParam{1}};

	const auto crossXY = Vector< 3, TypeParam >::crossProduct(vecX, vecY);
	const auto crossYZ = Vector< 3, TypeParam >::crossProduct(vecY, vecZ);
	const auto crossZX = Vector< 3, TypeParam >::crossProduct(vecZ, vecX);

	/* X × Y = Z */
	ASSERT_EQ(crossXY[X], vecZ[X]);
	ASSERT_EQ(crossXY[Y], vecZ[Y]);
	ASSERT_EQ(crossXY[Z], vecZ[Z]);

	/* Y × Z = X */
	ASSERT_EQ(crossYZ[X], vecX[X]);
	ASSERT_EQ(crossYZ[Y], vecX[Y]);
	ASSERT_EQ(crossYZ[Z], vecX[Z]);

	/* Z × X = Y */
	ASSERT_EQ(crossZX[X], vecY[X]);
	ASSERT_EQ(crossZX[Y], vecY[Y]);
	ASSERT_EQ(crossZX[Z], vecY[Z]);
}

TYPED_TEST(MathVector, CrossProductAnticommutative)
{
	/* Cross product should be anticommutative: a×b = -(b×a) */
	const auto a = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto b = Vector< 3, TypeParam >{TypeParam{4}, TypeParam{5}, TypeParam{6}};

	const auto cross1 = Vector< 3, TypeParam >::crossProduct(a, b);
	const auto cross2 = Vector< 3, TypeParam >::crossProduct(b, a);

	ASSERT_EQ(cross1[X], -cross2[X]);
	ASSERT_EQ(cross1[Y], -cross2[Y]);
	ASSERT_EQ(cross1[Z], -cross2[Z]);
}

TYPED_TEST(MathVector, CrossProductOrthogonal)
{
	/* Cross product result should be orthogonal to both input vectors */
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		const auto a = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
		const auto b = Vector< 3, TypeParam >{TypeParam{4}, TypeParam{5}, TypeParam{6}};

		const auto cross = Vector< 3, TypeParam >::crossProduct(a, b);

		/* cross · a = 0 and cross · b = 0 */
		const auto dotA = Vector< 3, TypeParam >::dotProduct(cross, a);
		const auto dotB = Vector< 3, TypeParam >::dotProduct(cross, b);

		ASSERT_TRUE(nearEqual(dotA, TypeParam{0}));
		ASSERT_TRUE(nearEqual(dotB, TypeParam{0}));
	}
}

TYPED_TEST(MathVector, CrossProductParallelVectorsAreZero)
{
	/* Cross product of parallel vectors should be zero */
	const auto a = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto b = Vector< 3, TypeParam >{TypeParam{2}, TypeParam{4}, TypeParam{6}};  // 2*a

	const auto cross = Vector< 3, TypeParam >::crossProduct(a, b);

	ASSERT_EQ(cross[X], TypeParam{0});
	ASSERT_EQ(cross[Y], TypeParam{0});
	ASSERT_EQ(cross[Z], TypeParam{0});
}

// ============================================================================
// DISTANCE TESTS
// ============================================================================

TYPED_TEST(MathVector, DistanceBetweenPoints)
{
	/* Distance between (0,0,0) and (3,4,0) should be 5 */
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		const auto a = Vector< 3, TypeParam >{TypeParam{0}, TypeParam{0}, TypeParam{0}};
		const auto b = Vector< 3, TypeParam >{TypeParam{3}, TypeParam{4}, TypeParam{0}};

		const auto dist = Vector< 3, TypeParam >::distance(a, b);
		ASSERT_TRUE(nearEqual(dist, TypeParam{5}));
	}
}

TYPED_TEST(MathVector, DistanceSquared)
{
	/* Distance² is cheaper to compute and useful for comparisons */
	const auto a = Vector< 3, TypeParam >{TypeParam{0}, TypeParam{0}, TypeParam{0}};
	const auto b = Vector< 3, TypeParam >{TypeParam{3}, TypeParam{4}, TypeParam{0}};

	const auto distSq = Vector< 3, TypeParam >::distanceSquared(a, b);
	ASSERT_EQ(distSq, TypeParam{25});
}

TYPED_TEST(MathVector, DistanceToPoint)
{
	/* distanceToPoint is convenience method for distance */
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		const auto origin = Vector< 3, TypeParam >{TypeParam{0}, TypeParam{0}, TypeParam{0}};
		const auto point = Vector< 3, TypeParam >{TypeParam{3}, TypeParam{4}, TypeParam{0}};

		const auto dist = origin.distanceToPoint(point);
		ASSERT_TRUE(nearEqual(dist, TypeParam{5}));
	}
}

TYPED_TEST(MathVector, SamePointHasZeroDistance)
{
	/* Distance from point to itself should be 0 */
	const auto point = Vector< 3, TypeParam >{TypeParam{5}, TypeParam{7}, TypeParam{9}};

	const auto dist = point.distanceToPoint(point);
	ASSERT_EQ(dist, TypeParam{0});
}

// ============================================================================
// EDGE CASES AND ROBUSTNESS TESTS
// ============================================================================

TYPED_TEST(MathVector, ZeroVectorOperations)
{
	/* Operations on zero vector should behave correctly */
	const auto zero = Vector< 3, TypeParam >{};

	/* Zero + anything = anything */
	const auto vec = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto result = zero + vec;

	ASSERT_EQ(result[X], TypeParam{1});
	ASSERT_EQ(result[Y], TypeParam{2});
	ASSERT_EQ(result[Z], TypeParam{3});

	/* Zero length */
	ASSERT_EQ(zero.lengthSquared(), TypeParam{0});
}

TYPED_TEST(MathVector, ScalarAdditionWithAllComponents)
{
	/* Scalar addition should affect all components */
	const auto vec = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto result = vec + TypeParam{5};

	ASSERT_EQ(result[X], TypeParam{6});
	ASSERT_EQ(result[Y], TypeParam{7});
	ASSERT_EQ(result[Z], TypeParam{8});
}

TYPED_TEST(MathVector, NegativeValues)
{
	/* Vector should handle negative values correctly */
	const auto vec = Vector< 3, TypeParam >{TypeParam{-1}, TypeParam{-2}, TypeParam{-3}};

	ASSERT_EQ(vec[X], TypeParam{-1});
	ASSERT_EQ(vec[Y], TypeParam{-2});
	ASSERT_EQ(vec[Z], TypeParam{-3});

	const auto negated = -vec;
	ASSERT_EQ(negated[X], TypeParam{1});
	ASSERT_EQ(negated[Y], TypeParam{2});
	ASSERT_EQ(negated[Z], TypeParam{3});
}

TYPED_TEST(MathVector, MultiplicationByZero)
{
	/* Multiplying by zero should produce zero vector */
	const auto vec = Vector< 3, TypeParam >{TypeParam{5}, TypeParam{7}, TypeParam{9}};
	const auto result = vec * TypeParam{0};

	ASSERT_EQ(result[X], TypeParam{0});
	ASSERT_EQ(result[Y], TypeParam{0});
	ASSERT_EQ(result[Z], TypeParam{0});
}

TYPED_TEST(MathVector, ChainedOperations)
{
	/* Complex expression should evaluate correctly */
	const auto a = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};
	const auto b = Vector< 3, TypeParam >{TypeParam{4}, TypeParam{5}, TypeParam{6}};
	const auto c = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{1}, TypeParam{1}};

	const auto result = (a + b) * TypeParam{2} - c;

	ASSERT_EQ(result[X], TypeParam{9});   // (1+4)*2 - 1 = 9
	ASSERT_EQ(result[Y], TypeParam{13});  // (2+5)*2 - 1 = 13
	ASSERT_EQ(result[Z], TypeParam{17});  // (3+6)*2 - 1 = 17
}

// ============================================================================
// REAL-WORLD 3D GRAPHICS SCENARIOS
// ============================================================================

TYPED_TEST(MathVector, PositionVsDirection)
{
	/* In 3D graphics: positions have W=1, directions have W=0 */
	const auto position = Vector< 4, TypeParam >{TypeParam{10}, TypeParam{20}, TypeParam{30}, TypeParam{1}};
	const auto direction = Vector< 4, TypeParam >{TypeParam{1}, TypeParam{0}, TypeParam{0}, TypeParam{0}};

	ASSERT_EQ(position[W], TypeParam{1});
	ASSERT_EQ(direction[W], TypeParam{0});
}

TYPED_TEST(MathVector, NormalCalculationFromTriangle)
{
	/* Calculate normal from triangle vertices (common 3D operation) */
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		const auto v0 = Vector< 3, TypeParam >{TypeParam{0}, TypeParam{0}, TypeParam{0}};
		const auto v1 = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{0}, TypeParam{0}};
		const auto v2 = Vector< 3, TypeParam >{TypeParam{0}, TypeParam{1}, TypeParam{0}};

		const auto edge1 = v1 - v0;
		const auto edge2 = v2 - v0;
		const auto normal = Vector< 3, TypeParam >::crossProduct(edge1, edge2).normalized();

		/* Normal should point in +Z direction */
		ASSERT_TRUE(nearEqual(normal[X], TypeParam{0}));
		ASSERT_TRUE(nearEqual(normal[Y], TypeParam{0}));
		ASSERT_TRUE(nearEqual(normal[Z], TypeParam{1}));
	}
}

TYPED_TEST(MathVector, ReflectionVector)
{
	/* Reflect vector across normal (common for lighting/physics) */
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		const auto incident = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{-1}, TypeParam{0}}.normalized();
		const auto normal = Vector< 3, TypeParam >{TypeParam{0}, TypeParam{1}, TypeParam{0}};

		const auto dot = Vector< 3, TypeParam >::dotProduct(incident, normal);
		const auto reflected = incident - normal * (TypeParam{2} * dot);

		/* Reflected ray should point upward */
		ASSERT_TRUE(reflected[Y] > TypeParam{0});
		ASSERT_TRUE(nearEqual(reflected.length(), TypeParam{1}));
	}
}

TYPED_TEST(MathVector, LinearInterpolation)
{
	/* Lerp between two points (animation, camera movement) */
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		const auto start = Vector< 3, TypeParam >{TypeParam{0}, TypeParam{0}, TypeParam{0}};
		const auto end = Vector< 3, TypeParam >{TypeParam{10}, TypeParam{10}, TypeParam{10}};

		/* At t=0.5, should be halfway */
		const auto mid = start + (end - start) * static_cast< TypeParam >(0.5);

		ASSERT_TRUE(nearEqual(mid[X], TypeParam{5}));
		ASSERT_TRUE(nearEqual(mid[Y], TypeParam{5}));
		ASSERT_TRUE(nearEqual(mid[Z], TypeParam{5}));
	}
}

/* 2026-09-26: Vector::linearInterpolation() returned `A * t + B * (1 - t)` — factor 0 gave B. The test above never
 * called it, and at t = 0.5 no test can tell the two directions apart: these check the ENDS. */
TYPED_TEST(MathVector, LinearInterpolationRunsFromTheFirstOperand)
{
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		using V3 = Vector< 3, TypeParam >;

		const V3 start{TypeParam{1}, TypeParam{0}, TypeParam{0}};
		const V3 end{TypeParam{0}, TypeParam{0}, TypeParam{1}};

		ASSERT_EQ(V3::linearInterpolation(start, end, TypeParam{0}), start);
		ASSERT_EQ(V3::linearInterpolation(start, end, TypeParam{1}), end);

		const auto quarter = V3::linearInterpolation(start, end, static_cast< TypeParam >(0.25));

		ASSERT_TRUE(nearEqual(quarter[X], static_cast< TypeParam >(0.75)));
		ASSERT_TRUE(nearEqual(quarter[Z], static_cast< TypeParam >(0.25)));
	}
}

/* The Bezier helpers are built on the LERP: through the reversed one, each returned its LAST point at factor 0, and every
 * BSpline / BezierCurve segment ran from the next point back to the current one. */
TYPED_TEST(MathVector, BezierInterpolationsRunFromTheirFirstPoint)
{
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		using V3 = Vector< 3, TypeParam >;

		const V3 a{TypeParam{0}, TypeParam{0}, TypeParam{0}};
		const V3 b{TypeParam{10}, TypeParam{10}, TypeParam{10}};
		const V3 c{TypeParam{20}, TypeParam{0}, TypeParam{0}};
		const V3 d{TypeParam{30}, TypeParam{10}, TypeParam{0}};

		ASSERT_EQ(V3::quadraticBezierInterpolation(a, b, c, TypeParam{0}), a);
		ASSERT_EQ(V3::quadraticBezierInterpolation(a, b, c, TypeParam{1}), c);
		ASSERT_EQ(V3::cubicBezierInterpolation(a, b, c, d, TypeParam{0}), a);
		ASSERT_EQ(V3::cubicBezierInterpolation(a, b, c, d, TypeParam{1}), d);

		/* The standard quadratic at t = 0.25: A (1-t)² + 2 B t (1-t) + C t² = (5, 3.75, 3.75); run backwards (t = 0.75)
		 * it would read (15, 3.75, 3.75). */
		const auto quarter = V3::quadraticBezierInterpolation(a, b, c, static_cast< TypeParam >(0.25));

		ASSERT_TRUE(nearEqual(quarter[X], static_cast< TypeParam >(5)));
		ASSERT_TRUE(nearEqual(quarter[Y], static_cast< TypeParam >(3.75)));
		ASSERT_TRUE(nearEqual(quarter[Z], static_cast< TypeParam >(3.75)));
	}
}

TYPED_TEST(MathVector, RotateTowardsTurnsAtMostTheGivenAngle)
{
	if constexpr ( std::is_floating_point_v< TypeParam > )
	{
		using V3 = Vector< 3, TypeParam >;

		const auto angleBetween = [] (const V3 & lhs, const V3 & rhs) {
			return std::acos(std::clamp(V3::dotProduct(lhs.normalized(), rhs.normalized()), TypeParam{-1}, TypeParam{1}));
		};

		const V3 north{TypeParam{0}, TypeParam{0}, TypeParam{-1}};
		const V3 east{TypeParam{1}, TypeParam{0}, TypeParam{0}};
		const auto step = Radian(static_cast< TypeParam >(10));

		/* A limited turn: exactly `step` away from where it started, in the plane of the two directions, unit length. */
		const auto turned = V3::rotateTowards(north, east * TypeParam{5}, step);

		ASSERT_TRUE(nearEqual(angleBetween(north, turned), step));
		ASSERT_TRUE(nearEqual(angleBetween(turned, east), Radian(static_cast< TypeParam >(80))));
		ASSERT_TRUE(nearEqual(turned.length(), TypeParam{1}));
		ASSERT_TRUE(nearEqual(turned[Y], TypeParam{0}));

		/* Close enough: the target itself (normalised). */
		ASSERT_EQ(V3::rotateTowards(north, east, Radian(static_cast< TypeParam >(95))), east);

		/* Opposite headings do not stall: they turn around +Y, and stay horizontal. */
		const auto away = V3::rotateTowards(north, -north, step);

		ASSERT_TRUE(nearEqual(angleBetween(north, away), step));
		ASSERT_TRUE(nearEqual(away[Y], TypeParam{0}));

		/* Nearly parallel (below the cross product's resolution): it must not take the opposite-direction fallback and turn
		 * AWAY from the target (review, 2026-09-26). */
		const V3 almost{-std::sin(static_cast< TypeParam >(3.0e-4)), TypeParam{0}, -std::cos(static_cast< TypeParam >(3.0e-4))};

		ASSERT_TRUE(V3::rotateTowards(north, almost, static_cast< TypeParam >(1.0e-4))[X] <= TypeParam{0});

		/* Degenerate inputs. */
		ASSERT_EQ(V3::rotateTowards(V3{}, east, step), east);
		ASSERT_EQ(V3::rotateTowards(north, V3{}, step), north);
		ASSERT_EQ(V3::rotateTowards(north, east, TypeParam{0}), north);
	}
}

// ============================================================================
// NAMED COMPONENT ACCESSORS
// ============================================================================

TYPED_TEST(MathVector, Vector3NamedAccessors)
{
	/* x()/y()/z() accessors should return the matching components */
	const auto vec = Vector< 3, TypeParam >{TypeParam{1}, TypeParam{2}, TypeParam{3}};

	ASSERT_EQ(vec.x(), TypeParam{1});
	ASSERT_EQ(vec.y(), TypeParam{2});
	ASSERT_EQ(vec.z(), TypeParam{3});
}

/* 2026-10-09: Vector::normal() answers EXACTLY zero for a degenerate (flat) triangle — relative to its edges — and the
 * unit normal for a small but valid one; isDegenerateCrossProduct() / areNearlyParallel() are the shared tests. */
TEST(MathVectorDegeneracy, normalOfSmallAndFlatTriangles)
{
	using V3 = Vector< 3, float >;

	for ( const float scale : {1.0F, 1e-3F, 1e-6F} )
	{
		const V3 a{0.0F, 0.0F, 0.0F};
		const V3 b{scale, 0.0F, 0.0F};
		const V3 c{0.0F, scale, 0.0F};

		const auto valid = V3::normal(a, b, c);

		EXPECT_NEAR(valid.length(), 1.0F, 1e-5F) << "scale " << scale;
		EXPECT_NEAR(std::abs(valid[Z]), 1.0F, 1e-5F) << "scale " << scale;

		/* Collinear: no normal at all. */
		const V3 collinear{scale * 2.0F, 0.0F, 0.0F};

		EXPECT_TRUE(V3::normal(a, b, collinear).isZero()) << "scale " << scale;
		EXPECT_TRUE(V3::isDegenerateCrossProduct(V3::crossProduct(b - a, collinear - a), b - a, collinear - a)) << "scale " << scale;
		EXPECT_FALSE(V3::isDegenerateCrossProduct(V3::crossProduct(b - a, c - a), b - a, c - a)) << "scale " << scale;
	}

	/* Coincident points. */
	EXPECT_TRUE(V3::normal(V3{1.0F, 2.0F, 3.0F}, V3{1.0F, 2.0F, 3.0F}, V3{4.0F, 5.0F, 6.0F}).isZero());

	using V2 = Vector< 2, float >;

	EXPECT_TRUE(V2::areNearlyParallel(V2{1e-5F, 0.0F}, V2{3e-5F, 0.0F}));
	EXPECT_FALSE(V2::areNearlyParallel(V2{1e-5F, 0.0F}, V2{0.0F, 1e-5F}));
	EXPECT_TRUE(V2::areNearlyParallel(V2{0.0F, 0.0F}, V2{1.0F, 0.0F}));
}
