/*
 * src/Testing/test_VertexFactoryShapeVertex.cpp
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

/* Local inclusions. */
#include "Math/Vector.hpp"
#include "VertexFactory/ShapeVertex.hpp"

using namespace EmEn::Base::Math;
using namespace EmEn::Base::VertexFactory;

namespace
{
	/* A right-handed frame: normal +Z, tangent +X, so cross(N, T) is +Y. */
	constexpr auto NormalZ = 2UL;

	ShapeVertex< float >
	makeFrame () noexcept
	{
		ShapeVertex< float > vertex;
		vertex.setNormal(Vector< 3, float >{0.0F, 0.0F, 1.0F});
		vertex.setTangent(Vector< 3, float >{1.0F, 0.0F, 0.0F});

		return vertex;
	}
}

/* The neutral handedness is +1, and it must be the DEFAULT: every generated shape and every
 * loader that never touches the handedness has to behave exactly as it did before the field
 * existed. This test is the regression guard for that promise. */
TEST(VertexFactoryShapeVertex, defaultHandednessLeavesTheCrossProductUntouched)
{
	const auto vertex = makeFrame();

	EXPECT_FLOAT_EQ(vertex.tangentHandedness(), 1.0F);

	const auto biNormal = vertex.biNormal();

	EXPECT_FLOAT_EQ(biNormal[X], 0.0F);
	EXPECT_FLOAT_EQ(biNormal[Y], 1.0F);
	EXPECT_FLOAT_EQ(biNormal[Z], 0.0F);
}

/* A mirrored UV island authors a handedness of -1, and the bitangent must FLIP. Without this,
 * cross(normal, tangent) points the wrong way on the mirrored half of a model and its normal
 * map lights backwards — the defect the Khronos NormalTangentMirrorTest exists to catch. */
TEST(VertexFactoryShapeVertex, negativeHandednessFlipsTheBiNormal)
{
	auto vertex = makeFrame();
	vertex.setTangentHandedness(-1.0F);

	const auto biNormal = vertex.biNormal();

	EXPECT_FLOAT_EQ(biNormal[X], 0.0F);
	EXPECT_FLOAT_EQ(biNormal[Y], -1.0F);
	EXPECT_FLOAT_EQ(biNormal[Z], 0.0F);
}

/* ⚠️ The 4D overload used to DROP W. It is the whole point of the vec4 in glTF's TANGENT
 * accessor, so a silent drop there is what lost the mirroring information. */
TEST(VertexFactoryShapeVertex, fourComponentSetterKeepsWAsTheHandedness)
{
	ShapeVertex< float > vertex;
	vertex.setNormal(Vector< 3, float >{0.0F, 0.0F, 1.0F});
	vertex.setTangent(Vector< 4, float >{1.0F, 0.0F, 0.0F, -1.0F});

	EXPECT_FLOAT_EQ(vertex.tangentHandedness(), -1.0F);

	/* The XYZ must still land in the tangent, unchanged. */
	EXPECT_FLOAT_EQ(vertex.tangent()[X], 1.0F);
	EXPECT_FLOAT_EQ(vertex.tangent()[Y], 0.0F);
	EXPECT_FLOAT_EQ(vertex.tangent()[Z], 0.0F);

	EXPECT_FLOAT_EQ(vertex.biNormal()[Y], -1.0F);
}

/* The 3D overload must NOT touch the handedness: a caller setting only a direction is not
 * making a statement about mirroring, and clobbering it to a default would silently undo an
 * authored handedness set beforehand. */
TEST(VertexFactoryShapeVertex, threeComponentSetterLeavesTheHandednessAlone)
{
	ShapeVertex< float > vertex;
	vertex.setTangentHandedness(-1.0F);
	vertex.setTangent(Vector< 3, float >{0.0F, 1.0F, 0.0F});

	EXPECT_FLOAT_EQ(vertex.tangentHandedness(), -1.0F);
}

/* The handedness is a SIGN, and biNormal() applies it as a plain multiplication — so a caller
 * that stores something other than ±1 gets it scaled, not normalised. Pinned deliberately:
 * the loaders are responsible for handing over ±1, and a future normalisation inside biNormal()
 * would be a behaviour change this test makes visible. */
TEST(VertexFactoryShapeVertex, handednessIsAppliedAsAPlainFactor)
{
	auto vertex = makeFrame();
	vertex.setTangentHandedness(-2.0F);

	EXPECT_FLOAT_EQ(vertex.biNormal()[Y], -2.0F);
}

/* ⚠️⚠️ FileFormatNative writes vertices as a RAW BLOB of sizeof(ShapeVertex), so this size is
 * part of a persisted format. If this test fails, the on-disk layout changed and
 * FileFormatNative's version MUST be bumped in the same commit — a size change with an
 * unchanged version number is silent data corruption, not a compatibility question. */
TEST(VertexFactoryShapeVertex, sizeIsPinnedBecauseTheNativeFormatIsARawBlob)
{
	EXPECT_EQ(sizeof(ShapeVertex< float >), 84U);
	static_assert(NormalZ == 2UL, "frame convention");
}
