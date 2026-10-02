/*
 * src/Math/Space3D/TriangleMesh.hpp
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
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <vector>

/* Local inclusions for usages. */
#include "Math/Space3D/Triangle.hpp"
#include "Math/Space3D/Contacts/ContactManifold.hpp"
#include "Math/Space3D/Contacts/SphereTriangle.hpp"
#include "Math/Vector.hpp"

/*
 * A static triangle mesh for collision queries (physics overhaul P5): its triangles, their winding normals, the
 * ACTIVE EDGES of each one, and a bounding-volume hierarchy built once.
 * - The hierarchy is a binary AABB tree built with the surface area heuristic over 12 bins of the centroids
 *   (I. Wald, "On fast Construction of SAH-based Bounding Volume Hierarchies", 2007), at most 4 triangles per leaf,
 *   stored depth first (a node's left child follows it).
 * - Active edges: a body sliding across two coplanar triangles must not bump on the edge they share. An edge shared
 *   by exactly two triangles is INACTIVE when it is flat (the normals within the threshold) or concave, ACTIVE when it
 *   is convex beyond the threshold; a border, a non-manifold edge (3 triangles or more) or a knife edge is active. A
 *   contact whose points all lie on inactive features takes the face normal (correctInternalEdgeNormal()). The idea of
 *   Jolt's MeshShape "active edges" (J. Rouwé, MIT) — no code taken.
 * - Vertices of the same position are welded first (an exported mesh splits them by normal or by texture coordinate).
 */

namespace EmEn::Base::Math::Space3D
{
	/**
	 * @brief A static triangle mesh with its bounding-volume hierarchy and its active edges.
	 * @tparam precision_t The floating point type. Default float.
	 */
	template< typename precision_t = float >
	requires (std::is_floating_point_v< precision_t >)
	class TriangleMesh final
	{
		public:

			using Vec3 = Vector< 3, precision_t >;

			/** @brief The edge from A to B is active. */
			static constexpr uint8_t EdgeABActive{1};
			/** @brief The edge from B to C is active. */
			static constexpr uint8_t EdgeBCActive{2};
			/** @brief The edge from C to A is active. */
			static constexpr uint8_t EdgeCAActive{4};
			/** @brief The most triangles a leaf holds. */
			static constexpr uint32_t MaxLeafTriangles{4};
			/** @brief The deepest node (a leaf is forced there; the query stack is sized by it). */
			static constexpr uint32_t MaxDepth{48};

			/** @brief A node of the hierarchy: a leaf when count > 0 (its triangles [first, first + count)), else its left
			 * child is the next node and its right child is `right`. */
			struct Node final
			{
				Vec3 minimum;
				Vec3 maximum;
				uint32_t first{0};
				uint32_t count{0};
				uint32_t right{0};
			};

			/**
			 * @brief Constructs an empty mesh.
			 */
			TriangleMesh () noexcept = default;

			/**
			 * @brief Builds the mesh from an indexed triangle list.
			 * @note Refused (false, the mesh left empty): no index, a count not a multiple of 3, an index out of the
			 * vertices, a non-finite vertex, a cosine outside [-1, 1] or not finite. Degenerate triangles are skipped;
			 * none left is a refusal too.
			 * @param vertices The positions.
			 * @param indices Three indices per triangle, counter-clockwise for the front face.
			 * @param activeEdgeCosine The cosine of the angle under which a convex edge is flat (inactive). Jolt's
			 * default is cos(5°).
			 * @return bool
			 */
			[[nodiscard]]
			bool
			build (std::span< const Vec3 > vertices, std::span< const uint32_t > indices, precision_t activeEdgeCosine) noexcept
			{
				this->clear();

				if ( indices.empty() || indices.size() % 3 != 0 || !std::isfinite(activeEdgeCosine) || activeEdgeCosine < static_cast< precision_t >(-1) || activeEdgeCosine > static_cast< precision_t >(1) )
				{
					return false;
				}

				for ( const auto index : indices )
				{
					if ( index >= vertices.size() || !isFiniteVector(vertices[index]) )
					{
						return false;
					}
				}

				const auto welded = weldedIdentifiers(vertices);

				/* The triangles, the degenerate ones skipped. */
				std::vector< std::array< uint32_t, 3 > > identifiers;

				identifiers.reserve(indices.size() / 3);
				m_triangles.reserve(indices.size() / 3);
				m_normals.reserve(indices.size() / 3);

				for ( size_t offset = 0; offset < indices.size(); offset += 3 )
				{
					const Triangle< precision_t > triangle{vertices[indices[offset]], vertices[indices[offset + 1]], vertices[indices[offset + 2]]};
					Vec3 normal;

					if ( !TriangleDetail::unitNormal(triangle, normal) )
					{
						continue;
					}

					m_triangles.push_back(triangle);
					m_normals.push_back(normal);
					identifiers.push_back({welded[indices[offset]], welded[indices[offset + 1]], welded[indices[offset + 2]]});
				}

				if ( m_triangles.empty() )
				{
					this->clear();

					return false;
				}

				this->computeActiveEdges(identifiers, activeEdgeCosine);
				this->buildHierarchy();

				return true;
			}

			/**
			 * @brief Empties the mesh.
			 * @return void
			 */
			void
			clear () noexcept
			{
				m_triangles.clear();
				m_normals.clear();
				m_activeEdges.clear();
				m_nodes.clear();
				m_minimum.reset();
				m_maximum.reset();
			}

			/** @brief Returns whether the mesh holds no triangle. */
			[[nodiscard]]
			bool
			empty () const noexcept
			{
				return m_triangles.empty();
			}

			/** @brief Returns the number of triangles (the degenerate ones were skipped). */
			[[nodiscard]]
			size_t
			triangleCount () const noexcept
			{
				return m_triangles.size();
			}

			/**
			 * @brief Returns a triangle, in the hierarchy's order.
			 * @pre index < triangleCount().
			 * @param index The index.
			 * @return const Triangle< precision_t > &
			 */
			[[nodiscard]]
			const Triangle< precision_t > &
			triangle (size_t index) const noexcept
			{
				return m_triangles[index];
			}

			/**
			 * @brief Returns the unit winding normal of a triangle ((B - A) × (C - A)): its front face.
			 * @pre index < triangleCount().
			 * @param index The index.
			 * @return const Vec3 &
			 */
			[[nodiscard]]
			const Vec3 &
			normal (size_t index) const noexcept
			{
				return m_normals[index];
			}

			/**
			 * @brief Returns the active edge flags of a triangle (EdgeABActive | EdgeBCActive | EdgeCAActive).
			 * @pre index < triangleCount().
			 * @param index The index.
			 * @return uint8_t
			 */
			[[nodiscard]]
			uint8_t
			activeEdges (size_t index) const noexcept
			{
				return m_activeEdges[index];
			}

			/** @brief Returns the nodes of the hierarchy (the root first). */
			[[nodiscard]]
			const std::vector< Node > &
			nodes () const noexcept
			{
				return m_nodes;
			}

			/** @brief Returns the lowest corner of the mesh's bounds. */
			[[nodiscard]]
			const Vec3 &
			minimum () const noexcept
			{
				return m_minimum;
			}

			/** @brief Returns the highest corner of the mesh's bounds. */
			[[nodiscard]]
			const Vec3 &
			maximum () const noexcept
			{
				return m_maximum;
			}

			/**
			 * @brief Calls a function with the index of every triangle whose bounds overlap a box (borders included).
			 * @tparam function_t A callable taking the triangle index (uint32_t).
			 * @param minimum The lowest corner of the box.
			 * @param maximum The highest corner of the box.
			 * @param function The callable.
			 * @return void
			 */
			template< typename function_t >
			void
			visit (const Vec3 & minimum, const Vec3 & maximum, const function_t & function) const noexcept
			{
				if ( m_nodes.empty() )
				{
					return;
				}

				std::array< uint32_t, MaxDepth + 2 > stack{};
				size_t top = 0;
				uint32_t current = 0;

				while ( true )
				{
					const auto & node = m_nodes[current];

					if ( overlaps(node.minimum, node.maximum, minimum, maximum) )
					{
						if ( node.count > 0 )
						{
							for ( uint32_t index = node.first; index < node.first + node.count; ++index )
							{
								Vec3 lowest;
								Vec3 highest;

								triangleBounds(m_triangles[index], lowest, highest);

								if ( overlaps(lowest, highest, minimum, maximum) )
								{
									function(index);
								}
							}
						}
						else
						{
							/* The right child waits, the left one (the next node) goes on. A push happens once per level
							 * of an inner node, so the stack never holds more than the depth. */
							stack[top++] = node.right;
							current += 1;

							continue;
						}
					}

					if ( top == 0 )
					{
						return;
					}

					current = stack[--top];
				}
			}

			/**
			 * @brief Gives a contact on an INACTIVE edge or vertex of a triangle the triangle's face normal.
			 * @note The manifold is A (a body) ↔ B (this triangle), its normal from A to B. When every point lies on an
			 * inactive feature (an inactive edge, a vertex whose two edges are inactive, or the face) and the normal leans
			 * away from the face's, the normal becomes the face normal on the body's side and each depth is projected on
			 * it. Otherwise the manifold is left as it is.
			 * @pre triangleIndex < triangleCount().
			 * @param triangleIndex The triangle the manifold was generated with.
			 * @param manifold A reference to the manifold.
			 * @return bool True when the normal was replaced.
			 */
			bool
			correctInternalEdgeNormal (size_t triangleIndex, ContactManifold< precision_t > & manifold) const noexcept
			{
				return correctInternalEdgeNormal(m_triangles[triangleIndex], m_normals[triangleIndex], m_activeEdges[triangleIndex], manifold);
			}

			/**
			 * @brief The same correction for a triangle placed elsewhere (the world copy of a mesh triangle).
			 * @param triangle A reference to the triangle, in the manifold's space.
			 * @param faceNormal Its unit front normal in that space.
			 * @param activeEdges Its active edge flags (activeEdges()).
			 * @param manifold A reference to the manifold, A (a body) ↔ B (the triangle).
			 * @return bool True when the normal was replaced.
			 */
			static
			bool
			correctInternalEdgeNormal (const Triangle< precision_t > & triangle, const Vec3 & faceNormal, uint8_t activeEdges, ContactManifold< precision_t > & manifold) noexcept
			{
				if ( manifold.empty() )
				{
					return false;
				}

				const auto previous = manifold.normal();
				/* From the body to the triangle: against the face normal when the body is in front of it. */
				const Vec3 target = Vec3::dotProduct(previous, faceNormal) <= 0 ? -faceNormal : faceNormal;
				const auto cosine = Vec3::dotProduct(previous, target);

				if ( cosine >= AlignedCosine )
				{
					return false;
				}

				for ( const auto & point : manifold.points() )
				{
					if ( isActiveAt(triangle, activeEdges, point.position()) )
					{
						return false;
					}
				}

				const auto points = manifold.points();
				const auto projection = std::max(cosine, static_cast< precision_t >(0));

				manifold.clear();
				manifold.setNormal(target);

				for ( const auto & point : points )
				{
					static_cast< void >(manifold.addPoint({point.position(), point.depth() * projection, point.featureId()}));
				}

				return true;
			}

			/**
			 * @brief The same correction for ONE surface normal at a point of a triangle (a sweep's hit).
			 * @param triangle A reference to the triangle.
			 * @param faceNormal Its unit front normal.
			 * @param activeEdges Its active edge flags.
			 * @param point The contact point on (or next to) the triangle.
			 * @param normal A reference to the surface normal (pointing away from the triangle, towards the body).
			 * @return bool True when the normal was replaced by the face normal on its side.
			 */
			static
			bool
			correctInternalEdgeNormal (const Triangle< precision_t > & triangle, const Vec3 & faceNormal, uint8_t activeEdges, const Vec3 & point, Vec3 & normal) noexcept
			{
				const Vec3 target = Vec3::dotProduct(normal, faceNormal) >= 0 ? faceNormal : -faceNormal;

				if ( Vec3::dotProduct(normal, target) >= AlignedCosine || isActiveAt(triangle, activeEdges, point) )
				{
					return false;
				}

				normal = target;

				return true;
			}

		private:

			/** @brief The number of bins of the surface area heuristic. */
			static constexpr size_t Bins{12};

			/** @brief Within this cosine a normal already is the face normal. */
			static constexpr auto AlignedCosine = static_cast< precision_t >(1) - static_cast< precision_t >(1.0e-6);

			/** @brief Whether the triangle feature closest to a point is an ACTIVE edge or vertex. */
			[[nodiscard]]
			static
			bool
			isActiveAt (const Triangle< precision_t > & triangle, uint8_t activeEdges, const Vec3 & point) noexcept
			{
				TriangleDetail::Region region{TriangleDetail::Region::Face};

				static_cast< void >(TriangleDetail::closestPointOnTriangle(point, triangle, region));

				return isActiveRegion(region, activeEdges);
			}

			[[nodiscard]]
			static
			bool
			isFiniteVector (const Vec3 & vector) noexcept
			{
				return std::isfinite(vector[X]) && std::isfinite(vector[Y]) && std::isfinite(vector[Z]);
			}

			[[nodiscard]]
			static
			bool
			overlaps (const Vec3 & minimumA, const Vec3 & maximumA, const Vec3 & minimumB, const Vec3 & maximumB) noexcept
			{
				return minimumA[X] <= maximumB[X] && maximumA[X] >= minimumB[X] &&
					minimumA[Y] <= maximumB[Y] && maximumA[Y] >= minimumB[Y] &&
					minimumA[Z] <= maximumB[Z] && maximumA[Z] >= minimumB[Z];
			}

			static
			void
			triangleBounds (const Triangle< precision_t > & triangle, Vec3 & minimum, Vec3 & maximum) noexcept
			{
				for ( size_t axis = 0; axis < 3; ++axis )
				{
					minimum[axis] = std::min({triangle.pointA()[axis], triangle.pointB()[axis], triangle.pointC()[axis]});
					maximum[axis] = std::max({triangle.pointA()[axis], triangle.pointB()[axis], triangle.pointC()[axis]});
				}
			}

			[[nodiscard]]
			static
			precision_t
			halfArea (const Vec3 & minimum, const Vec3 & maximum) noexcept
			{
				const auto extent = maximum - minimum;

				return (extent[X] * extent[Y]) + (extent[Y] * extent[Z]) + (extent[Z] * extent[X]);
			}

			[[nodiscard]]
			static
			bool
			isActiveRegion (TriangleDetail::Region region, uint8_t flags) noexcept
			{
				using TriangleDetail::Region;

				switch ( region )
				{
					case Region::EdgeAB :
						return (flags & EdgeABActive) != 0;

					case Region::EdgeBC :
						return (flags & EdgeBCActive) != 0;

					case Region::EdgeAC :
						return (flags & EdgeCAActive) != 0;

					case Region::VertexA :
						return (flags & (EdgeABActive | EdgeCAActive)) != 0;

					case Region::VertexB :
						return (flags & (EdgeABActive | EdgeBCActive)) != 0;

					case Region::VertexC :
						return (flags & (EdgeBCActive | EdgeCAActive)) != 0;

					case Region::Face :
						return false;
				}

				return true;
			}

			/**
			 * @brief One identifier per vertex, the same for the vertices of the same position (the lowest index).
			 */
			[[nodiscard]]
			static
			std::vector< uint32_t >
			weldedIdentifiers (std::span< const Vec3 > vertices) noexcept
			{
				std::vector< uint32_t > order(vertices.size());

				for ( uint32_t index = 0; index < order.size(); ++index )
				{
					order[index] = index;
				}

				std::ranges::sort(order, [&vertices] (uint32_t lhs, uint32_t rhs) {
					const auto & a = vertices[lhs];
					const auto & b = vertices[rhs];

					if ( a[X] != b[X] )
					{
						return a[X] < b[X];
					}

					if ( a[Y] != b[Y] )
					{
						return a[Y] < b[Y];
					}

					if ( a[Z] != b[Z] )
					{
						return a[Z] < b[Z];
					}

					return lhs < rhs;
				});

				std::vector< uint32_t > identifiers(vertices.size());

				for ( size_t position = 0; position < order.size(); ++position )
				{
					const auto index = order[position];

					/* Exact components (Vector::operator== has a tolerance, the sort above has none). */
					const auto samePosition = position > 0 && vertices[order[position - 1]][X] == vertices[index][X] && vertices[order[position - 1]][Y] == vertices[index][Y] && vertices[order[position - 1]][Z] == vertices[index][Z];

					if ( samePosition )
					{
						identifiers[index] = identifiers[order[position - 1]];
					}
					else
					{
						identifiers[index] = index;
					}
				}

				return identifiers;
			}

			/**
			 * @brief Sets the active edge flags from the shared edges (by welded vertex).
			 */
			void
			computeActiveEdges (const std::vector< std::array< uint32_t, 3 > > & identifiers, precision_t activeEdgeCosine) noexcept
			{
				/* Opposite normals within this make a knife edge (two faces back to back): active. */
				constexpr auto KnifeCosine = static_cast< precision_t >(-0.999);
				/* A vertex this far above a plane makes the edge concave (in units of the mesh, after a unit normal). */
				constexpr auto ConcaveDistance = static_cast< precision_t >(1.0e-6);

				struct Edge final
				{
					uint64_t key;
					uint32_t triangle;
					uint32_t slot;
				};

				std::vector< Edge > edges;

				edges.reserve(identifiers.size() * 3);

				const auto keyOf = [] (uint32_t first, uint32_t second) {
					return (static_cast< uint64_t >(std::min(first, second)) << 32U) | std::max(first, second);
				};

				for ( uint32_t triangleIndex = 0; triangleIndex < identifiers.size(); ++triangleIndex )
				{
					const auto & [a, b, c] = identifiers[triangleIndex];

					/* Slot 0 = AB, 1 = BC, 2 = CA (the bit of its flag). */
					edges.push_back({keyOf(a, b), triangleIndex, 0});
					edges.push_back({keyOf(b, c), triangleIndex, 1});
					edges.push_back({keyOf(c, a), triangleIndex, 2});
				}

				std::ranges::sort(edges, [] (const Edge & lhs, const Edge & rhs) {
					return lhs.key != rhs.key ? lhs.key < rhs.key : lhs.triangle < rhs.triangle;
				});

				/* Every edge starts active; the flat or concave shared ones are cleared. */
				m_activeEdges.assign(identifiers.size(), static_cast< uint8_t >(EdgeABActive | EdgeBCActive | EdgeCAActive));

				const auto vertexOf = [this] (uint32_t triangleIndex, uint32_t slot) -> const Vec3 & {
					const auto & triangle = m_triangles[triangleIndex];

					switch ( slot )
					{
						case 0 :
							return triangle.pointA();

						case 1 :
							return triangle.pointB();

						default :
							return triangle.pointC();
					}
				};

				size_t begin = 0;

				while ( begin < edges.size() )
				{
					size_t end = begin + 1;

					while ( end < edges.size() && edges[end].key == edges[begin].key )
					{
						++end;
					}

					if ( end - begin == 2 )
					{
						const auto & edgeA = edges[begin];
						const auto & edgeB = edges[begin + 1];
						const auto & normalA = m_normals[edgeA.triangle];
						const auto & normalB = m_normals[edgeB.triangle];
						const auto cosine = Vec3::dotProduct(normalA, normalB);
						bool active = true;

						if ( cosine >= activeEdgeCosine )
						{
							active = false;
						}
						else if ( cosine > KnifeCosine )
						{
							/* Concave: the far vertex of the other triangle stands in front of this one's plane. */
							const auto & onEdge = vertexOf(edgeA.triangle, edgeA.slot);
							const auto & far = vertexOf(edgeB.triangle, (edgeB.slot + 2) % 3);

							active = Vec3::dotProduct(normalA, far - onEdge) <= ConcaveDistance;
						}

						if ( !active )
						{
							m_activeEdges[edgeA.triangle] &= static_cast< uint8_t >(~(1U << edgeA.slot));
							m_activeEdges[edgeB.triangle] &= static_cast< uint8_t >(~(1U << edgeB.slot));
						}
					}

					begin = end;
				}
			}

			/**
			 * @brief Builds the hierarchy (iterative: a right range waits on a stack while the left one is built next, so a
			 * node's left child follows it) and puts the triangles in its leaf order.
			 */
			void
			buildHierarchy () noexcept
			{
				const auto count = static_cast< uint32_t >(m_triangles.size());

				std::vector< Vec3 > centroids(count);
				std::vector< uint32_t > order(count);

				for ( uint32_t index = 0; index < count; ++index )
				{
					const auto & triangle = m_triangles[index];

					centroids[index] = (triangle.pointA() + triangle.pointB() + triangle.pointC()) * (static_cast< precision_t >(1) / static_cast< precision_t >(3));
					order[index] = index;
				}

				struct Task final
				{
					uint32_t parent;
					uint32_t begin;
					uint32_t end;
					uint32_t depth;
				};

				constexpr auto NoParent = std::numeric_limits< uint32_t >::max();

				std::vector< Task > tasks;

				tasks.push_back({NoParent, 0, count, 0});
				m_nodes.reserve((2 * count / MaxLeafTriangles) + 1);

				while ( !tasks.empty() )
				{
					auto task = tasks.back();

					tasks.pop_back();

					while ( true )
					{
						const auto nodeIndex = static_cast< uint32_t >(m_nodes.size());

						m_nodes.emplace_back();

						if ( task.parent != NoParent )
						{
							m_nodes[task.parent].right = nodeIndex;
						}

						Vec3 lowest;
						Vec3 highest;
						Vec3 centroidLowest;
						Vec3 centroidHighest;

						for ( uint32_t position = task.begin; position < task.end; ++position )
						{
							Vec3 triangleLowest;
							Vec3 triangleHighest;

							triangleBounds(m_triangles[order[position]], triangleLowest, triangleHighest);

							const auto & centroid = centroids[order[position]];

							for ( size_t axis = 0; axis < 3; ++axis )
							{
								const bool first = position == task.begin;

								lowest[axis] = first ? triangleLowest[axis] : std::min(lowest[axis], triangleLowest[axis]);
								highest[axis] = first ? triangleHighest[axis] : std::max(highest[axis], triangleHighest[axis]);
								centroidLowest[axis] = first ? centroid[axis] : std::min(centroidLowest[axis], centroid[axis]);
								centroidHighest[axis] = first ? centroid[axis] : std::max(centroidHighest[axis], centroid[axis]);
							}
						}

						m_nodes[nodeIndex].minimum = lowest;
						m_nodes[nodeIndex].maximum = highest;

						const auto size = task.end - task.begin;
						const auto split = size <= MaxLeafTriangles || task.depth + 1 >= MaxDepth ? task.begin : this->splitPosition(centroids, order, task.begin, task.end, centroidLowest, centroidHighest);

						if ( split == task.begin )
						{
							m_nodes[nodeIndex].first = task.begin;
							m_nodes[nodeIndex].count = size;

							break;
						}

						tasks.push_back({nodeIndex, split, task.end, task.depth + 1});
						task = {NoParent, task.begin, split, task.depth + 1};
					}
				}

				/* The triangles in the leaves' order: a leaf's range indexes them directly. */
				std::vector< Triangle< precision_t > > triangles;
				std::vector< Vec3 > normals;
				std::vector< uint8_t > activeEdges;

				triangles.reserve(count);
				normals.reserve(count);
				activeEdges.reserve(count);

				for ( const auto index : order )
				{
					triangles.push_back(m_triangles[index]);
					normals.push_back(m_normals[index]);
					activeEdges.push_back(m_activeEdges[index]);
				}

				m_triangles = std::move(triangles);
				m_normals = std::move(normals);
				m_activeEdges = std::move(activeEdges);
				m_minimum = m_nodes.front().minimum;
				m_maximum = m_nodes.front().maximum;
			}

			/**
			 * @brief Splits a range by the surface area heuristic on its longest centroid axis; falls back to the median
			 * when no bin boundary separates it. Answers `begin` to make a leaf (the split costs more than the leaf).
			 */
			[[nodiscard]]
			uint32_t
			splitPosition (const std::vector< Vec3 > & centroids, std::vector< uint32_t > & order, uint32_t begin, uint32_t end, const Vec3 & centroidLowest, const Vec3 & centroidHighest) const noexcept
			{
				const auto extent = centroidHighest - centroidLowest;
				size_t axis = 0;

				if ( extent[Y] > extent[axis] )
				{
					axis = 1;
				}

				if ( extent[Z] > extent[axis] )
				{
					axis = 2;
				}

				const auto middle = begin + ((end - begin) / 2);

				/* All the centroids in one point: the median of the order (the triangles overlap). */
				if ( !(extent[axis] > 0) )
				{
					return middle;
				}

				struct Bin final
				{
					Vec3 lowest;
					Vec3 highest;
					uint32_t count{0};
				};

				std::array< Bin, Bins > bins{};
				const auto scale = static_cast< precision_t >(Bins) / extent[axis];
				const auto binOf = [&] (uint32_t triangleIndex) {
					const auto bin = static_cast< size_t >((centroids[triangleIndex][axis] - centroidLowest[axis]) * scale);

					return std::min(bin, Bins - 1);
				};

				for ( uint32_t position = begin; position < end; ++position )
				{
					auto & bin = bins[binOf(order[position])];
					Vec3 triangleLowest;
					Vec3 triangleHighest;

					triangleBounds(m_triangles[order[position]], triangleLowest, triangleHighest);

					for ( size_t component = 0; component < 3; ++component )
					{
						bin.lowest[component] = bin.count == 0 ? triangleLowest[component] : std::min(bin.lowest[component], triangleLowest[component]);
						bin.highest[component] = bin.count == 0 ? triangleHighest[component] : std::max(bin.highest[component], triangleHighest[component]);
					}

					++bin.count;
				}

				/* The cost of each boundary: the areas and counts on its two sides. */
				std::array< precision_t, Bins - 1 > leftCost{};
				Vec3 lowest;
				Vec3 highest;
				uint32_t leftCount = 0;

				for ( size_t boundary = 0; boundary + 1 < Bins; ++boundary )
				{
					const auto & bin = bins[boundary];

					if ( bin.count > 0 )
					{
						for ( size_t component = 0; component < 3; ++component )
						{
							lowest[component] = leftCount == 0 ? bin.lowest[component] : std::min(lowest[component], bin.lowest[component]);
							highest[component] = leftCount == 0 ? bin.highest[component] : std::max(highest[component], bin.highest[component]);
						}

						leftCount += bin.count;
					}

					leftCost[boundary] = leftCount == 0 ? 0 : halfArea(lowest, highest) * static_cast< precision_t >(leftCount);
				}

				auto bestCost = std::numeric_limits< precision_t >::max();
				size_t bestBoundary = Bins;
				uint32_t rightCount = 0;

				for ( size_t boundary = Bins - 1; boundary > 0; --boundary )
				{
					const auto & bin = bins[boundary];

					if ( bin.count > 0 )
					{
						for ( size_t component = 0; component < 3; ++component )
						{
							lowest[component] = rightCount == 0 ? bin.lowest[component] : std::min(lowest[component], bin.lowest[component]);
							highest[component] = rightCount == 0 ? bin.highest[component] : std::max(highest[component], bin.highest[component]);
						}

						rightCount += bin.count;
					}

					const auto leftTriangles = (end - begin) - rightCount;

					if ( rightCount == 0 || leftTriangles == 0 )
					{
						continue;
					}

					const auto cost = leftCost[boundary - 1] + (halfArea(lowest, highest) * static_cast< precision_t >(rightCount));

					if ( cost < bestCost )
					{
						bestCost = cost;
						bestBoundary = boundary;
					}
				}

				if ( bestBoundary == Bins )
				{
					std::nth_element(order.begin() + begin, order.begin() + middle, order.begin() + end, [&centroids, axis] (uint32_t lhs, uint32_t rhs) {
						return centroids[lhs][axis] < centroids[rhs][axis];
					});

					return middle;
				}

				const auto partition = std::stable_partition(order.begin() + begin, order.begin() + end, [&binOf, bestBoundary] (uint32_t triangleIndex) {
					return binOf(triangleIndex) < bestBoundary;
				});

				return static_cast< uint32_t >(partition - order.begin());
			}

			std::vector< Triangle< precision_t > > m_triangles;
			std::vector< Vec3 > m_normals;
			std::vector< uint8_t > m_activeEdges;
			std::vector< Node > m_nodes;
			Vec3 m_minimum;
			Vec3 m_maximum;
	};
}
