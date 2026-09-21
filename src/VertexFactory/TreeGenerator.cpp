/*
 * src/VertexFactory/TreeGenerator.cpp
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


#include "TreeGenerator.hpp"

/* Local inclusions. */
#include "TreeParametricGrower.hpp"
#include "TreeSkinner.hpp"

namespace EmEn::Base::VertexFactory
{
	TreeMesh< float >
	TreeGenerator::generate (uint32_t seed) const noexcept
	{
		TreeMesh< float > mesh;

		auto skeleton = m_growerType == GrowerType::Parametric ?
			TreeParametricGrower< float >{m_parameters}.grow(seed) :
			m_colonizationGrower.grow(seed);

		if ( skeleton.empty() )
		{
			return mesh;
		}

		/* The pruning ladder of the coarser levels is expressed relative to the thickest branch,
		 * so a sapling and a mature oak lose the same PROPORTION of their twigs. */
		float trunkRadius = 0.0F;

		for ( const auto & segment : skeleton.segments() )
		{
			trunkRadius = std::max(trunkRadius, segment.startRadius());
		}

		for ( uint32_t level = 0; level < m_levelOfDetailCount; ++level )
		{
			const TreeSkinner< float > skinner{m_skinningOptions.coarsened(level, trunkRadius)};

			mesh.addLevelOfDetail(skinner.skin(skeleton));
		}

		if ( m_imposterEnabled )
		{
			const TreeSkinner< float > skinner{m_skinningOptions};

			mesh.setImposter(skinner.skinImposter(skeleton, m_imposterQuadCount));
		}

		mesh.setSkeleton(std::move(skeleton));

		return mesh;
	}
}
