/*
 * src/VertexFactory/ShapeSimplifier.cpp
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

#include "ShapeSimplifier.hpp"

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

/* Third-party inclusions. */
#include "meshoptimizer.h"

/* Local inclusions. */
#include "Logging/Logging.hpp"

namespace EmEn::Base::VertexFactory
{
	std::optional< Shape< float > >
	simplifyShape (const Shape< float > & source, const ShapeSimplifierOptions & options) noexcept
	{
		const auto & sourceVertices = source.vertices();
		const auto & sourceTriangles = source.triangles();

		if ( sourceVertices.empty() || sourceTriangles.empty() || !std::isfinite(options.targetRatio) || options.targetRatio <= 0.0F || options.targetRatio > 1.0F || !std::isfinite(options.targetError) || options.targetError < 0.0F )
		{
			return std::nullopt;
		}

		/* meshoptimizer reads positions as a packed float3 array. */
		std::vector< float > positions;
		positions.reserve(sourceVertices.size() * 3);

		for ( const auto & vertex : sourceVertices )
		{
			const auto & position = vertex.position();

			positions.push_back(position[0]);
			positions.push_back(position[1]);
			positions.push_back(position[2]);
		}

		/* A shape without groups is one group over every triangle. */
		std::vector< std::pair< uint32_t, uint32_t > > groups{source.groups().begin(), source.groups().end()};

		if ( groups.empty() )
		{
			groups.emplace_back(0U, static_cast< uint32_t >(sourceTriangles.size()));
		}

		std::vector< std::vector< uint32_t > > groupIndices;
		groupIndices.reserve(groups.size());

		for ( const auto & [first, count] : groups )
		{
			if ( static_cast< size_t >(first) + count > sourceTriangles.size() )
			{
				Logging::error("ShapeSimplifier", "A group [" + std::to_string(first) + ", +" + std::to_string(count) + ") lies outside the " + std::to_string(sourceTriangles.size()) + " triangles: refused.");

				return std::nullopt;
			}

			/* An EMPTY group stays empty: it is a sub-geometry all the same, and every level must expose the same ones
			 * (JungleRuins' trees carry one — refusing it cost them their whole chain, 2026-10-04). */
			if ( count == 0 )
			{
				groupIndices.emplace_back();

				continue;
			}

			std::vector< uint32_t > indices;
			indices.reserve(static_cast< size_t >(count) * 3);

			for ( uint32_t triangle = first; triangle < first + count; ++triangle )
			{
				for ( uint32_t corner = 0; corner < 3; ++corner )
				{
					const auto vertexIndex = sourceTriangles[triangle].vertexIndex(corner);

					if ( vertexIndex >= sourceVertices.size() )
					{
						Logging::error("ShapeSimplifier", "A triangle indexes vertex " + std::to_string(vertexIndex) + " of " + std::to_string(sourceVertices.size()) + ": refused.");

						return std::nullopt;
					}

					indices.push_back(vertexIndex);
				}
			}

			const auto targetIndexCount = std::max< size_t >(3, static_cast< size_t >(static_cast< float >(indices.size()) * options.targetRatio) / 3 * 3);
			std::vector< uint32_t > simplified(indices.size());

			auto resultCount = meshopt_simplify(simplified.data(), indices.data(), indices.size(), positions.data(), sourceVertices.size(), sizeof(float) * 3, targetIndexCount, options.targetError, 0, nullptr);

			/* Stalled far above the target: disconnected cards. The sloppy pass welds across the gaps. */
			if ( options.allowSloppy && resultCount > targetIndexCount * 2 )
			{
				resultCount = meshopt_simplifySloppy(simplified.data(), indices.data(), indices.size(), positions.data(), sourceVertices.size(), sizeof(float) * 3, nullptr, targetIndexCount, std::numeric_limits< float >::max(), nullptr);
			}

			if ( resultCount < 3 )
			{
				Logging::warning("ShapeSimplifier", "A group of " + std::to_string(indices.size() / 3) + " triangles would vanish (target " + std::to_string(targetIndexCount / 3) + "): no level.");

				return std::nullopt;
			}

			simplified.resize(resultCount);
			groupIndices.push_back(std::move(simplified));
		}

		/* Only the referenced vertices, in first-use order, with their attributes. */
		Shape< float > output;
		std::vector< uint32_t > remap(sourceVertices.size(), std::numeric_limits< uint32_t >::max());

		auto & outputGroups = output.groups();
		outputGroups.clear();

		for ( const auto & indices : groupIndices )
		{
			const auto groupStart = static_cast< uint32_t >(output.triangles().size());

			for ( size_t index = 0; index + 2 < indices.size(); index += 3 )
			{
				std::array< uint32_t, 3 > corners{};

				for ( size_t corner = 0; corner < 3; ++corner )
				{
					const auto sourceIndex = indices[index + corner];

					if ( remap[sourceIndex] == std::numeric_limits< uint32_t >::max() )
					{
						const auto & vertex = sourceVertices[sourceIndex];
						const auto & secondary = vertex.secondaryTextureCoordinates();

						remap[sourceIndex] = static_cast< uint32_t >(output.saveVertex(vertex.position(), vertex.normal(), vertex.textureCoordinates()));
						output.vertices()[remap[sourceIndex]].setSecondaryTextureCoordinates({secondary[0], secondary[1]});
					}

					corners[corner] = remap[sourceIndex];
				}

				output.triangles().emplace_back(corners[0], corners[1], corners[2]);
			}

			outputGroups.emplace_back(groupStart, static_cast< uint32_t >(output.triangles().size()) - groupStart);
		}

		output.declareNormalsAvailable();
		static_cast< void >(output.computeTriangleNormal());
		output.updateProperties();

		return output;
	}
}
