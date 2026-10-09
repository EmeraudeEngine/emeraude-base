/*
 * src/Testing/test_VertexFactoryShapeSimplifier.cpp
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

#include <gtest/gtest.h>

/* STL inclusions. */
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stop_token>
#include <thread>
#include <vector>
#include <cmath>

/* Local inclusions. */
#include "VertexFactory/ShapeDecimator.hpp"
#include "VertexFactory/ShapeGenerator.hpp"
#include "VertexFactory/ShapeSimplifier.hpp"

using namespace EmEn::Base;
using namespace EmEn::Base::VertexFactory;

TEST(VertexFactoryShapeSimplifier, RefusesBadInput)
{
	const Shape< float > empty;

	EXPECT_FALSE(simplifyShape(empty).has_value());

	const auto plane = ShapeGenerator::generatePlane< float, uint32_t >(10.0F, 10.0F, 8U, 8U);

	EXPECT_FALSE(simplifyShape(plane, {.targetRatio = 0.0F}).has_value());
	EXPECT_FALSE(simplifyShape(plane, {.targetRatio = 1.5F}).has_value());
	EXPECT_FALSE(simplifyShape(plane, {.targetRatio = std::nanf("")}).has_value());
	EXPECT_FALSE(simplifyShape(plane, {.targetRatio = 0.5F, .targetError = -1.0F}).has_value());
}

TEST(VertexFactoryShapeSimplifier, FlatPlaneCollapsesToFewTriangles)
{
	/* A flat grid carries no information beyond its outline: the quadric pass may remove almost everything. */
	const auto plane = ShapeGenerator::generatePlane< float, uint32_t >(10.0F, 10.0F, 32U, 32U);
	const auto simplified = simplifyShape(plane, {.targetRatio = 0.1F, .targetError = 0.01F});

	ASSERT_TRUE(simplified.has_value());
	EXPECT_LE(simplified->triangles().size(), plane.triangles().size() / 10 + 2);
	EXPECT_GE(simplified->triangles().size(), 2U);

	/* The outline is kept: the same bounding box. */
	EXPECT_NEAR(simplified->boundingBox().width(), plane.boundingBox().width(), 1e-4F);
	EXPECT_NEAR(simplified->boundingBox().depth(), plane.boundingBox().depth(), 1e-4F);
}

TEST(VertexFactoryShapeSimplifier, SphereKeepsItsShape)
{
	const auto sphere = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 64U, 32U);
	const auto simplified = simplifyShape(sphere, {.targetRatio = 0.25F, .targetError = 0.05F});

	ASSERT_TRUE(simplified.has_value());
	EXPECT_LT(simplified->triangles().size(), sphere.triangles().size() / 2);

	/* Every kept vertex is a source vertex: still on the unit sphere. */
	for ( const auto & vertex : simplified->vertices() )
	{
		EXPECT_NEAR(vertex.position().length(), 1.0F, 1e-4F);
	}

	/* No index out of range. */
	for ( const auto & triangle : simplified->triangles() )
	{
		for ( uint32_t corner = 0; corner < 3; ++corner )
		{
			EXPECT_LT(triangle.vertexIndex(corner), simplified->vertices().size());
		}
	}
}

TEST(VertexFactoryShapeSimplifier, GroupsAreKeptOneByOne)
{
	/* Two groups: a multi-material mesh must keep both sub-geometries on every level. */
	auto shape = ShapeGenerator::generatePlane< float, uint32_t >(10.0F, 10.0F, 16U, 16U);
	const auto triangleCount = static_cast< uint32_t >(shape.triangles().size());
	const auto half = triangleCount / 2;

	shape.groups().clear();
	shape.groups().emplace_back(0U, half);
	shape.groups().emplace_back(half, triangleCount - half);

	const auto simplified = simplifyShape(shape, {.targetRatio = 0.2F, .targetError = 0.01F});

	ASSERT_TRUE(simplified.has_value());
	ASSERT_EQ(simplified->groups().size(), 2U);
	EXPECT_EQ(simplified->groups()[0].first, 0U);
	EXPECT_GT(simplified->groups()[0].second, 0U);
	EXPECT_EQ(simplified->groups()[1].first, simplified->groups()[0].second);
	EXPECT_EQ(simplified->groups()[0].second + simplified->groups()[1].second, simplified->triangles().size());
}

TEST(VertexFactoryShapeSimplifier, EmptyGroupStaysEmpty)
{
	/* JungleRuins' trees carry an empty group: it must survive as an empty group, never refuse the level. */
	auto shape = ShapeGenerator::generatePlane< float, uint32_t >(10.0F, 10.0F, 16U, 16U);
	const auto triangleCount = static_cast< uint32_t >(shape.triangles().size());

	shape.groups().clear();
	shape.groups().emplace_back(0U, 0U);
	shape.groups().emplace_back(0U, triangleCount);

	const auto simplified = simplifyShape(shape, {.targetRatio = 0.2F, .targetError = 0.01F});

	ASSERT_TRUE(simplified.has_value());
	ASSERT_EQ(simplified->groups().size(), 2U);
	EXPECT_EQ(simplified->groups()[0].second, 0U);
	EXPECT_EQ(simplified->groups()[1].first, 0U);
	EXPECT_EQ(simplified->groups()[1].second, simplified->triangles().size());
	EXPECT_LT(simplified->triangles().size(), triangleCount);
}

/*
 * The decimator rebuilds its output vertices with saveVertex() (handedness +1): until 2026-10-06 it copied only the
 * 3D tangent, so every LOD level of a mirrored UV island (handedness -1) lit its normal map backwards.
 * Since 2026-10-07 the computed frame derives the handedness from the UV winding, and the decimator recomputes the
 * frame of its output: the island must be GENUINELY mirrored (its U reversed), not a sphere tagged -1 by hand.
 */
TEST(VertexFactoryShapeDecimator, aRaisedCancellationFlagStopsTheDecimation)
{
	/* The engine raises its shutdown flag: a decimation of a large mesh must not hold the exit for seconds. */
	const auto sphere = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 32, 16);
	const std::atomic_bool cancelled{true};

	ShapeDecimator< float, uint32_t > decimator{sphere, 0.5F};
	decimator.setCancellationFlag(&cancelled);

	EXPECT_TRUE(decimator.isCancelled());
	EXPECT_TRUE(decimator.decimate().triangles().empty());
}

/* Ave Robustus II, decision D1 (2026-10-08): the stop token is the decimator's cancellation. */
TEST(VertexFactoryShapeDecimator, aRequestedStopTokenStopsTheDecimation)
{
	const auto sphere = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 32, 16);
	std::stop_source stopSource;

	stopSource.request_stop();

	ShapeDecimator< float, uint32_t > decimator{sphere, 0.5F};
	decimator.setStopToken(stopSource.get_token());

	EXPECT_TRUE(decimator.isCancelled());
	EXPECT_TRUE(decimator.decimate().triangles().empty());
}

/* Stop latency on a large mesh: a stop requested at any moment of the decimation must end it within the bound
 * (decision D7 proposes 50 ms). Measurement, not a gate — run it on demand:
 *   EmeraudeBaseUnitTests --gtest_also_run_disabled_tests --gtest_filter='*DISABLED_StopLatency*' */
TEST(VertexFactoryShapeDecimator, DISABLED_StopLatencyOnALargeMesh)
{
	/* ~2.2 M triangles, the order of the IvySim_Leaves mesh that stalled the Windows shutdown for 10-20 s. */
	const auto sphere = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 1400, 800);

	std::cout << "[latency] source triangles: " << sphere.triangles().size() << "\n";

	const auto decimateWithStopAfter = [&sphere] (std::chrono::milliseconds delay) {
		std::stop_source stopSource;
		ShapeDecimator< float, uint32_t > decimator{sphere, 0.25F};
		decimator.setStopToken(stopSource.get_token());

		std::atomic_bool returned{false};
		std::chrono::steady_clock::time_point returnedAt;

		std::thread worker{[&decimator, &returned, &returnedAt] () {
			static_cast< void >(decimator.decimate());
			returnedAt = std::chrono::steady_clock::now();
			returned = true;
		}};

		std::this_thread::sleep_for(delay);

		const auto requestedAt = std::chrono::steady_clock::now();
		stopSource.request_stop();
		worker.join();

		const bool finishedBefore = returnedAt < requestedAt;

		return std::make_pair(finishedBefore, std::chrono::duration_cast< std::chrono::microseconds >(returnedAt - requestedAt).count());
	};

	int64_t worstMicroseconds = 0;

	for ( const int delayMs : {0, 25, 50, 100, 200, 400, 700, 1000, 1500, 2000, 3000, 4000, 6000, 8000, 12000} )
	{
		const auto [finishedBefore, latency] = decimateWithStopAfter(std::chrono::milliseconds{delayMs});

		std::cout << "[latency] stop at " << delayMs << " ms: " << (finishedBefore ? "finished before the stop" : std::to_string(latency) + " us") << "\n";

		if ( !finishedBefore )
		{
			worstMicroseconds = std::max(worstMicroseconds, latency);
		}
		else
		{
			break;
		}
	}

	std::cout << "[latency] worst: " << worstMicroseconds << " us\n";

	EXPECT_LT(worstMicroseconds, 50000);
}

/* One full decimation of the large mesh, for a profiler (perf record): which stage holds the time. On demand only. */
TEST(VertexFactoryShapeDecimator, DISABLED_ProfileALargeDecimation)
{
	const auto sphere = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 1400, 800);
	const ShapeDecimator< float, uint32_t > decimator{sphere, 0.25F};

	EXPECT_FALSE(decimator.decimate().triangles().empty());
}

TEST(VertexFactoryShapeDecimator, aLoweredCancellationFlagChangesNothing)
{
	const auto sphere = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 32, 16);
	const std::atomic_bool cancelled{false};

	const ShapeDecimator< float, uint32_t > reference{sphere, 0.5F};
	ShapeDecimator< float, uint32_t > watched{sphere, 0.5F};
	watched.setCancellationFlag(&cancelled);

	const auto expected = reference.decimate();
	const auto actual = watched.decimate();

	EXPECT_FALSE(watched.isCancelled());
	ASSERT_GT(expected.triangles().size(), 0U);
	EXPECT_EQ(actual.triangles().size(), expected.triangles().size());
	EXPECT_EQ(actual.vertices().size(), expected.vertices().size());
}

TEST(VertexFactoryShapeDecimator, decimatedVerticesKeepTheTangentHandedness)
{
	auto sphere = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 32, 16, ShapeBuilderOptions< float >{true, true, false, false, false});

	for ( auto & vertex : sphere.vertices() )
	{
		auto textureCoordinates = vertex.textureCoordinates();
		textureCoordinates[EmEn::Base::Math::X] = 1.0F - textureCoordinates[EmEn::Base::Math::X];

		vertex.setTextureCoordinates(textureCoordinates);
	}

	ASSERT_TRUE(sphere.computeTriangleTangent());
	ASSERT_TRUE(sphere.computeVertexTangent());

	for ( const auto & vertex : sphere.vertices() )
	{
		ASSERT_EQ(vertex.tangentHandedness(), -1.0F) << "the source island must be mirrored";
	}

	const ShapeDecimator< float, uint32_t > decimator{sphere, 0.5F};
	const auto decimated = decimator.decimate();

	ASSERT_GT(decimated.vertices().size(), 0U);
	ASSERT_LT(decimated.triangles().size(), sphere.triangles().size());

	/* Every vertex: the decimator no longer folds UVs over (base item decimator-uv-fold-overs, 2026-10-07 — until then
	 * a vertex next to a fold-over could side with it). */
	size_t lost = 0;

	for ( const auto & vertex : decimated.vertices() )
	{
		if ( vertex.tangentHandedness() != -1.0F )
		{
			++lost;
		}
	}

	EXPECT_EQ(lost, 0U) << lost << " of " << decimated.vertices().size() << " vertices lost their handedness in the decimation.";
}

/*
 * A decimated LOD must not fold its UVs over: a triangle whose UV winding is reversed maps its texture mirrored and
 * turns its tangent frame around (base item decimator-uv-fold-overs, 2026-10-07: 27 of 448 triangles on a 50 % sphere).
 * The source sphere has no mirrored island, so every output triangle must keep the source's winding.
 */
TEST(VertexFactoryShapeDecimator, decimationNeverFoldsTheUVsOver)
{
	const auto sphere = ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 32, 16, ShapeBuilderOptions< float >{true, true, false, false, false});

	for ( const auto ratio : {0.95F, 0.75F, 0.5F, 0.25F} )
	{
		const ShapeDecimator< float, uint32_t > decimator{sphere, ratio};
		const auto decimated = decimator.decimate();

		ASSERT_GT(decimated.triangles().size(), 0U);

		size_t folded = 0;

		for ( const auto & triangle : decimated.triangles() )
		{
			if ( triangle.surfaceTangentHandedness() < 0.0F )
			{
				++folded;
			}
		}

		EXPECT_EQ(folded, 0U) << folded << " of " << decimated.triangles().size() << " triangles folded over at a ratio of " << ratio;
	}
}

namespace
{
	/** @brief FNV-1a over bytes (a fingerprint, not a security hash). */
	[[nodiscard]]
	uint64_t
	fingerprintBytes (uint64_t hash, const void * data, size_t size) noexcept
	{
		const auto * bytes = static_cast< const unsigned char * >(data);

		for ( size_t index = 0; index < size; ++index )
		{
			hash ^= bytes[index];
			hash *= UINT64_C(1099511628211);
		}

		return hash;
	}

	/** @brief A fingerprint of a decimated shape: positions, normals, UVs, tangents, corners and groups, bit for bit. */
	[[nodiscard]]
	uint64_t
	fingerprintShape (const Shape< float, uint32_t > & shape) noexcept
	{
		uint64_t hash = UINT64_C(1469598103934665603);

		for ( const auto & vertex : shape.vertices() )
		{
			hash = fingerprintBytes(hash, vertex.position().data(), 12);
			hash = fingerprintBytes(hash, vertex.normal().data(), 12);
			hash = fingerprintBytes(hash, vertex.textureCoordinates().data(), 12);
			hash = fingerprintBytes(hash, vertex.tangent().data(), 12);
		}

		for ( const auto & triangle : shape.triangles() )
		{
			for ( uint32_t corner = 0; corner < 3; ++corner )
			{
				const auto index = triangle.vertexIndex(corner);

				hash = fingerprintBytes(hash, &index, sizeof(index));
			}
		}

		for ( const auto & group : shape.groups() )
		{
			hash = fingerprintBytes(hash, &group, sizeof(group));
		}

		return hash;
	}
}

/* The decimator's golden fingerprints (D7 rewrite, 2026-10-09): a refactor that must not change the output keeps every
 * line identical; comparing the lines of the three OS tells whether the decimation is the same everywhere. Measurement,
 * not a gate (the values are platform results) — run it on demand:
 *   EmeraudeBaseUnitTests --gtest_also_run_disabled_tests --gtest_filter='*DISABLED_PrintGoldenFingerprints' */
TEST(VertexFactoryShapeDecimator, DISABLED_PrintGoldenFingerprints)
{
	struct GoldenCase
	{
		const char * name;
		Shape< float, uint32_t > shape;
		float ratio;
		uint32_t normalMapResolution;
	};

	std::vector< GoldenCase > cases;
	cases.push_back({"sphere48", ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 48, 24), 0.5F, 0});
	cases.push_back({"sphere300", ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 300, 150), 0.25F, 0});
	cases.push_back({"torus", ShapeGenerator::generateTorus< float, uint32_t >(1.0F, 0.4F, 64, 32), 0.3F, 0});
	cases.push_back({"geodesic", ShapeGenerator::generateGeodesicSphere< float, uint32_t >(1.0F, 4), 0.4F, 0});
	cases.push_back({"hollowedCube", ShapeGenerator::generateHollowedCube< float, uint32_t >(1.0F, 0.2F), 0.5F, 0});
	cases.push_back({"capsule", ShapeGenerator::generateCapsule< float, uint32_t >(1.0F, 2.0F, 32, 16), 0.3F, 0});
	cases.push_back({"cylinder", ShapeGenerator::generateCylinder< float, uint32_t >(1.0F, 0.5F, 2.0F, 40, 10, CapUVMapping::PerSegment), 0.5F, 0});
	cases.push_back({"sphere48-normalmap", ShapeGenerator::generateSphere< float, uint32_t >(1.0F, 48, 24), 0.3F, 64});

	for ( const auto & golden : cases )
	{
		const ShapeDecimator< float, uint32_t > decimator{golden.shape, golden.ratio, 1000.0F, golden.normalMapResolution};
		const auto output = decimator.decimate();
		uint64_t normalMapHash = 0;

		if ( golden.normalMapResolution > 0 )
		{
			const auto & normalMap = decimator.normalMap();

			normalMapHash = fingerprintBytes(UINT64_C(1469598103934665603), normalMap.data().data(), normalMap.data().size());
		}

		std::cout << "[golden] " << golden.name << " " << output.triangles().size() << " tris " << output.vertices().size() << " verts " << std::hex << fingerprintShape(output) << " nm " << normalMapHash << std::dec << "\n";

		EXPECT_FALSE(output.triangles().empty()) << golden.name;
	}
}
