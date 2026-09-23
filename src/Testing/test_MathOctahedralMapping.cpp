/*
 * src/Testing/test_MathOctahedralMapping.cpp
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
 */

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* STL inclusions. */
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

/* Local inclusions. */
#include "Math/OctahedralMapping.hpp"

using namespace EmEn::Base::Math;

namespace
{
	/** @brief A spread of directions covering BOTH hemispheres, plus the axes and the folds. */
	std::vector< Vector< 3, float > >
	sphereSamples ()
	{
		std::vector< Vector< 3, float > > directions{
			{0.0F, 1.0F, 0.0F}, {0.0F, -1.0F, 0.0F},
			{1.0F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F},
			{0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, -1.0F},
			/* The four octahedron edges of the equator, where the fold happens. */
			{0.7071F, 0.0F, 0.7071F}, {-0.7071F, 0.0F, 0.7071F},
			{0.7071F, 0.0F, -0.7071F}, {-0.7071F, 0.0F, -0.7071F}
		};

		/* A regular spread over the whole sphere, so the lower hemisphere is not left out. */
		for ( uint32_t parallel = 1; parallel < 12; ++parallel )
		{
			const auto theta = std::numbers::pi_v< float > * static_cast< float >(parallel) / 12.0F;

			for ( uint32_t meridian = 0; meridian < 16; ++meridian )
			{
				const auto phi = 2.0F * std::numbers::pi_v< float > * static_cast< float >(meridian) / 16.0F;

				directions.emplace_back(Vector< 3, float >{
					std::sin(theta) * std::cos(phi),
					std::cos(theta),
					std::sin(theta) * std::sin(phi)
				}.normalized());
			}
		}

		return directions;
	}
}

/* ⚠️ The whole point of the fold: the map must round-trip on BOTH hemispheres. Getting the
 * lower-hemisphere fold wrong is the classic octahedral defect, and it survives any test that
 * samples only the upper half — which is why sphereSamples() sweeps parallels from pole to pole. */
TEST(MathOctahedralMapping, everyDirectionSurvivesTheRoundTrip)
{
	for ( const auto & direction : sphereSamples() )
	{
		const auto encoded = octahedralEncode(direction);

		ASSERT_GE(encoded[X], -1e-5F) << "the encoded point left the unit square";
		ASSERT_LE(encoded[X], 1.0F + 1e-5F) << "the encoded point left the unit square";
		ASSERT_GE(encoded[Y], -1e-5F) << "the encoded point left the unit square";
		ASSERT_LE(encoded[Y], 1.0F + 1e-5F) << "the encoded point left the unit square";

		const auto decoded = octahedralDecode(encoded);

		EXPECT_NEAR((Vector< 3, float >::dotProduct(direction, decoded)), 1.0F, 1e-4F)
			<< "direction (" << direction[X] << ", " << direction[Y] << ", " << direction[Z]
			<< ") came back as (" << decoded[X] << ", " << decoded[Y] << ", " << decoded[Z] << ")";
	}
}

/* The blend feeds a weighted sum of three texture samples: weights that do not sum to 1 change the
 * brightness of the imposter with the camera angle. */
TEST(MathOctahedralMapping, theThreeBlendWeightsArePositiveAndSumToOne)
{
	for ( const auto gridSize : {2U, 3U, 8U, 16U} )
	{
		for ( const auto & direction : sphereSamples() )
		{
			const auto blend = octahedralBlend(direction, gridSize);

			float sum = 0.0F;

			for ( size_t index = 0; index < 3; ++index )
			{
				EXPECT_GE(blend.weights[index], -1e-5F) << "a negative weight would subtract a view";

				sum += blend.weights[index];

				ASSERT_LT(blend.cells[index][0], gridSize) << "a cell fell outside the atlas";
				ASSERT_LT(blend.cells[index][1], gridSize) << "a cell fell outside the atlas";
			}

			EXPECT_NEAR(sum, 1.0F, 1e-4F) << "the three weights do not sum to 1, the imposter would change brightness with the angle";
		}
	}
}

/* ⚠️⚠️ The baker and the shader must agree on the DIRECTION a cell holds — not on its index.
 * On the outer border the map is 2-to-1: two border points denote the same direction, so two
 * border cells legitimately hold the same view. An earlier version of this test demanded that
 * a cell recognise its own INDEX and failed on cell (3, 7) of an 8x8 grid, whose direction the
 * blend correctly attributes to cell (4, 7) — the two decode to the very same vector. The
 * property that matters for an imposter is that the blended VIEW is the right one. */
TEST(MathOctahedralMapping, aCellDirectionBlendsBackOntoThatSameDirection)
{
	constexpr uint32_t GridSize = 8;

	for ( uint32_t cellY = 0; cellY < GridSize; ++cellY )
	{
		for ( uint32_t cellX = 0; cellX < GridSize; ++cellX )
		{
			const auto direction = octahedralCellDirection< float >(cellX, cellY, GridSize);

			const auto blend = octahedralBlend(direction, GridSize);

			/* What the imposter would actually show: the blend of the cells it selected. */
			Vector< 3, float > blended{0.0F, 0.0F, 0.0F};

			for ( size_t index = 0; index < 3; ++index )
			{
				blended += octahedralCellDirection< float >(blend.cells[index][0], blend.cells[index][1], GridSize) * blend.weights[index];
			}

			ASSERT_GT(blended.length(), 1e-4F) << "the three selected cells cancelled each other out";

			EXPECT_NEAR((Vector< 3, float >::dotProduct(direction, blended.normalized())), 1.0F, 1e-3F)
				<< "cell (" << cellX << ", " << cellY << ") blends back to a different direction";
		}
	}
}

/* An imposter that jumps as the camera turns is the defect this technique exists to avoid, so two
 * close directions must land on close points of the square. */
TEST(MathOctahedralMapping, aSmallRotationMovesTheEncodedPointOnlyALittle)
{
	constexpr float Step = 0.01F;

	float worst = 0.0F;

	for ( const auto & direction : sphereSamples() )
	{
		/* A small perturbation in an arbitrary but reproducible direction. */
		const auto nudged = Vector< 3, float >{
			direction[X] + Step * 0.7F,
			direction[Y] - Step * 0.3F,
			direction[Z] + Step * 0.6F
		}.normalized();

		const auto a = octahedralEncode(direction);
		const auto b = octahedralEncode(nudged);

		/* ⚠️ Across the OUTER border the map wraps, so the square distance legitimately jumps;
		 * those samples are the seam and are excluded rather than asserted on. */
		const auto onSeam = a[X] < 0.02F || a[X] > 0.98F || a[Y] < 0.02F || a[Y] > 0.98F;

		if ( onSeam )
		{
			continue;
		}

		worst = std::max(worst, (b - a).length());
	}

	EXPECT_LT(worst, 0.05F) << "a " << Step << " rotation moved the encoded point by " << worst << " of the square";
}

namespace
{
	/** @brief A spread of UPPER-hemisphere directions, the pole and the horizon included. */
	std::vector< Vector< 3, float > >
	upperHemisphereSamples ()
	{
		std::vector< Vector< 3, float > > directions{
			{0.0F, 1.0F, 0.0F},
			{1.0F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, -1.0F},
			{0.7071F, 0.0F, 0.7071F}, {-0.7071F, 0.0F, -0.7071F}
		};

		for ( uint32_t parallel = 0; parallel <= 12; ++parallel )
		{
			const auto theta = 0.5F * std::numbers::pi_v< float > * static_cast< float >(parallel) / 12.0F;

			for ( uint32_t meridian = 0; meridian < 24; ++meridian )
			{
				const auto phi = 2.0F * std::numbers::pi_v< float > * static_cast< float >(meridian) / 24.0F;

				directions.emplace_back(Vector< 3, float >{
					std::sin(theta) * std::cos(phi),
					std::cos(theta),
					std::sin(theta) * std::sin(phi)
				}.normalized());
			}
		}

		return directions;
	}
}

/* The hemi map spends the whole square on the upper hemisphere: every direction above the horizon, the
 * horizon itself included, must come back unchanged. */
TEST(MathOctahedralMapping, everyUpperDirectionSurvivesTheHemiRoundTrip)
{
	for ( const auto & direction : upperHemisphereSamples() )
	{
		const auto encoded = hemiOctahedralEncode(direction);

		ASSERT_GE(encoded[X], -1e-5F);
		ASSERT_LE(encoded[X], 1.0F + 1e-5F);
		ASSERT_GE(encoded[Y], -1e-5F);
		ASSERT_LE(encoded[Y], 1.0F + 1e-5F);

		const auto decoded = hemiOctahedralDecode(encoded);

		EXPECT_NEAR((Vector< 3, float >::dotProduct(direction, decoded)), 1.0F, 1e-4F)
			<< "direction (" << direction[X] << ", " << direction[Y] << ", " << direction[Z]
			<< ") came back as (" << decoded[X] << ", " << decoded[Y] << ", " << decoded[Z] << ")";
	}
}

/* The whole square decodes to the upper hemisphere: no point of the atlas describes a view from below. */
TEST(MathOctahedralMapping, theHemiSquareNeverDecodesBelowTheHorizon)
{
	for ( uint32_t row = 0; row <= 32; ++row )
	{
		for ( uint32_t column = 0; column <= 32; ++column )
		{
			const auto decoded = hemiOctahedralDecode(Vector< 2, float >{static_cast< float >(column) / 32.0F, static_cast< float >(row) / 32.0F});

			EXPECT_GE(decoded[Y], 0.0F);
			EXPECT_NEAR(decoded.length(), 1.0F, 1e-5F);
		}
	}
}

/* A tree seen from slightly below shows its horizon view: a lower direction encodes onto the horizon. */
TEST(MathOctahedralMapping, aDirectionBelowTheHorizonClampsOntoIt)
{
	const Vector< 3, float > below{0.8F, -0.3F, 0.2F};
	const Vector< 3, float > horizon{0.8F, 0.0F, 0.2F};

	const auto encodedBelow = hemiOctahedralEncode(below);
	const auto encodedHorizon = hemiOctahedralEncode(horizon);

	EXPECT_NEAR(encodedBelow[X], encodedHorizon[X], 1e-6F);
	EXPECT_NEAR(encodedBelow[Y], encodedHorizon[Y], 1e-6F);
}

/* Unlike the full map, the hemi map has NO 2-to-1 border: every cell's own direction must blend back onto
 * that very cell with a weight of 1 — which the full map cannot promise (see the border test above). */
TEST(MathOctahedralMapping, everyHemiCellBlendsBackOntoItself)
{
	for ( const auto gridSize : {2U, 3U, 8U, 16U} )
	{
		for ( uint32_t cellY = 0; cellY < gridSize; ++cellY )
		{
			for ( uint32_t cellX = 0; cellX < gridSize; ++cellX )
			{
				const auto direction = hemiOctahedralCellDirection(cellX, cellY, gridSize);
				const auto blend = hemiOctahedralBlend(direction, gridSize);

				float ownWeight = 0.0F;
				float sum = 0.0F;

				for ( size_t index = 0; index < 3; ++index )
				{
					EXPECT_GE(blend.weights[index], -1e-5F);

					sum += blend.weights[index];

					if ( blend.cells[index][0] == cellX && blend.cells[index][1] == cellY )
					{
						ownWeight += blend.weights[index];
					}
				}

				EXPECT_NEAR(sum, 1.0F, 1e-4F);
				EXPECT_NEAR(ownWeight, 1.0F, 1e-3F) << "grid " << gridSize << ", cell (" << cellX << ", " << cellY << ") does not recognise its own view";
			}
		}
	}
}

/* The frame the baker renders a cell with and the shader reads it back with: orthonormal, right-handed,
 * looking along -direction, upright (its up leans toward +Y) everywhere but at the pole. */
TEST(MathOctahedralMapping, theImposterCellFrameIsOrthonormalRightHandedAndUpright)
{
	for ( const auto & direction : upperHemisphereSamples() )
	{
		const auto frame = imposterCellFrame(direction);

		EXPECT_NEAR(frame.right.length(), 1.0F, 1e-5F);
		EXPECT_NEAR(frame.up.length(), 1.0F, 1e-5F);
		EXPECT_NEAR(frame.back.length(), 1.0F, 1e-5F);

		EXPECT_NEAR((Vector< 3, float >::dotProduct(frame.right, frame.up)), 0.0F, 1e-5F);
		EXPECT_NEAR((Vector< 3, float >::dotProduct(frame.right, frame.back)), 0.0F, 1e-5F);
		EXPECT_NEAR((Vector< 3, float >::dotProduct(frame.up, frame.back)), 0.0F, 1e-5F);

		EXPECT_NEAR((Vector< 3, float >::dotProduct(Vector< 3, float >::crossProduct(frame.right, frame.up), frame.back)), 1.0F, 1e-5F) << "the frame is left-handed";
		EXPECT_NEAR((Vector< 3, float >::dotProduct(frame.back, direction.normalized())), 1.0F, 1e-5F);

		if ( direction[Y] < 0.9999F )
		{
			EXPECT_GE(frame.up[Y], 0.0F) << "the view of (" << direction[X] << ", " << direction[Y] << ", " << direction[Z] << ") is upside down";
		}
	}

	/* The engine's own camera convention: a view from +Z sees +X on its right and +Y up. */
	const auto front = imposterCellFrame(Vector< 3, float >{0.0F, 0.0F, 1.0F});

	EXPECT_NEAR(front.right[X], 1.0F, 1e-6F);
	EXPECT_NEAR(front.up[Y], 1.0F, 1e-6F);
}
