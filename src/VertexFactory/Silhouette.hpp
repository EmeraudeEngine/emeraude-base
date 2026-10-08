/*
 * src/VertexFactory/Silhouette.hpp
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
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <vector>

/* Local inclusions for usages. */
#include "Math/Vector.hpp"
#include "Shape.hpp"
#include "ShapeEdge.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief Extracts the silhouette (contour) edges of a shape as seen from a viewer.
	 * @note A silhouette edge is an edge shared by a triangle facing the viewer and a triangle facing away,
	 * plus every boundary edge (an edge with a single triangle) whose triangle faces the viewer.
	 * See Akenine-Möller, Haines, Hoffman et al., "Real-Time Rendering", 4th ed., § 15.2.
	 * @note The adjacency is NOT taken from Shape::edges(): those half-edges pair by vertex INDEX, so a
	 * seam (per-face normals, UV seams) splits the surface and every seam edge looks like a boundary. This
	 * class welds the vertices by exact position instead (the approach of meshoptimizer's
	 * meshopt_generateShadowIndexBuffer, https://github.com/zeux/meshoptimizer, MIT). Positions that differ
	 * by float noise are NOT welded.
	 * @note The adjacency and the triangle planes are computed once by prepare(); build() and
	 * buildOrthographic() only classify the triangles and walk the edges, without allocation once the
	 * output has reached its peak size. prepare() again whenever the shape positions change (skinning,
	 * deformation). No reference to the shape is kept.
	 * @note Degenerate (zero-area) triangles are ignored. A double-sided card made of duplicated reversed
	 * triangles reports its interior edges too, since each of them joins a front and a back triangle.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 * @tparam index_data_t The precision type of index data. Default uint32_t.
	 */
	template< typename vertex_data_t = float, typename index_data_t = uint32_t >
	requires (std::is_floating_point_v< vertex_data_t > && std::is_unsigned_v< index_data_t > )
	class Silhouette final
	{
		public:

			/**
			 * @brief Constructs an empty silhouette.
			 */
			Silhouette () noexcept = default;

			/**
			 * @brief Gives access to the silhouette edges computed by the last build.
			 * @note Each edge holds vertex indexes of the shape given to prepare(), taken from the triangle
			 * facing the viewer and in its winding order (counter-clockwise), so the contour is consistently
			 * oriented and the indexes carry the attributes of the visible side of a seam.
			 * @return const std::vector< ShapeEdge< index_data_t > > &
			 */
			[[nodiscard]]
			const std::vector< ShapeEdge< index_data_t > > &
			edges () const noexcept
			{
				return m_edges;
			}

			/**
			 * @brief Returns whether prepare() succeeded.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isPrepared () const noexcept
			{
				return !m_planes.empty();
			}

			/**
			 * @brief Computes the adjacency and the triangle planes of a shape.
			 * @param geometry The shape from which the silhouette will be extracted.
			 * @return bool False if the shape has no triangle or only degenerate ones.
			 */
			bool
			prepare (const Shape< vertex_data_t, index_data_t > & geometry) noexcept
			{
				m_planes.clear();
				m_sides.clear();
				m_groupOffsets.clear();
				m_edges.clear();

				if ( !geometry.isValid() || geometry.vertices().empty() )
				{
					return false;
				}

				const auto & vertices = geometry.vertices();
				const auto & triangles = geometry.triangles();

				/* Weld the vertices by exact position. */
				std::vector< index_data_t > weldedIndexes(vertices.size());

				{
					std::unordered_map< Position, index_data_t, PositionHash > positionToWelded;
					positionToWelded.reserve(vertices.size());

					for ( size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex )
					{
						const auto & position = vertices[vertexIndex].position();
						const Position key{position.x(), position.y(), position.z()};

						weldedIndexes[vertexIndex] = positionToWelded.try_emplace(key, static_cast< index_data_t >(positionToWelded.size())).first->second;
					}
				}

				/* Compute the triangle planes and collect the half-edges keyed by welded endpoints. */
				m_planes.reserve(triangles.size());
				m_sides.reserve(triangles.size() * 3);

				for ( const auto & triangle : triangles )
				{
					const std::array< index_data_t, 3 > vertexIndexes{triangle.vertexIndex(0), triangle.vertexIndex(1), triangle.vertexIndex(2)};

					const auto & pointA = vertices[vertexIndexes[0]].position();
					const auto & pointB = vertices[vertexIndexes[1]].position();
					const auto & pointC = vertices[vertexIndexes[2]].position();

					/* NOTE: Not normalized, only the sign of the facing test matters. Counter-clockwise
					 * winding, the same direction as Math::Vector::normal(A, B, C). */
					const auto normal = Math::Vector< 3, vertex_data_t >::crossProduct(pointB - pointA, pointC - pointA);

					if ( normal.lengthSquared() <= 0 )
					{
						continue;
					}

					const auto planeIndex = static_cast< index_data_t >(m_planes.size());

					m_planes.push_back({normal, pointA});

					for ( size_t side = 0; side < 3; ++side )
					{
						const auto vertexA = vertexIndexes[side];
						const auto vertexB = vertexIndexes[(side + 1) % 3];
						const auto weldedA = weldedIndexes[vertexA];
						const auto weldedB = weldedIndexes[vertexB];

						m_sides.push_back({std::min(weldedA, weldedB), std::max(weldedA, weldedB), planeIndex, vertexA, vertexB});
					}
				}

				if ( m_planes.empty() )
				{
					return false;
				}

				/* Group the half-edges sharing the same welded endpoints. */
				std::ranges::sort(m_sides, [] (const Side & lhs, const Side & rhs) {
					if ( lhs.weldedLow != rhs.weldedLow )
					{
						return lhs.weldedLow < rhs.weldedLow;
					}

					return lhs.weldedHigh < rhs.weldedHigh;
				});

				for ( size_t sideIndex = 0; sideIndex < m_sides.size(); ++sideIndex )
				{
					if ( sideIndex == 0 || m_sides[sideIndex].weldedLow != m_sides[sideIndex - 1].weldedLow || m_sides[sideIndex].weldedHigh != m_sides[sideIndex - 1].weldedHigh )
					{
						m_groupOffsets.push_back(sideIndex);
					}
				}

				m_groupOffsets.push_back(m_sides.size());

				m_facing.resize(m_planes.size());

				return true;
			}

			/**
			 * @brief Extracts the silhouette seen from a point (perspective viewer).
			 * @param eyePosition The viewer position, in the shape's local space.
			 * @return bool False if prepare() did not succeed.
			 */
			bool
			build (const Math::Vector< 3, vertex_data_t > & eyePosition) noexcept
			{
				if ( !this->isPrepared() )
				{
					m_edges.clear();

					return false;
				}

				for ( size_t planeIndex = 0; planeIndex < m_planes.size(); ++planeIndex )
				{
					const auto & plane = m_planes[planeIndex];

					m_facing[planeIndex] = static_cast< uint8_t >(Math::Vector< 3, vertex_data_t >::dotProduct(plane.normal, eyePosition - plane.point) > 0);
				}

				this->collectEdges();

				return true;
			}

			/**
			 * @brief Extracts the silhouette seen along a direction (orthographic viewer).
			 * @param viewDirection The direction the viewer looks at, in the shape's local space. It does not need to be normalized.
			 * @return bool False if prepare() did not succeed.
			 */
			bool
			buildOrthographic (const Math::Vector< 3, vertex_data_t > & viewDirection) noexcept
			{
				if ( !this->isPrepared() )
				{
					m_edges.clear();

					return false;
				}

				for ( size_t planeIndex = 0; planeIndex < m_planes.size(); ++planeIndex )
				{
					m_facing[planeIndex] = static_cast< uint8_t >(Math::Vector< 3, vertex_data_t >::dotProduct(m_planes[planeIndex].normal, viewDirection) < 0);
				}

				this->collectEdges();

				return true;
			}

		private:

			/**
			 * @brief Walks the edge groups and keeps the silhouette edges from the current facing flags.
			 */
			void
			collectEdges () noexcept
			{
				m_edges.clear();

				for ( size_t groupIndex = 0; groupIndex + 1 < m_groupOffsets.size(); ++groupIndex )
				{
					const auto first = m_groupOffsets[groupIndex];
					const auto last = m_groupOffsets[groupIndex + 1];

					/* NOTE: A boundary edge, kept when its only triangle faces the viewer. */
					if ( last - first == 1 )
					{
						const auto & side = m_sides[first];

						if ( m_facing[side.planeIndex] != 0 )
						{
							m_edges.emplace_back(side.vertexA, side.vertexB);
						}

						continue;
					}

					/* NOTE: A shared edge (two triangles, or more on a non-manifold shape), kept when its
					 * triangles disagree on facing, emitted once from a triangle facing the viewer. */
					const Side * frontSide = nullptr;
					bool hasBackSide = false;

					for ( auto sideIndex = first; sideIndex < last; ++sideIndex )
					{
						const auto & side = m_sides[sideIndex];

						if ( m_facing[side.planeIndex] != 0 )
						{
							if ( frontSide == nullptr )
							{
								frontSide = &side;
							}
						}
						else
						{
							hasBackSide = true;
						}
					}

					if ( frontSide != nullptr && hasBackSide )
					{
						m_edges.emplace_back(frontSide->vertexA, frontSide->vertexB);
					}
				}
			}

			/**
			 * @brief A triangle plane: its unnormalized counter-clockwise normal and one of its points.
			 */
			struct Plane final
			{
				Math::Vector< 3, vertex_data_t > normal;
				Math::Vector< 3, vertex_data_t > point;
			};

			/**
			 * @brief One side of a triangle: its welded endpoints (sorted) for grouping, its triangle plane
			 * and its shape vertex indexes in winding order.
			 */
			struct Side final
			{
				index_data_t weldedLow{0};
				index_data_t weldedHigh{0};
				index_data_t planeIndex{0};
				index_data_t vertexA{0};
				index_data_t vertexB{0};
			};

			using Position = std::array< vertex_data_t, 3 >;

			/**
			 * @brief Hashes a position so that +0 and -0 (equal under operator==) share a hash.
			 */
			struct PositionHash final
			{
				/**
				 * @brief Returns the hash of a position.
				 * @param position A reference to the position.
				 * @return size_t
				 */
				[[nodiscard]]
				size_t
				operator() (const Position & position) const noexcept
				{
					size_t hash = 0;

					for ( const auto value : position )
					{
						const auto valueHash = value < 0 || value > 0 ? std::hash< vertex_data_t >{}(value) : 0;

						hash ^= valueHash + 0x9E3779B9UL + (hash << 6U) + (hash >> 2U);
					}

					return hash;
				}
			};

			std::vector< Plane > m_planes;
			std::vector< Side > m_sides;
			std::vector< size_t > m_groupOffsets;
			std::vector< uint8_t > m_facing;
			std::vector< ShapeEdge< index_data_t > > m_edges;
	};
}
