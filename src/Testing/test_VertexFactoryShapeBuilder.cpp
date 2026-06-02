/*
 * src/Testing/test_VertexFactoryShapeBuilder.cpp
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
#include <cstdint>

/* Local inclusions. */
#include "Math/Vector.hpp"
#include "VertexFactory/Shape.hpp"
#include "VertexFactory/ShapeBuilder.hpp"

using namespace EmEn::Base::VertexFactory;

/* Ave robustus! (Axis B — correction marker): confirms the ConstructionMode::TriangleFan vertex
 * shift (the resolved `FIXME: Check this` in ShapeBuilder). A fan of one origin + 4 rim vertices
 * (5 total) must emit N-2 = 3 triangles, all sharing the fan origin. */
TEST(VertexFactoryShapeBuilder, triangleFanProducesNMinus2Triangles)
{
	Shape< float, uint32_t > shape;
	ShapeBuilder< float, uint32_t > builder{shape};

	builder.options().enableGlobalNormal(EmEn::Base::Math::Vector< 3, float >::positiveZ());
	builder.beginConstruction(ConstructionMode::TriangleFan);

	builder.setPosition(0.0F, 0.0F, 0.0F);   builder.newVertex();   /* fan origin */
	builder.setPosition(1.0F, 0.0F, 0.0F);   builder.newVertex();
	builder.setPosition(1.0F, 1.0F, 0.0F);   builder.newVertex();
	builder.setPosition(0.0F, 1.0F, 0.0F);   builder.newVertex();
	builder.setPosition(-1.0F, 1.0F, 0.0F);  builder.newVertex();

	builder.endConstruction();

	EXPECT_EQ(shape.triangles().size(), 3U);
}
