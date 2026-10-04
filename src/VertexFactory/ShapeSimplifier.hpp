/*
 * src/VertexFactory/ShapeSimplifier.hpp
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

#pragma once

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <optional>

/* Local inclusions for usages. */
#include "Shape.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief How a shape is simplified.
	 */
	struct ShapeSimplifierOptions final
	{
		/** @brief The triangle count to reach, as a fraction of the source's, in ]0, 1]. */
		float targetRatio{0.25F};

		/**
		 * @brief The largest geometric error allowed, RELATIVE to the shape's extent (meshoptimizer's convention):
		 * 0.01 = 1 % of the bounding size. The topology-preserving pass stops at whichever comes first.
		 */
		float targetError{0.02F};

		/**
		 * @brief When the topology-preserving pass stalls above twice the target (foliage: thousands of disconnected
		 * leaf cards it may only shrink, never merge), retry that group with meshoptimizer's "sloppy" simplifier, which
		 * welds across the gaps.
		 */
		bool allowSloppy{true};
	};

	/**
	 * @brief Simplifies a shape with meshoptimizer (Arseny Kapoulkine, MIT licence,
	 * https://github.com/zeux/meshoptimizer): a quadric simplifier that keeps the topology, and a "sloppy" one for what it
	 * cannot reduce.
	 * @note Group by group: every group (a sub-geometry, one material) is simplified on its own and kept, so a chain of
	 * levels exposes the same sub-geometries on every level (MultiLayerMeshResource requires it). An empty group stays empty.
	 * @note The kept vertices keep their attributes (normal, both texture coordinate sets); triangle normals, tangents and
	 * the bounding volumes are recomputed. The output carries no vertex colour: a simplified mesh is a distant level.
	 * @param source The shape.
	 * @param options The options.
	 * @return std::optional< Shape< float > > Nothing when the source is empty, the ratio invalid, or a group would
	 * vanish entirely (a chain must stop at the previous level rather than lose a sub-geometry).
	 */
	[[nodiscard]]
	std::optional< Shape< float > > simplifyShape (const Shape< float > & source, const ShapeSimplifierOptions & options = {}) noexcept;
}
