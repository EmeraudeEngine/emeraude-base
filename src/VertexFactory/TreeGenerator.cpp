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
	namespace
	{
		/* The one bark of the store so far (Materials/Vegetals/palm_bark.json). */
		constexpr auto DefaultBark{"Vegetals/palm_bark"};
	}

	TreeGenerator
	TreeGenerator::quakingAspen () noexcept
	{
		TreeGenerator generator;
		generator.parameters() = TreeParameters< float >::quakingAspen();
		/* A fast pioneer: 13 m at 25 years, ~20 m at a hundred, where aspens stop. */
		generator.setGrowthCurve({25.0F, 0.05F, 1.3F});
		generator.setBarkMaterial(DefaultBark);
		generator.setLeafMaterial("Vegetals/leaf001");
		generator.skinningOptions().setLeafAspectRatio(1.0F);

		return generator;
	}

	TreeGenerator
	TreeGenerator::broadleaf () noexcept
	{
		TreeGenerator generator;
		generator.parameters() = TreeParameters< float >::broadleaf();
		/* A slow broadleaf: 9 m at 25 years, ~30 m at two hundred — an old beech. */
		generator.setGrowthCurve({25.0F, 0.02F, 1.3F});
		generator.setBarkMaterial(DefaultBark);
		generator.setLeafMaterial("Vegetals/leaf003");
		generator.skinningOptions().setLeafAspectRatio(1.0F);

		return generator;
	}

	TreeGenerator
	TreeGenerator::conifer () noexcept
	{
		TreeGenerator generator;
		generator.parameters() = TreeParameters< float >::conifer();
		/* 18 m at 35 years, ~36 m at two hundred. */
		generator.setGrowthCurve({35.0F, 0.025F, 1.3F});
		generator.setBarkMaterial(DefaultBark);
		generator.setLeafMaterial("Vegetals/leaf007");
		/* leaf007 is 1024 x 2048: a card twice as long as wide keeps the twig undistorted. ⚠️ The card is a whole
		 * TWIG of needles, 23 % opaque (the old fullfoliage card was 96 %): at the preset's 10 cm the crown was 54 %
		 * opaque (99 % before) and the far conifers showed their trunks alone (owner, 2026-09-23). A 20 cm twig
		 * stacks back to 77 % of the foliage footprint, 42 % of the crown box (38 % before), for no triangle. */
		generator.skinningOptions().setLeafAspectRatio(0.5F);
		generator.parameters().setLeafScale(0.2F);

		return generator;
	}

	TreeGenerator
	TreeGenerator::colonizedCrown () noexcept
	{
		TreeGenerator generator;
		generator.setGrowerType(GrowerType::SpaceColonization);
		/* A maple-like crown: ~11.5 m at 25 years, ~26 m at a hundred and fifty. */
		generator.setGrowthCurve({25.0F, 0.03F, 1.3F});
		generator.colonizationGrower().setAttractorCount(1600);
		generator.colonizationGrower().setCrownCenter({0.0F, 8.0F, 0.0F});
		generator.colonizationGrower().setCrownRadii({4.5F, 3.5F, 4.5F});
		generator.colonizationGrower().setTrunkHeight(4.0F);
		generator.setBarkMaterial(DefaultBark);
		generator.setLeafMaterial("Vegetals/leaf002");
		generator.skinningOptions().setLeafAspectRatio(1.0F);
		/* leaf002 is 36 % opaque against fullfoliage's 96 %: 1.5 times the leaf size holds the sparse colonized crown
		 * at 18 % of its box (12.5 % before the new leaves) for no triangle; doubling the leaves cost 36 % more. */
		generator.colonizationGrower().setLeafScale(0.225F);

		return generator;
	}

	TreeMesh< float >
	TreeGenerator::generate (uint32_t seed) const noexcept
	{
		TreeMesh< float > mesh;
		mesh.setMaterialNames(m_barkMaterial, m_leafMaterial);

		/* The age acts on a COPY: the generator keeps describing its reference tree. */
		auto skeleton = [this, seed] {
			if ( m_growerType == GrowerType::Parametric )
			{
				auto parameters = m_parameters;

				m_growthCurve.apply(parameters, m_age);

				return TreeParametricGrower< float >{parameters}.grow(seed);
			}

			auto grower = m_colonizationGrower;

			m_growthCurve.apply(grower, m_age);

			return grower.grow(seed);
		}();

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
