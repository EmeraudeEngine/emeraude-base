/*
 * src/Testing/test_MathRigidBody.cpp
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
#include <cmath>
#include <limits>
#include <numbers>

/* Local inclusions. */
#include "Math/Matrix.hpp"
#include "Math/Quaternion.hpp"
#include "Math/RigidBody.hpp"
#include "Math/Vector.hpp"

using namespace EmEn::Base::Math;

namespace
{
	using Vec3 = Vector< 3, float >;

	constexpr float RigidBodyTolerance{1.0e-4F};

	/** What a refused tensor reads as, after the ASSERT_TRUE that already reported it. */
	const Matrix< 3, float > NoTensor{0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};

	void
	expectDiagonal (const Matrix< 3, float > & matrix, float xx, float yy, float zz)
	{
		EXPECT_NEAR(matrix(0, 0), xx, RigidBodyTolerance);
		EXPECT_NEAR(matrix(1, 1), yy, RigidBodyTolerance);
		EXPECT_NEAR(matrix(2, 2), zz, RigidBodyTolerance);
		EXPECT_NEAR(matrix(0, 1), 0.0F, RigidBodyTolerance);
		EXPECT_NEAR(matrix(0, 2), 0.0F, RigidBodyTolerance);
		EXPECT_NEAR(matrix(1, 2), 0.0F, RigidBodyTolerance);
	}
}

TEST(MathRigidBody, solidBoxInertia)
{
	const auto box = RigidBody::solidBoxInertia(10.0F, Vec3{1.0F, 2.0F, 3.0F});

	ASSERT_TRUE(box.has_value());
	expectDiagonal(box.value_or(NoTensor), 10.0F * 13.0F / 12.0F, 10.0F * 10.0F / 12.0F, 10.0F * 5.0F / 12.0F);

	/* The 2 m, 10 kg cube of collision-debug: m (s² + s²) / 12 = 6.667 on every axis. */
	const auto cube = RigidBody::solidBoxInertia(10.0F, Vec3{2.0F, 2.0F, 2.0F});

	ASSERT_TRUE(cube.has_value());
	expectDiagonal(cube.value_or(NoTensor), 80.0F / 12.0F, 80.0F / 12.0F, 80.0F / 12.0F);
}

TEST(MathRigidBody, solidSphereAndCylinderInertia)
{
	const auto sphere = RigidBody::solidSphereInertia(2.0F, 0.5F);

	ASSERT_TRUE(sphere.has_value());
	expectDiagonal(sphere.value_or(NoTensor), 0.2F, 0.2F, 0.2F);

	const auto cylinder = RigidBody::solidCylinderInertia(3.0F, 0.5F, 2.0F);

	ASSERT_TRUE(cylinder.has_value());
	expectDiagonal(cylinder.value_or(NoTensor), 3.0F * (0.75F + 4.0F) / 12.0F, 3.0F * 0.25F * 0.5F, 3.0F * (0.75F + 4.0F) / 12.0F);
}

TEST(MathRigidBody, solidCapsuleInertiaLimits)
{
	/* No cylinder: a sphere. */
	const auto ball = RigidBody::solidCapsuleInertia(2.0F, 0.5F, 0.0F);
	const auto sphere = RigidBody::solidSphereInertia(2.0F, 0.5F);

	ASSERT_TRUE(ball.has_value() && sphere.has_value());
	expectDiagonal(ball.value_or(NoTensor), sphere.value_or(NoTensor)(0, 0), sphere.value_or(NoTensor)(1, 1), sphere.value_or(NoTensor)(2, 2));

	/* A very thin capsule: a rod, m h² / 12 across, ~0 about its axis. */
	const auto rod = RigidBody::solidCapsuleInertia(1.0F, 1.0e-4F, 2.0F);

	ASSERT_TRUE(rod.has_value());
	EXPECT_NEAR(rod.value_or(NoTensor)(0, 0), 4.0F / 12.0F, 1.0e-3F);
	EXPECT_NEAR(rod.value_or(NoTensor)(1, 1), 0.0F, 1.0e-6F);

	/* Between the two: larger than the cylinder of the same height alone, about every axis. */
	const auto capsule = RigidBody::solidCapsuleInertia(1.0F, 0.3F, 1.0F);

	ASSERT_TRUE(capsule.has_value());
	EXPECT_GT(capsule.value_or(NoTensor)(0, 0), 0.0F);
	EXPECT_NEAR(capsule.value_or(NoTensor)(0, 0), capsule.value_or(NoTensor)(2, 2), RigidBodyTolerance);
}

TEST(MathRigidBody, invalidInputsAreRefused)
{
	EXPECT_FALSE(RigidBody::solidBoxInertia(-1.0F, Vec3{1.0F, 1.0F, 1.0F}).has_value());
	EXPECT_FALSE(RigidBody::solidBoxInertia(1.0F, Vec3{1.0F, -1.0F, 1.0F}).has_value());
	EXPECT_FALSE(RigidBody::solidSphereInertia(std::numeric_limits< float >::quiet_NaN(), 1.0F).has_value());
	EXPECT_FALSE(RigidBody::solidCylinderInertia(1.0F, std::numeric_limits< float >::infinity(), 1.0F).has_value());
	EXPECT_FALSE(RigidBody::solidCapsuleInertia(1.0F, 1.0F, -0.5F).has_value());
	/* Zero is valid: a massless or a point body. */
	EXPECT_TRUE(RigidBody::solidSphereInertia(0.0F, 0.0F).has_value());
}

TEST(MathRigidBody, parallelAxisTheorem)
{
	/* A point mass of 2 kg moved 3 m along Z: +m d² about X and Y, nothing about Z. */
	const Matrix< 3, float > zero{0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
	const auto moved = RigidBody::parallelAxis(zero, 2.0F, Vec3{0.0F, 0.0F, 3.0F});

	expectDiagonal(moved, 18.0F, 18.0F, 0.0F);

	/* An off-axis offset makes products of inertia: -m x y. */
	const auto diagonalOffset = RigidBody::parallelAxis(zero, 1.0F, Vec3{1.0F, 2.0F, 0.0F});

	EXPECT_NEAR(diagonalOffset(0, 1), -2.0F, RigidBodyTolerance);
	EXPECT_NEAR(diagonalOffset(1, 0), -2.0F, RigidBodyTolerance);
}

TEST(MathRigidBody, skewSymmetricIsTheCrossProduct)
{
	const Vec3 a{1.0F, -2.0F, 3.0F};
	const Vec3 b{-4.0F, 5.0F, 0.5F};
	const Vec3 product = RigidBody::skewSymmetric(a) * b;
	const Vec3 cross = Vec3::crossProduct(a, b);

	EXPECT_NEAR(product[X], cross[X], RigidBodyTolerance);
	EXPECT_NEAR(product[Y], cross[Y], RigidBodyTolerance);
	EXPECT_NEAR(product[Z], cross[Z], RigidBodyTolerance);
}

TEST(MathRigidBody, integrationAboutAWorldAxisKeepsTheAxis)
{
	/* A body tilted 30° about X, spun at 2 rad/s about WORLD +Y for 10 000 steps of 1/60 s: its up vector precesses
	 * about +Y, so its Y component stays cos 30° — the bench's BenchSpinner property, which the engine's local-space
	 * rotateFromPhysics() breaks. The angle travelled is exact to float accuracy. */
	Quaternion< float > orientation;

	orientation.setFromScaledAxis(Vec3{std::numbers::pi_v< float > / 6.0F, 0.0F, 0.0F});

	const Vec3 initialUp = orientation.rotatedVector(Vec3{0.0F, 1.0F, 0.0F});
	const Vec3 spin{0.0F, 2.0F, 0.0F};
	constexpr float Step = 1.0F / 60.0F;

	for ( int index = 0; index < 10000; ++index )
	{
		orientation = RigidBody::integrateOrientation(orientation, spin, Step);
	}

	const Vec3 up = orientation.rotatedVector(Vec3{0.0F, 1.0F, 0.0F});

	EXPECT_NEAR(up[Y], initialUp[Y], 1.0e-3F);
	EXPECT_NEAR(up.length(), 1.0F, 1.0e-3F);

	/* The travelled angle: compare with one rotation of the initial up by the same total angle about +Y. */
	Quaternion< float > total;

	total.setFromScaledAxis(spin * (Step * 10000.0F));

	const Vec3 expected = total.rotatedVector(initialUp);

	EXPECT_NEAR(up[X], expected[X], 5.0e-3F);
	EXPECT_NEAR(up[Z], expected[Z], 5.0e-3F);
}

TEST(MathRigidBody, zeroAngularVelocityLeavesTheOrientation)
{
	Quaternion< float > orientation;

	orientation.setFromScaledAxis(Vec3{0.3F, -0.2F, 0.7F});

	const auto after = RigidBody::integrateOrientation(orientation, Vec3{0.0F, 0.0F, 0.0F}, 1.0F / 60.0F);
	const Vec3 probe{0.6F, 0.0F, -0.8F};

	EXPECT_NEAR(after.rotatedVector(probe)[X], orientation.rotatedVector(probe)[X], RigidBodyTolerance);
	EXPECT_NEAR(after.rotatedVector(probe)[Y], orientation.rotatedVector(probe)[Y], RigidBodyTolerance);
	EXPECT_NEAR(after.rotatedVector(probe)[Z], orientation.rotatedVector(probe)[Z], RigidBodyTolerance);
}

TEST(MathRigidBody, aTinyRotationKeepsItsAngle)
{
	/* A rotation of 1e-5 rad (2 rad/s ... a physics step's angle is ~1e-5 rad for a body turning at 6e-4 rad/s): in
	 * float, w = cos(angle / 2) rounds to exactly 1, so the former 2 acos(w) answered 0 — the solver's small rotations
	 * were silently lost while its angular velocity kept growing (a creeping stack, 2026-10-01). */
	Quaternion< float > rotation;

	rotation.setFromScaledAxis(Vec3{0.0F, 0.0F, 1.0e-5F});

	float angle = 0.0F;
	Vec3 axis;

	rotation.toAngleAxis(angle, axis);

	EXPECT_NEAR(angle, 1.0e-5F, 1.0e-7F);
	EXPECT_NEAR(axis[Z], 1.0F, 1.0e-4F);

	/* A large angle is unchanged by the fix. */
	rotation.setFromScaledAxis(Vec3{0.0F, 2.5F, 0.0F});
	rotation.toAngleAxis(angle, axis);

	EXPECT_NEAR(angle, 2.5F, 1.0e-5F);
	EXPECT_NEAR(axis[Y], 1.0F, 1.0e-5F);
}
