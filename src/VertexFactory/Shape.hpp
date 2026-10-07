/*
 * src/VertexFactory/Shape.hpp
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

/* Project configuration. */
#include "emeraude_base_config.hpp"

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <functional>
#include <iostream>
#include <limits>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

/* Local inclusions for usages. */
#include "Math/Matrix.hpp"
#include "Math/Space3D/AACuboid.hpp"
#include "Math/Space3D/Sphere.hpp"
#include "Math/Vector.hpp"
#include "PixelFactory/Color.hpp"
#include "ShapeEdge.hpp"
#include "ShapeTriangle.hpp"
#include "ShapeVertex.hpp"
#include "TextureCoordinates.hpp"
#include "Types.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief A boundary loop representing an ordered sequence of vertex indices forming a hole in a shape.
	 * @tparam index_data_t The precision type of index data. Default uint32_t.
	 */
	template< typename index_data_t = uint32_t >
	requires (std::is_unsigned_v< index_data_t >)
	struct BoundaryLoop final
	{
		std::vector< index_data_t > vertexIndices;
	};

	/**
	 * @brief The shape class for defining a complete geometry.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 * @tparam index_data_t The precision type of index data. Default uint32_t.
	 */
	template< typename vertex_data_t = float, typename index_data_t = uint32_t >
	requires (std::is_floating_point_v< vertex_data_t > && std::is_unsigned_v< index_data_t > )
	class Shape final
	{
		public:

			/**
			 * @brief Constructs a default shape.
			 */
			Shape () noexcept = default;

			/**
			 * @brief Constructs a shape and reserve space from triangle count.
			 * @param triangleCount The possible number of triangles to reserve.
			 */
			explicit
			Shape (index_data_t triangleCount) noexcept
			{
				this->reserveData(triangleCount);
			}

			/**
			 * @brief Constructs a shape and reserve space from vertex attributes count.
			 * @param positionsCount The possible number of positions to reserve.
			 * @param vertexColorsCount The possible number of color vertices to reserve.
			 * @param facesCount The possible number of faces to reserve.
			 * @param edgesCount The possible number of edges to reserve. Default 0.
			 */
			Shape (index_data_t positionsCount, index_data_t vertexColorsCount, index_data_t facesCount, index_data_t edgesCount = 0) noexcept
			{
				this->resizeData(positionsCount, vertexColorsCount, facesCount, edgesCount);
			}

			/**
			 * @brief Reserves data for geometry construction to avoid multiple re-allocations.
			 * @param triangleCount The possible number of triangles to reserve.
			 * @return void
			 */
			void
			reserveData (index_data_t triangleCount) noexcept
			{
				if ( triangleCount == 0 )
				{
					std::cerr << "Shape::reserveData(), reserving data with triangle count equals to zero !" "\n";

					return;
				}

				m_vertices.reserve(triangleCount * 3);
				m_vertexColors.reserve(triangleCount * 3);
				m_triangles.reserve(triangleCount);
				m_edges.reserve(triangleCount * 3);
			}

			/**
			 * @brief Reserves data vectors for geometry construction to avoid multiple re-allocations. Finer version.
			 * @param positionsCount The possible number of positions to reserve.
			 * @param vertexColorsCount The possible number of color vertices to reserve.
			 * @param facesCount The possible number of faces to reserve.
			 * @param edgesCount The possible number of edges to reserve. Default 0.
			 * @return void
			 */
			void
			reserveData (index_data_t positionsCount, index_data_t vertexColorsCount, index_data_t facesCount, index_data_t edgesCount = 0) noexcept
			{
				auto somethingReserved = false;

				if ( positionsCount > 0 )
				{
					m_vertices.reserve(positionsCount);

					somethingReserved = true;
				}

				if ( vertexColorsCount > 0 )
				{
					m_vertexColors.reserve(vertexColorsCount);

					somethingReserved = true;
				}

				if ( facesCount > 0 )
				{
					m_triangles.reserve(facesCount);

					somethingReserved = true;
				}

				if ( edgesCount > 0 )
				{
					m_edges.reserve(edgesCount);

					somethingReserved = true;
				}

				if ( !somethingReserved )
				{
					std::cerr << "Shape::reserveData(), trying to reserve data with all parameters set to zero !" "\n";
				}
			}

			/**
			 * @brief Resizes data vectors for geometry construction to avoid multiple re-allocations. Finer version.
			 * @param positionsCount The possible number of positions to reserve.
			 * @param vertexColorsCount The possible number of color vertices to reserve.
			 * @param facesCount The possible number of faces to reserve.
			 * @param edgesCount The possible number of edges to reserve. Default 0.
			 * @return void
			 */
			void
			resizeData (index_data_t positionsCount, index_data_t vertexColorsCount, index_data_t facesCount, index_data_t edgesCount = 0) noexcept
			{
				m_vertices.resize(positionsCount);
				m_vertexColors.resize(vertexColorsCount);
				m_triangles.resize(facesCount);
				m_edges.resize(edgesCount);
				m_unpairedEdges.clear();
				m_vertexIndex.clear();
				m_vertexColorIndex.clear();
				m_constructionIndexesReleased = false;
			}

			/**
			 * @brief Clears geometry data.
			 * @return void
			 */
			void
			clear () noexcept
			{
				m_vertices.clear();
				m_vertexColors.clear();
				m_triangles.clear();
				m_edges.clear();
				m_unpairedEdges.clear();
				m_vertexIndex.clear();
				m_vertexColorIndex.clear();
				m_constructionIndexesReleased = false;
				m_boundaryLoops.clear();
				m_boundaryLoopsAnalyzed = false;
				m_groups.clear();
				m_groups.resize(1);
				m_boundingBox.reset();
				m_boundingSphere.reset();
				m_farthestDistance = 0;
				m_textureCoordinatesDeclared = false;
				m_normalsDeclared = false;
				m_computeEdges = false;
			}

			/**
			 * @brief Returns the number of vertices.
			 * @return index_data_t
			 */
			[[nodiscard]]
			index_data_t
			vertexCount () const noexcept
			{
				return static_cast< index_data_t >(m_vertices.size());
			}

			/**
			 * @brief Gives access to the vertices list.
			 * @return const std::vector< ShapeVertex< vertex_data_t > > &
			 */
			[[nodiscard]]
			const std::vector< ShapeVertex< vertex_data_t > > &
			vertices () const noexcept
			{
				return m_vertices;
			}

			/**
			 * @brief Gives mutable access to the vertices list.
			 * @return std::vector< ShapeVertex< vertex_data_t > > &
			 */
			[[nodiscard]]
			std::vector< ShapeVertex< vertex_data_t > > &
			vertices () noexcept
			{
				return m_vertices;
			}

			/**
			 * @brief Gives access to the vertex colors list.
			 * @return const std::vector< Math::Vector< 4, vertex_data_t > > &
			 */
			[[nodiscard]]
			const std::vector< Math::Vector< 4, vertex_data_t > > &
			vertexColors () const noexcept
			{
				return m_vertexColors;
			}

			/**
			 * @brief Gives mutable access to the vertex colors list.
			 * @return std::vector< Math::Vector< 4, vertex_data_t > > &
			 */
			[[nodiscard]]
			std::vector< Math::Vector< 4, vertex_data_t > > &
			vertexColors () noexcept
			{
				return m_vertexColors;
			}

			/**
			 * @brief Gives access to the triangle list.
			 * @return const std::vector< ShapeTriangle< vertex_data_t > > &
			 */
			[[nodiscard]]
			const std::vector< ShapeTriangle< vertex_data_t > > &
			triangles () const noexcept
			{
				return m_triangles;
			}

			/**
			 * @brief Gives mutable access to the triangle list.
			 * @return std::vector< ShapeTriangle< vertex_data_t > > &
			 */
			[[nodiscard]]
			std::vector< ShapeTriangle< vertex_data_t > > &
			triangles () noexcept
			{
				return m_triangles;
			}

			/**
			 * @brief Returns a vertex from the list.
			 * @param index The index of the vertex in the list. You should get it from a ShapeTriangle.
			 * @return const ShapeVertex< vertex_data_t > &
			 */
			[[nodiscard]]
			const ShapeVertex< vertex_data_t > &
			vertex (index_data_t index) const noexcept
			{
				return m_vertices[index];
			}

			/**
			 * @brief Returns a vertex color from the list.
			 * @param index The index of the vertex color in the list. You should get it from a ShapeTriangle.
			 * @return const Math::Vector< 4, vertex_data_t > &
			 */
			[[nodiscard]]
			const Math::Vector< 4, vertex_data_t > &
			vertexColor (index_data_t index) const noexcept
			{
				return m_vertexColors[index];
			}

			/**
			 * @brief Gives access to the edge list.
			 * @return const std::vector< ShapeEdge< index_data_t > > &
			 */
			[[nodiscard]]
			const std::vector< ShapeEdge< index_data_t > > &
			edges () const noexcept
			{
				return m_edges;
			}

			/**
			 * @brief Gives access to the boundary loops list.
			 * @return const std::vector< BoundaryLoop< index_data_t > > &
			 */
			[[nodiscard]]
			const std::vector< BoundaryLoop< index_data_t > > &
			boundaryLoops () const noexcept
			{
				return m_boundaryLoops;
			}

			/**
			 * @brief Gives mutable access to the boundary loops list.
			 * @return std::vector< BoundaryLoop< index_data_t > > &
			 */
			[[nodiscard]]
			std::vector< BoundaryLoop< index_data_t > > &
			boundaryLoops () noexcept
			{
				return m_boundaryLoops;
			}

			/**
			 * @brief Clears all boundary loops.
			 * @return void
			 */
			void
			clearBoundaryLoops () noexcept
			{
				m_boundaryLoops.clear();
				m_boundaryLoopsAnalyzed = false;
			}

			/**
			 * @brief Returns whether the surface has known openings (boundary loops).
			 * @note Returns false if boundary loops have never been analyzed or set.
			 * Use analyzeBoundaryLoops() first, or check boundaryLoopsAnalyzed() to know
			 * if the result is meaningful.
			 * @return bool True if the shape has at least one boundary loop.
			 */
			[[nodiscard]]
			bool
			isSurfaceOpened () const noexcept
			{
				return !m_boundaryLoops.empty();
			}

			/**
			 * @brief Returns whether boundary loops have been analyzed or explicitly set.
			 * @note A shape loaded from a file will return false until boundary loops are
			 * analyzed or set by a processor (e.g. ShapeSplitter).
			 * @return bool
			 */
			[[nodiscard]]
			bool
			boundaryLoopsAnalyzed () const noexcept
			{
				return m_boundaryLoopsAnalyzed;
			}

			/**
			 * @brief Marks the boundary loops as having been analyzed.
			 * @note Called automatically by analyzeBoundaryLoops() and by ShapeSplitter.
			 * @return void
			 */
			void
			setBoundaryLoopsAnalyzed () noexcept
			{
				m_boundaryLoopsAnalyzed = true;
			}

			/**
			 * @brief Gives access to the bounding box.
			 * @return const Math::Space3D::AACuboid< vertex_data_t > &
			 */
			[[nodiscard]]
			const Math::Space3D::AACuboid< vertex_data_t > &
			boundingBox () const noexcept
			{
				return m_boundingBox;
			}

			/**
			 * @brief Gives access to the bounding sphere.
			 * @return const Math::Space3D::Sphere< vertex_data_t > &
			 */
			[[nodiscard]]
			const Math::Space3D::Sphere< vertex_data_t > &
			boundingSphere () const noexcept
			{
				return m_boundingSphere;
			}

			/**
			 * @brief Returns the bytes the shape holds in memory: the object itself, the capacity of every
			 * storage and an estimate of its construction-time hash indexes.
			 * @note The hash indexes are estimated (one pointer per bucket, one node per element: the value, a
			 * link and a cached hash); their exact layout is the standard library's.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			memoryOccupied () const noexcept
			{
				size_t bytes = sizeof(*this);

				bytes += m_groups.capacity() * sizeof(typename decltype(m_groups)::value_type);
				bytes += m_vertices.capacity() * sizeof(typename decltype(m_vertices)::value_type);
				bytes += m_vertexColors.capacity() * sizeof(typename decltype(m_vertexColors)::value_type);
				bytes += m_triangles.capacity() * sizeof(typename decltype(m_triangles)::value_type);
				bytes += m_edges.capacity() * sizeof(typename decltype(m_edges)::value_type);
				bytes += hashIndexBytes(m_unpairedEdges);
				bytes += hashIndexBytes(m_vertexIndex);
				bytes += hashIndexBytes(m_vertexColorIndex);
				bytes += m_boundaryLoops.capacity() * sizeof(typename decltype(m_boundaryLoops)::value_type);

				for ( const auto & loop : m_boundaryLoops )
				{
					bytes += loop.vertexIndices.capacity() * sizeof(index_data_t);
				}

				return bytes;
			}

			/**
			 * @brief Frees the construction-time indexes (the vertex, vertex colour and edge merge tables).
			 * @note Call it once the shape is final, typically before its GPU upload: those indexes only
			 * serve addVertex(), addVertexColor() and addEdge(), yet they can outweigh the edges
			 * themselves (citadel, 2026-10-03: 360 MiB of 1359). A later edit rebuilds them first, from
			 * the stored data, before its lookup (restoreConstructionIndexes()).
			 * @note ⚠️ The rebuilt vertex index holds EVERY stored vertex, those saved without merging
			 * (saveVertex()) included, and uses the merge tolerance current at the rebuild.
			 * @return void
			 */
			void
			releaseConstructionIndexes () noexcept
			{
				decltype(m_unpairedEdges){}.swap(m_unpairedEdges);
				decltype(m_vertexIndex){}.swap(m_vertexIndex);
				decltype(m_vertexColorIndex){}.swap(m_vertexColorIndex);

				m_constructionIndexesReleased = true;
			}

			/**
			 * @brief Returns whether the construction-time indexes were released (releaseConstructionIndexes()).
			 * @return bool
			 */
			[[nodiscard]]
			bool
			constructionIndexesReleased () const noexcept
			{
				return m_constructionIndexesReleased;
			}

			/**
			 * @brief Returns whether the geometry is composed of groups.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			hasGroups () const noexcept
			{
				return m_groups.size() > 1;
			}

			/**
			 * @brief Returns the number of groups the geometry is composed of.
			 * @return index_data_t
			 */
			[[nodiscard]]
			index_data_t
			groupCount () const noexcept
			{
				return static_cast< index_data_t >(m_groups.size());
			}

			/**
			 * @brief Returns the group list the geometry is composed of.
			 * @return const std::vector< std::pair< index_data_t, index_data_t > > &
			 */
			[[nodiscard]]
			const std::vector< std::pair< index_data_t, index_data_t > > &
			groups () const noexcept
			{
				return m_groups;
			}

			/**
			 * @brief Gives mutable access to the group list.
			 * @return std::vector< std::pair< index_data_t, index_data_t > > &
			 */
			[[nodiscard]]
			std::vector< std::pair< index_data_t, index_data_t > > &
			groups () noexcept
			{
				return m_groups;
			}

			/**
			 * @brief Returns the average center of the geometry.
			 * @return const Math::Vector< 3, vertex_data_t > &
			 */
			[[nodiscard]]
			const Math::Vector< 3, vertex_data_t > &
			centroid () const noexcept
			{
				return m_boundingSphere.position();
			}

			/**
			 * @brief Returns the distance of the farthest vertex from the origin (0, 0, 0).
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			farthestDistance () const noexcept
			{
				return m_farthestDistance;
			}

			/**
			 * @brief Computes the total surface area of the geometry.
			 * @return vertex_data_t The surface area in squared units.
			 */
			[[nodiscard]]
			vertex_data_t
			surfaceArea () const noexcept
			{
				vertex_data_t area = 0;

				for ( const auto & tri : m_triangles )
				{
					const auto & a = m_vertices[tri.vertexIndex(0)].position();
					const auto & b = m_vertices[tri.vertexIndex(1)].position();
					const auto & c = m_vertices[tri.vertexIndex(2)].position();

					area += Math::Vector< 3, vertex_data_t >::crossProduct(b - a, c - a).length() * static_cast< vertex_data_t >(0.5);
				}

				return area;
			}

			/**
			 * @brief Computes the volume of the geometry using the divergence theorem.
			 * @note Only meaningful for closed (watertight) surfaces. Returns 0 if
			 * boundary loops are known to exist (open surface).
			 * @return vertex_data_t The volume in cubic units.
			 */
			[[nodiscard]]
			vertex_data_t
			volume () const noexcept
			{
				if ( m_boundaryLoopsAnalyzed && !m_boundaryLoops.empty() )
				{
					return 0;
				}

				vertex_data_t vol = 0;

				for ( const auto & tri : m_triangles )
				{
					const auto & a = m_vertices[tri.vertexIndex(0)].position();
					const auto & b = m_vertices[tri.vertexIndex(1)].position();
					const auto & c = m_vertices[tri.vertexIndex(2)].position();

					vol += Math::Vector< 3, vertex_data_t >::dotProduct(a, Math::Vector< 3, vertex_data_t >::crossProduct(b, c));
				}

				return std::abs(vol) / static_cast< vertex_data_t >(6);
			}

			/**
			 * @brief Checks if the geometry is valid.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isValid () const noexcept
			{
				return !m_triangles.empty();
			}

			/**
			 * @brief Checks if the geometry is empty.
			 * @note Inverse of Shape::isValid(). Provided to satisfy C++ conventions.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			empty () const noexcept
			{
				return m_triangles.empty();
			}

			/**
			 * @brief Returns whether the geometry is an open shape or not.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isOpen () const noexcept
			{
				/* NOTE: if only one edge is not shared, then the geometry is open. */
				return std::ranges::any_of(m_edges, [] (const auto & edge) {
					return !edge.isShared();
				});
			}

			/**
			 * @brief Returns whether texture coordinates are available.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isTextureCoordinatesAvailable () const noexcept
			{
				return m_textureCoordinatesDeclared;
			}

			/**
			 * @brief Returns whether normals are available.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isNormalsAvailable () const noexcept
			{
				return m_normalsDeclared;
			}

			/**
			 * @brief Declares that normals are available.
			 * @note Use this after loading geometry with pre-existing normals.
			 * @return void
			 */
			void
			declareNormalsAvailable () noexcept
			{
				m_normalsDeclared = true;
			}

			/**
			 * @brief Declares that texture coordinates are available.
			 * @note Use this after generating UVs externally (e.g., UV unwrapping).
			 * @return void
			 */
			void
			declareTextureCoordinatesAvailable () noexcept
			{
				m_textureCoordinatesDeclared = true;
			}

			/**
			 * @brief Returns whether vertex color is available.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isVertexColorAvailable () const noexcept
			{
				return !m_vertexColors.empty();
			}

			/**
			 * @brief Generates automatically texture coordinates for the geometry.
			 * @param uScale U component scaling value. Default 1.
			 * @param vScale V component scaling value. Default 1.
			 * @param wScale W component scaling value. Default 1.
			 * @return bool
			 */
			bool
			generateTextureCoordinates (vertex_data_t uScale = 1, vertex_data_t vScale = 1, vertex_data_t wScale = 1) noexcept
			{
				if ( m_triangles.empty() || m_vertices.empty() )
				{
					std::cerr << "Shape::generateTextureCoordinates(), geometry data is empty !" "\n";

					return false;
				}

				const auto trianglesCount = static_cast< int64_t >(m_triangles.size());
				const auto & trianglesRef = m_triangles;
				auto & verticesRef = m_vertices;

				#pragma omp parallel for default(none) shared(uScale, vScale, wScale, trianglesCount, trianglesRef, verticesRef)
				for ( int64_t triangleIndex = 0; triangleIndex < trianglesCount; ++triangleIndex )
				{
					const auto & triangle = trianglesRef[triangleIndex];

					for ( index_data_t vertexIndex = 0; vertexIndex < 3; ++vertexIndex )
					{
						auto & currentVertex = verticesRef[triangle.vertexIndex(vertexIndex)];

						auto textureCoordinates = TextureCoordinates::generateCubicCoordinates(currentVertex.position(), currentVertex.normal());
						textureCoordinates[Math::U] *= uScale;
						textureCoordinates[Math::V] *= vScale;
						textureCoordinates[Math::W] *= wScale;

						currentVertex.setTextureCoordinates(textureCoordinates);
					}
				}

				m_textureCoordinatesDeclared = true;

				return true;
			}

			/**
			 * @brief Computes normal vector for every triangle.
			 * @param invert Inverts the computed normals. This is needed when an odd number
			 * of axis reflections have been applied to the vertex positions (e.g., flipYAxis),
			 * which reverses the cross product direction without changing the triangle winding.
			 * @return bool
			 */
			bool
			computeTriangleNormal (bool invert = false) noexcept
			{
				if ( m_triangles.empty() || m_vertices.empty() )
				{
					std::cerr << "Shape::computeTriangleNormal(), geometry data is empty !" "\n";

					return false;
				}

				const auto trianglesCount = static_cast< int64_t >(m_triangles.size());
				auto & trianglesRef = m_triangles;
				const auto & verticesRef = m_vertices;

				#pragma omp parallel for default(none) shared(trianglesCount, trianglesRef, verticesRef, invert)
				for ( int64_t triangleIndex = 0; triangleIndex < trianglesCount; ++triangleIndex )
				{
					auto & triangle = trianglesRef[triangleIndex];

					const auto & vertexA = verticesRef[triangle.vertexIndex(0)];
					const auto & vertexB = verticesRef[triangle.vertexIndex(1)];
					const auto & vertexC = verticesRef[triangle.vertexIndex(2)];

					/* Compute the surface normal. */
					const auto normal = Math::Vector< 3, vertex_data_t >::normal(
						vertexA.position(),
						vertexB.position(),
						vertexC.position()
					);

					triangle.setSurfaceNormal(invert ? -normal : normal);
				}

				return true;
			}

			/**
			 * @brief Computes tangent vector for every triangle.
			 * @warning The texture coordinates attributes are requested.
			 * @return bool
			 */
			bool
			computeTriangleTangent () noexcept
			{
				if ( m_triangles.empty() || m_vertices.empty() )
				{
					std::cerr << "Shape::computeTriangleTangent(), geometry data is empty !" "\n";

					return false;
				}

				if ( !this->isTextureCoordinatesAvailable() )
				{
					std::cerr << "Shape::computeTriangleTangent(), there is no texture coordinates !" "\n";

					return false;
				}

				const auto trianglesCount = static_cast< int64_t >(m_triangles.size());
				auto & trianglesRef = m_triangles;
				const auto & verticesRef = m_vertices;

				#pragma omp parallel for default(none) shared(trianglesCount, trianglesRef, verticesRef)
				for ( int64_t triangleIndex = 0; triangleIndex < trianglesCount; ++triangleIndex )
				{
					auto & triangle = trianglesRef[triangleIndex];

					const auto & vertexA = verticesRef[triangle.vertexIndex(0)];
					const auto & vertexB = verticesRef[triangle.vertexIndex(1)];
					const auto & vertexC = verticesRef[triangle.vertexIndex(2)];

					/* Compute the surface tangent frame (the tangent and its handedness). */
					const auto [tangent, handedness] = triangleTangentFrame(vertexA, vertexB, vertexC);

					triangle.setSurfaceTangent(tangent);
					triangle.setSurfaceTangentHandedness(handedness);
				}

				return true;
			}

			/**
			 * @brief Computes tangent vector for every triangle from edge projection.
			 * @note This method derives the tangent by projecting a triangle edge onto
			 * the plane defined by the surface normal (Gram-Schmidt). It does not require
			 * texture coordinates, making it suitable for FaceMode::V and FaceMode::V_VN.
			 * @warning Triangle surface normals must be computed first.
			 * @return bool
			 */
			bool
			computeTriangleTangentFromEdge () noexcept
			{
				if ( m_triangles.empty() || m_vertices.empty() )
				{
					std::cerr << "Shape::computeTriangleTangentFromEdge(), geometry data is empty !" "\n";

					return false;
				}

				const auto trianglesCount = static_cast< int64_t >(m_triangles.size());
				auto & trianglesRef = m_triangles;
				const auto & verticesRef = m_vertices;

				#pragma omp parallel for default(none) shared(trianglesCount, trianglesRef, verticesRef)
				for ( int64_t triangleIndex = 0; triangleIndex < trianglesCount; ++triangleIndex )
				{
					auto & triangle = trianglesRef[triangleIndex];

					const auto & vertexA = verticesRef[triangle.vertexIndex(0)];
					const auto & vertexB = verticesRef[triangle.vertexIndex(1)];

					const auto edge = vertexB.position() - vertexA.position();
					const auto & normal = triangle.surfaceNormal();

					/* Project the edge onto the plane defined by the surface normal (Gram-Schmidt). */
					const auto projected = edge - ((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((normal * Math::Vector< 3, vertex_data_t >::dotProduct(edge, normal)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));
					const auto length = projected.length();

					/* No texture coordinates: no mirrored island either. */
					triangle.setSurfaceTangentHandedness(1);

					if ( length > static_cast< vertex_data_t >(1e-6) )
					{
						triangle.setSurfaceTangent(projected / length);
					}
					else
					{
						/* Degenerate triangle: fall back to an arbitrary perpendicular. */
						const Math::Vector< 3, vertex_data_t > reference =
							std::abs(normal[Math::Y]) < static_cast< vertex_data_t >(0.999)
								? Math::Vector< 3, vertex_data_t >{0, 1, 0}
								: Math::Vector< 3, vertex_data_t >{1, 0, 0};

						triangle.setSurfaceTangent(
							Math::Vector< 3, vertex_data_t >::crossProduct(normal, reference).normalize());
					}
				}

				return true;
			}

			/**
			 * @brief Computes normal and tangent vectors for every triangle.
			 * @note If normals are present, use Shape::computeTriangleTangent() instead.
			 * @warning The texture coordinates attributes are requested.
			 * @return bool
			 */
			bool
			computeTriangleTBNSpace () noexcept
			{
				if ( m_triangles.empty() || m_vertices.empty() )
				{
					std::cerr << "Shape::computeTriangleTBNSpace(), geometry data is empty !" "\n";

					return false;
				}

				if ( !this->isTextureCoordinatesAvailable() )
				{
					std::cerr << "Shape::computeTriangleTBNSpace(), there is no texture coordinates !" "\n";

					return false;
				}

				const auto trianglesCount = static_cast< int64_t >(m_triangles.size());
				auto & trianglesRef = m_triangles;
				const auto & verticesRef = m_vertices;

#pragma omp parallel for default(none) shared(trianglesCount, trianglesRef, verticesRef)
				for ( int64_t triangleIndex = 0; triangleIndex < trianglesCount; ++triangleIndex )
				{
					auto & triangle = trianglesRef[triangleIndex];
					const auto & vertexA = verticesRef[triangle.vertexIndex(0)];
					const auto & vertexB = verticesRef[triangle.vertexIndex(1)];
					const auto & vertexC = verticesRef[triangle.vertexIndex(2)];

					/* Compute the surface tangent frame (the tangent and its handedness). */
					const auto [tangent, handedness] = triangleTangentFrame(vertexA, vertexB, vertexC);

					triangle.setSurfaceTangent(tangent);
					triangle.setSurfaceTangentHandedness(handedness);

					/* Compute the surface normal. */
					const auto normal = Math::Vector< 3, vertex_data_t >::normal(
						vertexA.position(),
						vertexB.position(),
						vertexC.position()
					);

					triangle.setSurfaceNormal(normal);
				}

				return true;
			}

			/**
			 * @brief Computes a normal vector for every vertex.
			 * @warning Geometry must have normal vectors computed for every triangle.
			 * @note Uses vertex-to-triangle adjacency table for O(V+T) performance.
			 * @return bool
			 */
			bool
			computeVertexNormal () noexcept
			{
				if ( m_triangles.empty() || m_vertices.empty() )
				{
					std::cerr << "Shape::computeVertexNormal(), geometry is empty !" "\n";

					return false;
				}

				/* Build adjacency table: vertex → adjacent triangle indices. O(T) */
				const auto adjacency = this->buildVertexToTriangleAdjacency();

				for ( size_t globalVertexIndex = 0; globalVertexIndex < m_vertices.size(); ++globalVertexIndex )
				{
					Math::Vector< 3, vertex_data_t > normal;

					for ( const auto triIndex : adjacency[globalVertexIndex] )
					{
						normal += m_triangles[triIndex].surfaceNormal();
					}

					m_vertices[globalVertexIndex].setNormal(normal.normalize());
				}

				m_normalsDeclared = true;

				return true;
			}

			/**
			 * @brief Computes tangent vector for every vertex.
			 * @warning Geometry must have tangent vectors computed for every triangle.
			 * @note Uses vertex-to-triangle adjacency table for O(V+T) performance.
			 * @return bool
			 */
			bool
			computeVertexTangent () noexcept
			{
				if ( m_triangles.empty() || m_vertices.empty() )
				{
					std::cerr << "Shape::computeVertexTangent(), geometry is empty !" "\n";

					return false;
				}

				/* Build adjacency table: vertex → adjacent triangle indices. O(T) */
				const auto adjacency = this->buildVertexToTriangleAdjacency();

				for ( size_t globalVertexIndex = 0; globalVertexIndex < m_vertices.size(); ++globalVertexIndex )
				{
					const auto handedness = this->vertexTangentHandedness(adjacency[globalVertexIndex]);
					Math::Vector< 3, vertex_data_t > tangent;

					for ( const auto triIndex : adjacency[globalVertexIndex] )
					{
						/* Only the triangles of the vertex's side: a mirrored neighbour's tangent points the other way. */
						if ( m_triangles[triIndex].surfaceTangentHandedness() == handedness )
						{
							tangent += m_triangles[triIndex].surfaceTangent();
						}
					}

					m_vertices[globalVertexIndex].setTangent(tangent.normalize());
					m_vertices[globalVertexIndex].setTangentHandedness(handedness);
				}

				return true;
			}

			/**
			 * @brief Computes normal and tangent vectors for every vertex.
			 * @note If normals are present, use Shape::computeVertexTangent() instead.
			 * @warning Geometry must have normal and tangent vectors computed for every triangle.
			 * @note Uses vertex-to-triangle adjacency table for O(V+T) performance.
			 * @return bool
			 */
			bool
			computeVertexTBNSpace () noexcept
			{
				if ( m_triangles.empty() || m_vertices.empty() )
				{
					std::cerr << "Shape::computeVertexTBNSpace(), geometry is empty !" "\n";

					return false;
				}

				/* Build adjacency table: vertex → adjacent triangle indices. O(T) */
				const auto adjacency = this->buildVertexToTriangleAdjacency();

				for ( size_t globalVertexIndex = 0; globalVertexIndex < m_vertices.size(); ++globalVertexIndex )
				{
					const auto handedness = this->vertexTangentHandedness(adjacency[globalVertexIndex]);
					Math::Vector< 3, vertex_data_t > tangent{};
					Math::Vector< 3, vertex_data_t > normal{};

					for ( const auto triIndex : adjacency[globalVertexIndex] )
					{
						/* Only the triangles of the vertex's side: a mirrored neighbour's tangent points the other way. */
						if ( m_triangles[triIndex].surfaceTangentHandedness() == handedness )
						{
							tangent += m_triangles[triIndex].surfaceTangent();
						}

						normal += m_triangles[triIndex].surfaceNormal();
					}

					m_vertices[globalVertexIndex].setTangent(tangent.normalize());
					m_vertices[globalVertexIndex].setTangentHandedness(handedness);
					m_vertices[globalVertexIndex].setNormal(normal.normalize());
				}

				return true;
			}

			/**
			 * @brief Moves the axis origin to the bottom of the geometry, so the shape
			 * rests on the Y=0 plane and extends toward +Y.
			 * @param updateProperties Enable the shape properties update. Default true.
			 * @return void
			 */
			void
			setCenterAtBottom (bool updateProperties = true)
			{
				/* Find the lowest position (the bottom, Y-up world). */
				auto bottom = std::numeric_limits< vertex_data_t >::max();

				for ( const auto & vertexRef : m_vertices )
				{
					if ( vertexRef.position()[Math::Y] < bottom )
					{
						bottom = vertexRef.position()[Math::Y];
					}
				}

				this->transform(Math::Matrix< 4, vertex_data_t >::translation(0, -bottom, 0), updateProperties);
			}

			/**
			 * @brief Performs a geometry transformation with a Matrix 4x4.
			 * @param transform A reference to a matrix.
			 * @param updateProperties Enable the shape properties to update. Default true.
			 * @return void
			 */
			void
			transform (const Math::Matrix< 4, vertex_data_t > & transform, bool updateProperties = true) noexcept
			{
				/* NOTE: For tangents and normals transformation,
				 * we don't want to translate the vector. */
				auto noTranslate(transform);
				noTranslate.clearTranslation();

				/* NOTE: A mirror (negative determinant) reverses cross(N, T) relative to the
				 * transformed bitangent, so the handedness flips with it. */
				const auto mirrored = noTranslate.fastDeterminant() < static_cast< vertex_data_t >(0);

				/* NOTE: A normal is a covector: it goes through the inverse transpose of the 3x3 part, not
				 * through the part itself, or a non-uniform scale leans it toward the stretched axis. The
				 * cofactor matrix is det(A) * A^-T: the same direction once normalised, and still defined
				 * for a singular A (a shape flattened onto a plane gets that plane's normal). The sign of
				 * det(A) is put back so a mirror does not turn the normals inward. Cyclic form of the 3x3
				 * cofactors: C(r, c) = a(r+1, c+1) a(r+2, c+2) - a(r+1, c+2) a(r+2, c+1), indices mod 3. */
				const auto sign = mirrored ? static_cast< vertex_data_t >(-1) : static_cast< vertex_data_t >(1);
				const auto cofactor = [&noTranslate, sign] (size_t row, size_t col) {
					const auto row1 = (row + 1) % 3;
					const auto row2 = (row + 2) % 3;
					const auto col1 = (col + 1) % 3;
					const auto col2 = (col + 2) % 3;

					return sign * ((noTranslate(row1, col1) * noTranslate(row2, col2)) - (noTranslate(row1, col2) * noTranslate(row2, col1)));
				};
				const Math::Vector< 3, vertex_data_t > normalRow0{cofactor(0, 0), cofactor(0, 1), cofactor(0, 2)};
				const Math::Vector< 3, vertex_data_t > normalRow1{cofactor(1, 0), cofactor(1, 1), cofactor(1, 2)};
				const Math::Vector< 3, vertex_data_t > normalRow2{cofactor(2, 0), cofactor(2, 1), cofactor(2, 2)};

				const auto transformNormal = [&normalRow0, &normalRow1, &normalRow2] (const Math::Vector< 3, vertex_data_t > & normal) {
					return Math::Vector< 3, vertex_data_t >{
						Math::Vector< 3, vertex_data_t >::dotProduct(normalRow0, normal),
						Math::Vector< 3, vertex_data_t >::dotProduct(normalRow1, normal),
						Math::Vector< 3, vertex_data_t >::dotProduct(normalRow2, normal)
					};
				};

				for ( auto & vertexRef : m_vertices )
				{
					vertexRef.setPosition(transform * Math::Vector< 4, vertex_data_t >(vertexRef.position(), 1));

					const auto normal = transformNormal(vertexRef.normal()).normalize();
					/* ⚠️⚠️ Through a Vector< 3 >, never the raw Vector< 4 > product: setTangent(Vector< 4 >)
					 * reads W as the bitangent HANDEDNESS, and W is 0 here — that zeroed biNormal() on every
					 * transformed shape and flattened its normal map (the black geodesic sphere, 2026-10-06). */
					Math::Vector< 3, vertex_data_t > tangent{noTranslate * Math::Vector< 4, vertex_data_t >(vertexRef.tangent(), 0)};

					/* A sheared or non-uniformly scaled tangent is no longer perpendicular to the new normal:
					 * Gram-Schmidt puts it back in the tangent plane. */
					tangent -= normal * Math::Vector< 3, vertex_data_t >::dotProduct(normal, tangent);

					vertexRef.setTangent(tangent.normalize());
					vertexRef.setNormal(normal);

					if ( mirrored )
					{
						vertexRef.setTangentHandedness(-vertexRef.tangentHandedness());
					}
				}

				/* Updates the invalided bounding box. */
				if ( updateProperties )
				{
					this->updateProperties();
				}
			}

			/**
			 * @brief Removes vertices too close to each other under a tolerance value.
			 * @param vertexDistanceTolerance The distance below which two vertices will be welded.
			 * @return void
			 */
			[[deprecated("Not working correctly")]]
			void
			removeDoubleVertices (vertex_data_t vertexDistanceTolerance) noexcept
			{
				/* Gets a copy of the vertices position and clean the vector for replacement. */
				const std::vector< ShapeVertex< vertex_data_t > > baseVertices(m_vertices);

				m_vertices.clear();

				/* For every old vertex ... */
				for ( index_data_t baseIndex = 0; baseIndex < baseVertices.size(); ++baseIndex )
				{
					const auto & baseVertex = baseVertices[baseIndex];

					/* ... We check if we don't found a close position in the new list. */
					auto found = false;
					index_data_t newIndex = 0;

					for ( const auto & vertexRef : m_vertices )
					{
						if ( Math::Vector< 3, vertex_data_t >::distance(baseVertex.position(), vertexRef.position()) < vertexDistanceTolerance )
						{
							found = true;

							break;
						}

						++newIndex;
					}

					/* If not found, we put the new vertex position. */
					if ( !found )
					{
						m_vertices.emplace_back(baseVertex);

						newIndex = static_cast< index_data_t >(m_vertices.size() - 1);
					}

					/* Ne replacement requested. */
					if ( baseIndex == newIndex )
					{
						continue;
					}

					/* We processLogics every face index for vertices position. */
					for ( auto & triangle : m_triangles )
					{
						for ( index_data_t vertexIndex = 0; vertexIndex < 3; ++vertexIndex )
						{
							if ( triangle.vertexIndex(vertexIndex) == baseIndex )
							{
								triangle.setVertexIndex(vertexIndex, newIndex);
							}
						}
					}
				}

				/* Removes invalid triangles. */
				auto triangleIt = m_triangles.begin();

				while ( triangleIt != m_triangles.end() )
				{
					if ( (*triangleIt).vertexIndex(0) == (*triangleIt).vertexIndex(1) || (*triangleIt).vertexIndex(0) == (*triangleIt).vertexIndex(2) || (*triangleIt).vertexIndex(1) == (*triangleIt).vertexIndex(2) )
					{
						triangleIt = m_triangles.erase(triangleIt);
					}
					else
					{
						++triangleIt;
					}
				}
			}

			/**
			 * @brief Removes all vertex color information and replace by a new one.
			 * @param color The new color.
			 * @return void
			 */
			void
			setGlobalVertexColor (const Math::Vector< 4, vertex_data_t > & color) noexcept
			{
				m_vertexColors.clear();
				m_vertexColors.emplace_back(color);

				/* Sets a color pointer to index 0, the unique color. */
				for ( auto & triangle : m_triangles )
				{
					for ( index_data_t vertexIndex = 0; vertexIndex < 3; ++vertexIndex )
					{
						triangle.setVertexColorIndex(vertexIndex, 0);
					}
				}
			}

			/**
			 * @brief Removes all vertex color information and replace by a new one.
			 * @param color The new color.
			 * @return void
			 */
			void
			setGlobalVertexColor (const PixelFactory::Color< vertex_data_t > & color) noexcept
			{
				this->setGlobalVertexColor(color.template toVector4< vertex_data_t >());
			}

			/**
			 * @brief Flips the surface of the shape. This will reverse the order of vertices and the normal and tangent vectors of every triangle.
			 * @return void
			 */
			void
			flipSurface () noexcept
			{
				for ( auto & vertex : m_vertices )
				{
					vertex.flip();
				}

				for ( auto & triangle : m_triangles )
				{
					triangle.flip();
				}
			}

			/**
			 * @brief Reverses the winding order of every triangle without touching normals or tangents.
			 * @note Companion of flipYAxis(): a mirror already yields the correct mirrored normals
			 * and tangents but reverses the front-face orientation, which this call restores.
			 * flipSurface() would negate the vectors a second time.
			 * @return void
			 */
			void
			reverseWinding () noexcept
			{
				for ( auto & triangle : m_triangles )
				{
					triangle.reverseWinding();
				}
			}

			/**
			 * @brief Negates the V texture coordinate of every vertex.
			 * @note Used to be folded into flipYAxis(); a caller that mirrors geometry does not
			 * necessarily want its UVs mirrored too.
			 * @return void
			 */
			void
			flipTextureV () noexcept
			{
				for ( auto & vertex : m_vertices )
				{
					vertex.flipTextureV();
				}
			}

			/**
			 * @brief Flip the Y-Axis of every GEOMETRIC vertex attribute.
			 * @note Texture coordinates are not touched — see flipTextureV().
			 * @return void
			 */
			void
			flipYAxis () noexcept
			{
				for ( auto & vertex : m_vertices )
				{
					vertex.flipYAxis();
				}

				for ( auto & triangle : m_triangles )
				{
					triangle.flipYAxis();
				}
			}

			/**
			 * @brief Builds a shape using a function giving access to shape data.
			 * @param buildFunction A reference to a function.
			 * @param textureCoordinatesDeclared Set if texture coordinates will be set during the build.
			 * @param computeEdges Declares if edges must be calculated. Default false.
			 * @return bool
			 */
			bool
			build (const std::function< bool (std::vector< std::pair< index_data_t, index_data_t > > &, std::vector< ShapeVertex< vertex_data_t > > &, std::vector< ShapeTriangle< vertex_data_t > > &) > & buildFunction, bool textureCoordinatesDeclared, bool computeEdges = false) noexcept
			{
				this->clear();

				m_textureCoordinatesDeclared = textureCoordinatesDeclared;
				m_computeEdges = computeEdges;

				if ( !buildFunction(m_groups, m_vertices, m_triangles) )
				{
					return false;
				}

				this->updateProperties();

				return true;
			}

			/**
			 * @brief Creates an indexed vertex buffer.
			 * @note Returns the element count in one vertex.
			 * @param vertexBuffer A reference to the vertex buffer.
			 * @param normalType Set the normal format. Default none.
			 * @param textureCoordinatesType Set the texture coordinates format. Default none.
			 * @param vertexColorType Set the vertex color format. Default none.
			 * @param skeletalAnimationType Set vertex attributes for skeletal animation. Default none.
			 * @param secondaryTextureCoordinatesType Set the secondary texture coordinates format (after the primary ones). Default none.
			 * @return index_data_t
			 */
			index_data_t
			createVertexBuffer (std::vector< vertex_data_t > & vertexBuffer, NormalType normalType = NormalType::None, TextureCoordinatesType textureCoordinatesType = TextureCoordinatesType::None, VertexColorType vertexColorType = VertexColorType::None, SkeletalAnimationType skeletalAnimationType = SkeletalAnimationType::None, TextureCoordinatesType secondaryTextureCoordinatesType = TextureCoordinatesType::None) const noexcept
			{
				const auto vertexElementCount = getVertexElementCount(normalType, textureCoordinatesType, vertexColorType, skeletalAnimationType, secondaryTextureCoordinatesType);

				/* NOTE: Resize destination buffers. */
				vertexBuffer.resize(m_triangles.size() * 3 * vertexElementCount);

				index_data_t indexBufferOffset = 0;

				for ( const auto & triangle : m_triangles )
				{
					for ( index_data_t triangleVertexIndex = 0; triangleVertexIndex < 3; ++triangleVertexIndex)
					{
						const auto shapeVertexIndex = triangle.vertexIndex(triangleVertexIndex);
						const auto & vertex = m_vertices[shapeVertexIndex];

						index_data_t vertexBufferOffset = vertexElementCount * indexBufferOffset;

						++indexBufferOffset;

						/* Positions */
						Shape::writeVector3ToBuffer(vertex.position(), vertexBuffer, vertexBufferOffset);

						/* Normals */
						switch ( normalType )
						{
							case NormalType::Normal :
								Shape::writeVector3ToBuffer(vertex.normal(), vertexBuffer, vertexBufferOffset);
								break;

							case NormalType::TangentNormal :
								Shape::writeVector3ToBuffer(vertex.tangent(), vertexBuffer, vertexBufferOffset);
								Shape::writeVector3ToBuffer(vertex.normal(), vertexBuffer, vertexBufferOffset);
								break;

							case NormalType::TBNSpace :
								Shape::writeVector3ToBuffer(vertex.tangent(), vertexBuffer, vertexBufferOffset);
								Shape::writeVector3ToBuffer(vertex.biNormal(), vertexBuffer, vertexBufferOffset);
								Shape::writeVector3ToBuffer(vertex.normal(), vertexBuffer, vertexBufferOffset);
								break;

							default:
								break;
						}

						/* Texture Coordinates */
						switch ( textureCoordinatesType )
						{
							case TextureCoordinatesType::UV :
								Shape::writeVector2ToBuffer(vertex.textureCoordinates(), vertexBuffer, vertexBufferOffset);
								break;

							case TextureCoordinatesType::UVW :
								Shape::writeVector3ToBuffer(vertex.textureCoordinates(), vertexBuffer, vertexBufferOffset);
								break;

							default:
								break;
						}

						/* Secondary texture coordinates, after the primary ones (the engine's vertex format order). The
						 * source set is 2D; a 3D request gets W = 0. */
						switch ( secondaryTextureCoordinatesType )
						{
							case TextureCoordinatesType::UV :
								Shape::writeVector2ToBuffer(vertex.secondaryTextureCoordinates(), vertexBuffer, vertexBufferOffset);
								break;

							case TextureCoordinatesType::UVW :
								Shape::writeVector3ToBuffer(Math::Vector< 3, vertex_data_t >{vertex.secondaryTextureCoordinates()[Math::X], vertex.secondaryTextureCoordinates()[Math::Y], 0}, vertexBuffer, vertexBufferOffset);
								break;

							default:
								break;
						}

						/* Vertex Colors */
						if ( vertexColorType != VertexColorType::None )
						{
							const auto & vertexColor = m_vertexColors[triangle.vertexColorIndex(triangleVertexIndex)];

							switch ( vertexColorType )
							{
								case VertexColorType::Gray :
									Shape::writeVector1ToBuffer(vertexColor, vertexBuffer, vertexBufferOffset);
									break;

								case VertexColorType::RGB :
									Shape::writeVector3ToBuffer(vertexColor, vertexBuffer, vertexBufferOffset);
									break;

								case VertexColorType::RGBA :
									Shape::writeVector4ToBuffer(vertexColor, vertexBuffer, vertexBufferOffset);
									break;

								default:
									break;
							}
						}

						switch ( skeletalAnimationType )
						{
							case SkeletalAnimationType::Average3:
								Shape::writeInfluenceToVertexBuffer(vertex.influences(), 3, vertexBuffer, vertexBufferOffset);
								break;

							case SkeletalAnimationType::Average4:
								Shape::writeInfluenceToVertexBuffer(vertex.influences(), 4, vertexBuffer, vertexBufferOffset);
								break;

							case SkeletalAnimationType::Weighted3:
								Shape::writeInfluenceToVertexBuffer(vertex.influences(), 3, vertexBuffer, vertexBufferOffset);

								Shape::writeVector3ToBuffer(vertex.weights(), vertexBuffer, vertexBufferOffset);
								break;

							case SkeletalAnimationType::Weighted4:
								Shape::writeInfluenceToVertexBuffer(vertex.influences(), 4, vertexBuffer, vertexBufferOffset);

								Shape::writeVector4ToBuffer(vertex.weights(), vertexBuffer, vertexBufferOffset);
								break;

							default:
								break;
						}
					}
				}

				return vertexElementCount;
			}

			/**
			 * @brief Creates an indexed vertex buffer.
			 * @note Returns the element count in one vertex.
			 * @param vertexBuffer A reference to the vertex buffer.
			 * @param indexBuffer A reference to the index buffer.
			 * @param normalType Set the normal format. Default none.
			 * @param textureCoordinatesType Set the texture coordinates format. Default none.
			 * @param vertexColorType Set the vertex color format. Default none.
			 * @param skeletalAnimationType Set vertex attributes for skeletal animation. Default none.
			 * @param secondaryTextureCoordinatesType Set the secondary texture coordinates format (after the primary ones). Default none.
			 * @return index_data_t
			 */
			index_data_t
			createIndexedVertexBuffer (std::vector< vertex_data_t > & vertexBuffer, std::vector< index_data_t > & indexBuffer, NormalType normalType = NormalType::None, TextureCoordinatesType textureCoordinatesType = TextureCoordinatesType::None, VertexColorType vertexColorType = VertexColorType::None, SkeletalAnimationType skeletalAnimationType = SkeletalAnimationType::None, TextureCoordinatesType secondaryTextureCoordinatesType = TextureCoordinatesType::None) const noexcept
			{
				/* NOTE: Keep track of vertex already used. */
				std::set< index_data_t > shapeVertexIndicesDone{};

				const auto vertexElementCount = getVertexElementCount(normalType, textureCoordinatesType, vertexColorType, skeletalAnimationType, secondaryTextureCoordinatesType);

				/* NOTE: Resize destination buffers. */
				vertexBuffer.resize(m_vertices.size() * vertexElementCount);
				indexBuffer.resize(m_triangles.size() * 3);

				index_data_t indexBufferOffset = 0;

				for ( const auto & triangle : m_triangles )
				{
					for ( index_data_t triangleVertexIndex = 0; triangleVertexIndex < 3; ++triangleVertexIndex )
					{
						const auto shapeVertexIndex = triangle.vertexIndex(triangleVertexIndex);

						indexBuffer[indexBufferOffset++] = shapeVertexIndex;

						/* NOTE: Skip the vertex index already done. */
						if ( shapeVertexIndicesDone.contains(shapeVertexIndex) )
						{
							continue;
						}

						const auto & vertex = m_vertices.at(shapeVertexIndex);

						index_data_t vertexBufferOffset = vertexElementCount * shapeVertexIndex;

						/* Positions */
						Shape::writeVector3ToBuffer(vertex.position(), vertexBuffer, vertexBufferOffset);

						/* Normals */
						switch ( normalType )
						{
							case NormalType::Normal :
								Shape::writeVector3ToBuffer(vertex.normal(), vertexBuffer, vertexBufferOffset);
								break;

							case NormalType::TangentNormal :
								Shape::writeVector3ToBuffer(vertex.tangent(), vertexBuffer, vertexBufferOffset);
								Shape::writeVector3ToBuffer(vertex.normal(), vertexBuffer, vertexBufferOffset);
								break;

							case NormalType::TBNSpace :
								Shape::writeVector3ToBuffer(vertex.tangent(), vertexBuffer, vertexBufferOffset);
								Shape::writeVector3ToBuffer(vertex.biNormal(), vertexBuffer, vertexBufferOffset);
								Shape::writeVector3ToBuffer(vertex.normal(), vertexBuffer, vertexBufferOffset);
								break;

							default:
								break;
						}

						/* Texture Coordinates */
						switch ( textureCoordinatesType )
						{
							case TextureCoordinatesType::UV :
								Shape::writeVector2ToBuffer(vertex.textureCoordinates(), vertexBuffer, vertexBufferOffset);
								break;

							case TextureCoordinatesType::UVW :
								Shape::writeVector3ToBuffer(vertex.textureCoordinates(), vertexBuffer, vertexBufferOffset);
								break;

							default:
								break;
						}

						/* Secondary texture coordinates, after the primary ones (the engine's vertex format order). The
						 * source set is 2D; a 3D request gets W = 0. */
						switch ( secondaryTextureCoordinatesType )
						{
							case TextureCoordinatesType::UV :
								Shape::writeVector2ToBuffer(vertex.secondaryTextureCoordinates(), vertexBuffer, vertexBufferOffset);
								break;

							case TextureCoordinatesType::UVW :
								Shape::writeVector3ToBuffer(Math::Vector< 3, vertex_data_t >{vertex.secondaryTextureCoordinates()[Math::X], vertex.secondaryTextureCoordinates()[Math::Y], 0}, vertexBuffer, vertexBufferOffset);
								break;

							default:
								break;
						}

						/* Vertex Colors */
						if ( vertexColorType != VertexColorType::None )
						{
							const auto & vertexColor = m_vertexColors[triangle.vertexColorIndex(triangleVertexIndex)];

							switch ( vertexColorType )
							{
								case VertexColorType::Gray :
									Shape::writeVector1ToBuffer(vertexColor, vertexBuffer, vertexBufferOffset);
									break;

								case VertexColorType::RGB :
									Shape::writeVector3ToBuffer(vertexColor, vertexBuffer, vertexBufferOffset);
									break;

								case VertexColorType::RGBA :
									Shape::writeVector4ToBuffer(vertexColor, vertexBuffer, vertexBufferOffset);
									break;

								default:
									break;
							}
						}

						switch ( skeletalAnimationType )
						{
							case SkeletalAnimationType::Average3:
								Shape::writeInfluenceToVertexBuffer(vertex.influences(), 3, vertexBuffer, vertexBufferOffset);
								break;

							case SkeletalAnimationType::Average4:
								Shape::writeInfluenceToVertexBuffer(vertex.influences(), 4, vertexBuffer, vertexBufferOffset);
								break;

							case SkeletalAnimationType::Weighted3:
								Shape::writeInfluenceToVertexBuffer(vertex.influences(), 3, vertexBuffer, vertexBufferOffset);

								Shape::writeVector3ToBuffer(vertex.weights(), vertexBuffer, vertexBufferOffset);
								break;

							case SkeletalAnimationType::Weighted4:
								Shape::writeInfluenceToVertexBuffer(vertex.influences(), 4, vertexBuffer, vertexBufferOffset);

								Shape::writeVector4ToBuffer(vertex.weights(), vertexBuffer, vertexBufferOffset);
								break;

							default:
								break;
						}

						shapeVertexIndicesDone.emplace(shapeVertexIndex);
					}
				}

				return vertexElementCount;
			}

			/**
			 * @brief Rebuilds the shape from an indexed vertex buffer written by createIndexedVertexBuffer() with the
			 * same formats: its inverse, for a GPU copy read back.
			 * @note The result is what the buffer holds, not necessarily the shape that wrote it: one colour per
			 * vertex (the buffer keeps the first triangle's), no attribute the formats left out (a TangentNormal
			 * buffer carries no handedness, an Average skinning no weight), no edge. Re-encoding it with the same
			 * formats gives the same buffer back.
			 * @param vertexBuffer The vertex attributes, vertexElementCount floats per vertex.
			 * @param indexBuffer The triangle list indices.
			 * @param groups The groups, (first triangle, triangle count); empty for one group of every triangle.
			 * @param normalType The normal format the buffer was written with.
			 * @param textureCoordinatesType The texture coordinates format.
			 * @param vertexColorType The vertex colour format.
			 * @param skeletalAnimationType The skeletal attributes.
			 * @param secondaryTextureCoordinatesType The secondary texture coordinates format.
			 * @return bool False on inconsistent sizes (a partial vertex, an index count not multiple of 3, an index
			 * out of range, a group beyond the triangles); the shape is then left empty.
			 */
			[[nodiscard]]
			bool
			readIndexedVertexBuffer (std::span< const vertex_data_t > vertexBuffer, std::span< const index_data_t > indexBuffer, const std::vector< std::pair< index_data_t, index_data_t > > & groups, NormalType normalType = NormalType::None, TextureCoordinatesType textureCoordinatesType = TextureCoordinatesType::None, VertexColorType vertexColorType = VertexColorType::None, SkeletalAnimationType skeletalAnimationType = SkeletalAnimationType::None, TextureCoordinatesType secondaryTextureCoordinatesType = TextureCoordinatesType::None) noexcept
			{
				this->clear();

				const size_t vertexElementCount = getVertexElementCount(normalType, textureCoordinatesType, vertexColorType, skeletalAnimationType, secondaryTextureCoordinatesType);

				if ( vertexElementCount == 0 || vertexBuffer.size() % vertexElementCount != 0 || indexBuffer.size() % 3 != 0 )
				{
					return false;
				}

				const auto vertexCount = vertexBuffer.size() / vertexElementCount;
				const auto triangleCount = indexBuffer.size() / 3;

				if ( vertexCount > std::numeric_limits< index_data_t >::max() || std::ranges::any_of(indexBuffer, [vertexCount] (index_data_t index) { return index >= vertexCount; }) )
				{
					return false;
				}

				m_vertices.resize(vertexCount);

				if ( vertexColorType != VertexColorType::None )
				{
					m_vertexColors.resize(vertexCount);
				}

				for ( size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex )
				{
					auto offset = vertexIndex * vertexElementCount;

					Shape::readVertex(vertexBuffer, offset, m_vertices[vertexIndex], vertexColorType != VertexColorType::None ? &m_vertexColors[vertexIndex] : nullptr, normalType, textureCoordinatesType, vertexColorType, skeletalAnimationType, secondaryTextureCoordinatesType);
				}

				m_triangles.resize(triangleCount);

				for ( size_t triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex )
				{
					auto & triangle = m_triangles[triangleIndex];

					for ( index_data_t corner = 0; corner < 3; ++corner )
					{
						const auto vertexIndex = indexBuffer[(triangleIndex * 3) + corner];

						triangle.setVertexIndex(corner, vertexIndex);
						/* NOTE: The buffer holds one colour per vertex. */
						triangle.setVertexColorIndex(corner, vertexColorType != VertexColorType::None ? vertexIndex : index_data_t{0});
					}
				}

				return this->finishReading(groups, triangleCount, textureCoordinatesType != TextureCoordinatesType::None, normalType != NormalType::None);
			}

			/**
			 * @brief Rebuilds the shape from a vertex buffer written by createVertexBuffer() with the same formats (three
			 * vertices per triangle, no index): its inverse, for a GPU copy read back.
			 * @note As readIndexedVertexBuffer(): what the buffer holds; every triangle gets its own three vertices.
			 * @param vertexBuffer The vertex attributes, vertexElementCount floats per vertex.
			 * @param groups The groups, (first triangle, triangle count); empty for one group of every triangle.
			 * @param normalType The normal format the buffer was written with.
			 * @param textureCoordinatesType The texture coordinates format.
			 * @param vertexColorType The vertex colour format.
			 * @param skeletalAnimationType The skeletal attributes.
			 * @param secondaryTextureCoordinatesType The secondary texture coordinates format.
			 * @return bool False on inconsistent sizes (a partial triangle, a group beyond the triangles); the shape is
			 * then left empty.
			 */
			[[nodiscard]]
			bool
			readVertexBuffer (std::span< const vertex_data_t > vertexBuffer, const std::vector< std::pair< index_data_t, index_data_t > > & groups, NormalType normalType = NormalType::None, TextureCoordinatesType textureCoordinatesType = TextureCoordinatesType::None, VertexColorType vertexColorType = VertexColorType::None, SkeletalAnimationType skeletalAnimationType = SkeletalAnimationType::None, TextureCoordinatesType secondaryTextureCoordinatesType = TextureCoordinatesType::None) noexcept
			{
				this->clear();

				const size_t vertexElementCount = getVertexElementCount(normalType, textureCoordinatesType, vertexColorType, skeletalAnimationType, secondaryTextureCoordinatesType);

				if ( vertexElementCount == 0 || vertexBuffer.size() % (vertexElementCount * 3) != 0 )
				{
					return false;
				}

				const auto vertexCount = vertexBuffer.size() / vertexElementCount;
				const auto triangleCount = vertexCount / 3;

				if ( vertexCount > std::numeric_limits< index_data_t >::max() )
				{
					return false;
				}

				m_vertices.resize(vertexCount);

				if ( vertexColorType != VertexColorType::None )
				{
					m_vertexColors.resize(vertexCount);
				}

				m_triangles.resize(triangleCount);

				for ( size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex )
				{
					auto offset = vertexIndex * vertexElementCount;

					Shape::readVertex(vertexBuffer, offset, m_vertices[vertexIndex], vertexColorType != VertexColorType::None ? &m_vertexColors[vertexIndex] : nullptr, normalType, textureCoordinatesType, vertexColorType, skeletalAnimationType, secondaryTextureCoordinatesType);

					const auto corner = static_cast< index_data_t >(vertexIndex % 3);
					auto & triangle = m_triangles[vertexIndex / 3];

					triangle.setVertexIndex(corner, static_cast< index_data_t >(vertexIndex));
					triangle.setVertexColorIndex(corner, vertexColorType != VertexColorType::None ? static_cast< index_data_t >(vertexIndex) : index_data_t{0});
				}

				return this->finishReading(groups, triangleCount, textureCoordinatesType != TextureCoordinatesType::None, normalType != NormalType::None);
			}

			/**
			 * @brief Declares a new group.
			 * @note This function is for building the shape manually.
			 * @return void
			 */
			void
			newGroup () noexcept
			{
				if ( m_triangles.empty() )
				{
					return;
				}

				/* Creates a new group with the offset
				 * according to triangles execute offset. */
				m_groups.emplace_back(static_cast< uint32_t >(m_triangles.size()), 0);
			}

			/**
			 * @brief Declares a new vertex and returns its index, merging it with an identical one.
			 * @note This function is for building the shape manually.
			 * @param position The position of the vertex.
			 * @return index_data_t
			 */
			index_data_t
			addVertex (const Math::Vector< 3, vertex_data_t > & position) noexcept
			{
				return this->addVertex(position, {}, {});
			}

			/**
			 * @brief Declares a new vertex and returns its index, merging it with an identical one.
			 * @note This function is for building the shape manually.
			 * @param position The position of the vertex.
			 * @param normal The normal of the vertex.
			 * @return index_data_t
			 */
			index_data_t
			addVertex (const Math::Vector< 3, vertex_data_t > & position, const Math::Vector< 3, vertex_data_t > & normal) noexcept
			{
				return this->addVertex(position, normal, {});
			}

			/**
			 * @brief Declares a new vertex and returns its index, merging it with an identical one.
			 * @note This function is for building the shape manually.
			 * @note ⚠️ Identical means "in the same cell of a grid of side mergeTolerance", NOT "equal
			 * within an epsilon". It used to be the latter, in a LINEAR SCAN of every vertex already
			 * stored, which made building a shape quadratic: 8.7 s for a 65 536 triangle sphere against
			 * 24.9 ms. No hash can reproduce an epsilon equality — that relation is not transitive — so
			 * making this fast IS a change of merge semantics, and a deliberate one (owner decision,
			 * 2026-09-22).
			 * @note The grid merges MORE than the old epsilon did, not less: the absolute
			 * numeric_limits<float>::epsilon() (1.19e-7) is finer than the rounding a generator's own
			 * trigonometry produces, so it missed vertices that agree on every attribute and differ
			 * only in their last float bits. Measured on a 256x128 sphere: 33 169 vertices against
			 * 36 405, and the same count the batch pass of ShapeProcessor returns.
			 * @note ⚠️ This does NOT close a UV seam, and must not: the two sides of a seam carry
			 * u = 0 and u = 1, which are genuinely different texture coordinates. A sphere keeps its
			 * seam and its poles split and is still not watertight in the edge sense — that was never
			 * the epsilon's doing.
			 * @note ⚠️ Merging progressively is order-dependent at a cell boundary, where the batch
			 * pass is not: a handful of vertices may differ between the two (2 of 2 145 measured on a
			 * 64x32 sphere). Do not pin an exact count across the two paths.
			 * @param position The position of the vertex.
			 * @param normal The normal of the vertex.
			 * @param textureCoordinates The texture coordinates of the vertex.
			 * @return index_data_t
			 */
			index_data_t
			addVertex (const Math::Vector< 3, vertex_data_t > & position, const Math::Vector< 3, vertex_data_t > & normal, const Math::Vector< 3, vertex_data_t > & textureCoordinates) noexcept
			{
				this->restoreConstructionIndexes();

				const VertexKey key{this->quantize(position), this->quantize(normal), this->quantize(textureCoordinates)};

				const auto keyIt = m_vertexIndex.find(key);

				if ( keyIt != m_vertexIndex.cend() )
				{
					return keyIt->second;
				}

				const auto index = this->saveVertex(position, normal, textureCoordinates);

				m_vertexIndex.emplace(key, index);

				return index;
			}

			/**
			 * @brief Declares a new vertex color and returns its index, merging it with an identical one.
			 * @note This function is for building the shape manually.
			 * @note Same grid equality as addVertex(), and it had the same quadratic defect.
			 * @param color The color.
			 * @return index_data_t
			 */
			index_data_t
			addVertexColor (const Math::Vector< 4, vertex_data_t > & color) noexcept
			{
				this->restoreConstructionIndexes();

				const ColorKey key{this->quantize(Math::Vector< 3, vertex_data_t >{color[Math::X], color[Math::Y], color[Math::Z]}), this->quantizeScalar(color[Math::W])};

				const auto keyIt = m_vertexColorIndex.find(key);

				if ( keyIt != m_vertexColorIndex.cend() )
				{
					return keyIt->second;
				}

				const auto index = this->saveVertexColor(color);

				m_vertexColorIndex.emplace(key, index);

				return index;
			}

			/**
			 * @brief Sets the side of the grid cell two vertices must share to be merged.
			 * @note Default 1e-4, the same value ShapeProcessor::deduplicateVertices() uses, so the two
			 * merge paths of the library speak the same language.
			 * @param tolerance The cell side. A value of 0 or less is ignored.
			 * @return void
			 */
			void
			setMergeTolerance (vertex_data_t tolerance) noexcept
			{
				if ( tolerance > 0 )
				{
					m_mergeTolerance = tolerance;
				}
			}

			/**
			 * @brief Returns the side of the grid cell two vertices must share to be merged.
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			mergeTolerance () const noexcept
			{
				return m_mergeTolerance;
			}

			/**
			 * @brief Saves the new vertex and returns its index.
			 * @note This function is for building the shape manually.
			 * @param position The position of the vertex.
			 * @param normal The normal of the vertex.
			 * @param textureCoordinates The texture coordinates to that vertex.
			 * @return index_data_t
			 */
			index_data_t
			saveVertex (const Math::Vector< 3, vertex_data_t > & position, const Math::Vector< 3, vertex_data_t > & normal = {}, const Math::Vector< 3, vertex_data_t > & textureCoordinates = {}) noexcept
			{
				m_vertices.emplace_back(position, normal, textureCoordinates);

				m_textureCoordinatesDeclared = true;

				return static_cast< index_data_t >(m_vertices.size() - 1);
			}

			/**
			 * @brief Saves a new vertex color and returns its index.
			 * @note This function is for building the shape manually.
			 * @param color The color.
			 * @return index_data_t The index of the new vertex color.
			 */
			index_data_t
			saveVertexColor (const Math::Vector< 4, vertex_data_t > & color) noexcept
			{
				m_vertexColors.emplace_back(color);

				return static_cast< index_data_t >(m_vertexColors.size() - 1);
			}

			/**
			 * @brief Declares a new triangle.
			 * @param triangle The triangle object.
			 * @return void
			 */
			void
			addTriangle (ShapeTriangle< vertex_data_t > & triangle) noexcept
			{
				/* Creates edges */
				triangle.setEdgeIndex(0, this->addEdge(triangle.vertexIndex(0), triangle.vertexIndex(1)));
				triangle.setEdgeIndex(1, this->addEdge(triangle.vertexIndex(1), triangle.vertexIndex(2)));
				triangle.setEdgeIndex(2, this->addEdge(triangle.vertexIndex(2), triangle.vertexIndex(0)));

				m_triangles.emplace_back(triangle);

				/* Updates group system. */
				if ( m_groups.empty() )
				{
					this->newGroup();
				}
				else
				{
					++m_groups.back().second;
				}
			}

			/**
			 * @brief Rebuilds the edge list and every triangle edge index from the current triangles.
			 * @note ⚠️ Anything that renumbers the VERTICES invalidates the edges: a ShapeEdge holds
			 * vertex indices, and a triangle holds indices into the edge list. Call this after such a
			 * pass, or shape.edges() answers with pre-renumbering indices. Measured on a 16x8 sphere
			 * deduplicated from 768 to 153 vertices: 765 of the 768 edge indices wrong, and 615 edges
			 * still naming vertices that no longer exist.
			 * @note Cheap since addEdge() became a hashed lookup (2026-09-21); it would have been
			 * unthinkable when pairing meant scanning the whole edge list.
			 * @note Does nothing on a shape that carries no edge, so a caller need not ask first.
			 * @return void
			 */
			void
			rebuildEdges () noexcept
			{
				if ( m_edges.empty() )
				{
					return;
				}

				this->restoreConstructionIndexes();

				m_edges.clear();
				m_unpairedEdges.clear();

				/* The adjacency changed, so any boundary loop found on the old one is stale too. */
				m_boundaryLoops.clear();
				m_boundaryLoopsAnalyzed = false;

				for ( auto & triangle : m_triangles )
				{
					triangle.setEdgeIndex(0, this->addEdge(triangle.vertexIndex(0), triangle.vertexIndex(1)));
					triangle.setEdgeIndex(1, this->addEdge(triangle.vertexIndex(1), triangle.vertexIndex(2)));
					triangle.setEdgeIndex(2, this->addEdge(triangle.vertexIndex(2), triangle.vertexIndex(0)));
				}
			}

			/**
			 * @brief Recomputes the centroid and the bounding box.
			 * @return void
			 */
			void
			updateProperties () noexcept
			{
				/* Reset properties. */
				m_boundingBox.reset();
				m_boundingSphere.reset();
				m_farthestDistance = 0;

				for ( const auto & vertexRef : m_vertices )
				{
					/* Update the bounding box. */
					m_boundingBox.merge(vertexRef.position());

					/* Update the farthest point from the origin. */
					const auto distance = vertexRef.position().length();

					if ( distance > m_farthestDistance )
					{
						m_farthestDistance = distance;
					}
				}

				/* NOTE: Use the bounding box center instead of the average of vertices.
				 * The average of vertices can be off-center for non-uniformly sampled geometry
				 * (e.g., spheres with more vertices at poles), causing an inflated bounding sphere. */
				const auto centroid = m_boundingBox.centroid();

				vertex_data_t centroidDistance = 0;

				for ( const auto & vertexRef : m_vertices )
				{
					/* Update the farthest point from the bounding box center. */
					const auto distance = (vertexRef.position() - centroid).length();

					if ( distance > centroidDistance )
					{
						centroidDistance = distance;
					}
				}

				m_boundingSphere.setRadius(centroidDistance);
				m_boundingSphere.setPosition(centroid);

				if constexpr ( VertexFactoryDebugEnabled )
				{
					std::cout <<
						"[DEBUG:VERTEX_FACTORY] Updating shape infos" "\n" <<
						m_boundingBox <<
						m_boundingSphere <<
						"Farthest distance : " << m_farthestDistance << "\n"
						"Centroid : " << centroid << "\n"
						"Centroid distance : " << centroid.length() << "\n";
				}
			}

			/**
			 * @brief STL streams printable object.
			 * @param out A reference to the stream output.
			 * @param obj A reference to the object to print.
			 * @return std::ostream &
			 */
			friend
			std::ostream &
			operator<< (std::ostream & out, const Shape & obj)
			{
				out <<
					"Shape triangle count: " << obj.m_triangles.size() << ", "
					"vertex count: " << obj.m_vertices.size() << ", "
					"vertex color count: " << obj.m_vertexColors.size() << "\n";

				index_data_t triangleIndex = 0;

				for ( const auto & triangle : obj.m_triangles )
				{
					out << "Triangle #" << triangleIndex << ", ";

					for ( index_data_t triangleVertexIndex = 0; triangleVertexIndex < 3; ++triangleVertexIndex )
					{
						const auto shapeVertexIndex = triangle.vertexIndex(triangleVertexIndex);
						const auto & vertex = obj.m_vertices.at(shapeVertexIndex);

						out <<
							"Triangle vertex index #" << triangleVertexIndex << " (Shape vertex index : #" << shapeVertexIndex << "). " "\n"
							"Position:" << vertex.position() << ", "
							"Tangent:" << vertex.tangent() << ", "
							"Normal:" << vertex.normal() << ", "
							"Texture Coordinates:" << vertex.textureCoordinates();

						if ( !obj.m_vertexColors.empty() )
						{
							const auto shapeColorIndex = triangle.vertexColorIndex(triangleVertexIndex);

							out << ", Vertex Color:" << obj.m_vertexColors.at(shapeColorIndex);
						}

						out << "\n";
					}

					++triangleIndex;
				}

				return out;
			}

			/**
			 * @brief Stringifies the object.
			 * @param obj A reference to the object to print.
			 * @return std::string
			 */
			friend
			std::string
			to_string (const Shape & obj) noexcept
			{
				std::stringstream output;

				output << obj;

				return output.str();
			}

		private:

			/**
			 * @brief Returns the tangent frame of a triangle from its texture coordinates: the tangent dP/du and the
			 * handedness, -1 on a MIRRORED UV island (Lengyel 2001; MikkTSpace, the glTF reference).
			 * @note Math::Vector::tangent() normalises without dividing by the UV determinant
			 * r = Δu1·Δv2 − Δu2·Δv1: it answers −sign(r)·dP/du. In the engine's UV space V grows DOWN the image, where an
			 * unmirrored island has r < 0 (generateQuad(): T = +X, bitangent cross(N, T) = +Y, the image's up) — the
			 * answer as it is, handedness +1. A mirrored island (r > 0) gets −answer = dP/du and a handedness of -1, so its
			 * bitangent cross(N, T)·(−1) is the image's up as well. A degenerate mapping (r = 0) keeps the answer, +1.
			 * @param vertexA The first vertex, counter-clockwise.
			 * @param vertexB The second vertex.
			 * @param vertexC The third vertex.
			 * @return std::pair< Math::Vector< 3, vertex_data_t >, vertex_data_t >
			 */
			[[nodiscard]]
			static
			std::pair< Math::Vector< 3, vertex_data_t >, vertex_data_t >
			triangleTangentFrame (const ShapeVertex< vertex_data_t > & vertexA, const ShapeVertex< vertex_data_t > & vertexB, const ShapeVertex< vertex_data_t > & vertexC) noexcept
			{
				const auto & uvA = vertexA.textureCoordinates();
				const auto & uvB = vertexB.textureCoordinates();
				const auto & uvC = vertexC.textureCoordinates();

				const auto tangent = Math::Vector< 3, vertex_data_t >::tangent(vertexA.position(), uvA, vertexB.position(), uvB, vertexC.position(), uvC);
				const auto determinant = ((uvB[Math::X] - uvA[Math::X]) * (uvC[Math::Y] - uvA[Math::Y])) - ((uvC[Math::X] - uvA[Math::X]) * (uvB[Math::Y] - uvA[Math::Y]));

				if ( determinant > 0 )
				{
					return {-tangent, static_cast< vertex_data_t >(-1)};
				}

				return {tangent, static_cast< vertex_data_t >(1)};
			}

			/**
			 * @brief Returns the handedness of a vertex: the side most of its triangles are on (+1 on a tie).
			 * @note A vertex shared by triangles of both sides sits on a mirror seam; it should be split (one copy per
			 * side), which this shape does not do — its frame is then right for its majority side only.
			 * @param triangleIndexes The indexes of the triangles around the vertex.
			 * @return vertex_data_t
			 */
			[[nodiscard]]
			vertex_data_t
			vertexTangentHandedness (const std::vector< size_t > & triangleIndexes) const noexcept
			{
				int64_t balance = 0;

				for ( const auto triangleIndex : triangleIndexes )
				{
					balance += m_triangles[triangleIndex].surfaceTangentHandedness() < 0 ? -1 : 1;
				}

				return balance < 0 ? static_cast< vertex_data_t >(-1) : static_cast< vertex_data_t >(1);
			}

			/**
			 * @brief Builds an adjacency table mapping each vertex index to the list of triangle indices that reference it.
			 * @details Single O(T) pass over triangles. Used by computeVertexNormal(), computeVertexTangent(),
			 * and computeVertexTBNSpace() to avoid O(V*T) brute-force scans.
			 * @return std::vector< std::vector< size_t > > The adjacency table.
			 */
			[[nodiscard]]
			std::vector< std::vector< size_t > >
			buildVertexToTriangleAdjacency () const noexcept
			{
				std::vector< std::vector< size_t > > adjacency(m_vertices.size());

				for ( size_t triIndex = 0; triIndex < m_triangles.size(); ++triIndex )
				{
					const auto & triangle = m_triangles[triIndex];

					adjacency[triangle.vertexIndex(0)].push_back(triIndex);
					adjacency[triangle.vertexIndex(1)].push_back(triIndex);
					adjacency[triangle.vertexIndex(2)].push_back(triIndex);
				}

				return adjacency;
			}

			/**
			 * @brief Declares a new edge.
			 * @param vertexIndexA The index in the vertices list of the first vertex.
			 * @param vertexIndexB The index in the vertices list of the second vertex.
			 * @return index_data_t
			 */
			index_data_t
			addEdge (index_data_t vertexIndexA, index_data_t vertexIndexB) noexcept
			{
				/* NOTE: The half-edge waiting for its mate is looked up by the unordered vertex index
				 * pair. A linear scan of the edge list here makes the whole shape construction quadratic
				 * in the triangle count, which no procedural generator can afford. */
				this->restoreConstructionIndexes();

				const EdgeKey key{std::min(vertexIndexA, vertexIndexB), std::max(vertexIndexA, vertexIndexB)};

				const auto slotIt = m_unpairedEdges.find(key);
				const auto found = slotIt != m_unpairedEdges.end();

				/* NOTE: A third triangle sharing the same edge means a non-manifold shape. */
				if ( found && slotIt->second.paired )
				{
					return std::numeric_limits< index_data_t >::max();
				}

				/* Insert the new edge. */
				m_edges.emplace_back(vertexIndexA, vertexIndexB);

				const auto newEdgeIndex = static_cast< index_data_t >(m_edges.size() - 1);

				/* Link with the shared edge if it was found. */
				if ( found )
				{
					/* Sets to the new inserted edge the index of the shared edge found. */
					m_edges.back().setSharedIndex(slotIt->second.index);

					/* Save the index of the new edge to the shared edge found. */
					m_edges[slotIt->second.index].setSharedIndex(newEdgeIndex);

					slotIt->second.paired = true;
				}
				else
				{
					m_unpairedEdges.emplace(key, EdgeSlot{newEdgeIndex, false});
				}

				return newEdgeIndex;
			}

			/**
			 * @briefs Checks and computes the vertex element count and returns the size.
			 * @param normalType Set the normal format. Default none.
			 * @param textureCoordinatesType Set the texture coordinates format. Default none.
			 * @param vertexColorType Set the vertex color format. Default none.
			 * @param skeletalAnimationType Set vertex attributes for skeletal animation. Default none.
			 * @param secondaryTextureCoordinatesType Set the secondary texture coordinates format. Default none.
			 * @return index_data_t
			 */
			[[nodiscard]]
			static
			index_data_t
			getVertexElementCount (NormalType normalType, TextureCoordinatesType textureCoordinatesType, VertexColorType vertexColorType, SkeletalAnimationType skeletalAnimationType, TextureCoordinatesType secondaryTextureCoordinatesType = TextureCoordinatesType::None)
			{
				auto vertexElementCount = 3;

				switch ( secondaryTextureCoordinatesType )
				{
					case TextureCoordinatesType::UV :
						vertexElementCount += 2;
						break;

					case TextureCoordinatesType::UVW :
						vertexElementCount += 3;
						break;

					default:
						break;
				}

				switch ( normalType )
				{
					case NormalType::Normal :
						vertexElementCount += 3;
						break;

					case NormalType::TangentNormal :
						vertexElementCount += 6;
						break;

					case NormalType::TBNSpace :
						vertexElementCount += 9;
						break;

					default:
						break;
				}

				switch ( textureCoordinatesType )
				{
					case TextureCoordinatesType::UV :
						vertexElementCount += 2;
						break;

					case TextureCoordinatesType::UVW :
						vertexElementCount += 3;
						break;

					default:
						break;
				}

				switch ( vertexColorType )
				{
					case VertexColorType::Gray :
						vertexElementCount += 1;
						break;

					case VertexColorType::RGB :
						vertexElementCount += 3;
						break;

					case VertexColorType::RGBA :
						vertexElementCount += 4;
						break;

					default:
						break;
				}

				switch ( skeletalAnimationType )
				{
					case SkeletalAnimationType::Average3 :
						vertexElementCount += 3;
						break;

					case SkeletalAnimationType::Average4 :
						vertexElementCount += 4;
						break;

					case SkeletalAnimationType::Weighted3 :
						vertexElementCount += 6;
						break;

					case SkeletalAnimationType::Weighted4 :
						vertexElementCount += 8;
						break;

					case SkeletalAnimationType::None :
					default:
						break;
				}

				return vertexElementCount;
			}

			/**
			 * @brief Writes the first value from a vector to a vertex buffer at a specific offset.
			 * @tparam vec_dim_t The dimension of the vector.
			 * @param vector A reference to a vector.
			 * @param vertexBuffer A reference to the vertex buffer.
			 * @param offset A reference to an offset.
			 * @return void
			 */
			template< size_t vec_dim_t >
			static
			void
			writeVector1ToBuffer (const Math::Vector< vec_dim_t, vertex_data_t > & vector, std::vector< vertex_data_t > & vertexBuffer, index_data_t & offset) noexcept
			{
				vertexBuffer[offset++] = vector[Math::X];
			}

			/**
			 * @brief Writes the 2 first values from a vector to a vertex buffer at a specific offset.
			 * @tparam vec_dim_t The dimension of the vector.
			 * @param vector A reference to a vector.
			 * @param vertexBuffer A reference to the vertex buffer.
			 * @param offset A reference to an offset.
			 * @return void
			 */
			template< size_t vec_dim_t >
			static
			void
			writeVector2ToBuffer (const Math::Vector< vec_dim_t, vertex_data_t > & vector, std::vector< vertex_data_t > & vertexBuffer, index_data_t & offset) noexcept requires ( vec_dim_t == 2UL || vec_dim_t == 3UL || vec_dim_t == 4UL )
			{
				vertexBuffer[offset++] = vector[Math::X];
				vertexBuffer[offset++] = vector[Math::Y];
			}

			/**
			 * @brief Writes the 3 first values from a vector to a vertex buffer at a specific offset.
			 * @tparam vec_dim_t The dimension of the vector.
			 * @param vector A reference to a vector.
			 * @param vertexBuffer A reference to the vertex buffer.
			 * @param offset A reference to an offset.
			 * @return void
			 */
			template< size_t vec_dim_t >
			static
			void
			writeVector3ToBuffer (const Math::Vector< vec_dim_t, vertex_data_t > & vector, std::vector< vertex_data_t > & vertexBuffer, index_data_t & offset) noexcept requires ( vec_dim_t == 3UL || vec_dim_t == 4UL )
			{
				vertexBuffer[offset++] = vector[Math::X];
				vertexBuffer[offset++] = vector[Math::Y];
				vertexBuffer[offset++] = vector[Math::Z];
			}

			/**
			 * @brief Writes a vector 4 to a vertex buffer at a specific offset.
			 * @param vector A reference to a vector.
			 * @param vertexBuffer A reference to the vertex buffer.
			 * @param offset A reference to an offset.
			 * @return void
			 */
			static
			void
			writeVector4ToBuffer (const Math::Vector< 4, vertex_data_t > & vector, std::vector< vertex_data_t > & vertexBuffer, index_data_t & offset) noexcept
			{
				vertexBuffer[offset++] = vector[Math::X];
				vertexBuffer[offset++] = vector[Math::Y];
				vertexBuffer[offset++] = vector[Math::Z];
				vertexBuffer[offset++] = vector[Math::W];
			}

			/**
			 * @brief Writes the texture coordinates vertex attributes to a vertex buffer at a specific offset.
			 * @param textureCoordinates A reference to a vector.
			 * @param size The element count wanted.
			 * @param vertexBuffer A reference to the vertex buffer.
			 * @param offset A reference to an offset.
			 * @return void
			 */
			static
			void
			writeTextureCoordinatesToVertexBuffer (const Math::Vector< 3, vertex_data_t > & textureCoordinates, uint32_t size, std::vector< vertex_data_t > & vertexBuffer, index_data_t & offset)
			{
				switch ( size )
				{
					case 2 :
						vertexBuffer[offset++] = textureCoordinates[Math::X];
						vertexBuffer[offset++] = textureCoordinates[Math::Y];
						break;

					case 3 :
						vertexBuffer[offset++] = textureCoordinates[Math::X];
						vertexBuffer[offset++] = textureCoordinates[Math::Y];
						vertexBuffer[offset++] = textureCoordinates[Math::Z];
						break;

					case 4 :
						vertexBuffer[offset++] = textureCoordinates[Math::X];
						vertexBuffer[offset++] = textureCoordinates[Math::Y];
						vertexBuffer[offset++] = textureCoordinates[Math::Z];
						vertexBuffer[offset++] = 0;
						break;

					default:
						break;
				}
			}

			/**
			 * @brief Writes the color vertex attributes to a vertex buffer at a specific offset.
			 * @param vertexColor A reference to a vector.
			 * @param size The element count wanted.
			 * @param vertexBuffer A reference to the vertex buffer.
			 * @param offset A reference to an offset.
			 * @return void
			 */
			static
			void
			writeColorToVertexBuffer (const Math::Vector< 4, vertex_data_t > & vertexColor, uint32_t size, std::vector< vertex_data_t > & vertexBuffer, index_data_t & offset)
			{
				switch ( size )
				{
					case 1 :
						vertexBuffer[offset++] = vertexColor[Math::R];
						break;

					case 2 :
						vertexBuffer[offset++] = vertexColor[Math::R];
						vertexBuffer[offset++] = vertexColor[Math::G];
						break;

					case 3 :
						vertexBuffer[offset++] = vertexColor[Math::R];
						vertexBuffer[offset++] = vertexColor[Math::G];
						vertexBuffer[offset++] = vertexColor[Math::B];
						break;

					case 4 :
						vertexBuffer[offset++] = vertexColor[Math::R];
						vertexBuffer[offset++] = vertexColor[Math::G];
						vertexBuffer[offset++] = vertexColor[Math::B];
						vertexBuffer[offset++] = vertexColor[Math::A];
						break;

					default:
						break;
				}
			}

			/**
			 * @brief Writes the influence attributes to a vertex buffer at a specific offset.
			 * @param influence A reference to a vector.
			 * @param size The element count wanted.
			 * @param vertexBuffer A reference to the vertex buffer.
			 * @param offset A reference to an offset.
			 * @return void
			 */
			static
			void
			writeInfluenceToVertexBuffer (const Math::Vector< 4, int32_t > & influence, uint32_t size, std::vector< vertex_data_t > & vertexBuffer, index_data_t & offset)
			{
				switch ( size )
				{
					case 1 :
						vertexBuffer[offset++] = static_cast< vertex_data_t >(influence[Math::X]);
						break;

					case 2 :
						vertexBuffer[offset++] = static_cast< vertex_data_t >(influence[Math::X]);
						vertexBuffer[offset++] = static_cast< vertex_data_t >(influence[Math::Y]);
						break;

					case 3 :
						vertexBuffer[offset++] = static_cast< vertex_data_t >(influence[Math::X]);
						vertexBuffer[offset++] = static_cast< vertex_data_t >(influence[Math::Y]);
						vertexBuffer[offset++] = static_cast< vertex_data_t >(influence[Math::Z]);
						break;

					case 4 :
						vertexBuffer[offset++] = static_cast< vertex_data_t >(influence[Math::X]);
						vertexBuffer[offset++] = static_cast< vertex_data_t >(influence[Math::Y]);
						vertexBuffer[offset++] = static_cast< vertex_data_t >(influence[Math::Z]);
						vertexBuffer[offset++] = static_cast< vertex_data_t >(influence[Math::W]);
						break;

					default:
						break;
				}
			}

			/** @brief A position, normal or texture coordinate snapped to the merge grid. */
			using QuantizedVector = std::array< int64_t, 3 >;

			/**
			 * @brief A vertex snapped to the merge grid.
			 * @note ⚠️ Same quantisation as ShapeProcessor::deduplicateVertices(), on purpose: the two
			 * merge paths of the library must not disagree about what "the same vertex" means.
			 */
			struct VertexKey final
			{
				QuantizedVector position{};
				QuantizedVector normal{};
				QuantizedVector textureCoordinates{};

				bool operator== (const VertexKey & other) const noexcept = default;
			};

			/** @brief A vertex color snapped to the merge grid. */
			struct ColorKey final
			{
				QuantizedVector rgb{};
				int64_t alpha{0};

				bool operator== (const ColorKey & other) const noexcept = default;
			};

			/** @brief Hashes a quantised vertex or color key. */
			struct QuantizedHash final
			{
				/**
				 * @brief Folds one quantised component into a running hash.
				 * @param seed A reference to the running hash.
				 * @param value The component.
				 * @return void
				 */
				static
				void
				combine (size_t & seed, int64_t value) noexcept
				{
					seed ^= std::hash< int64_t >{}(value) + 0x9E3779B9UL + (seed << 6U) + (seed >> 2U);
				}

				/**
				 * @brief Returns the hash of a vertex key.
				 * @param key A reference to the key.
				 * @return size_t
				 */
				[[nodiscard]]
				size_t
				operator() (const VertexKey & key) const noexcept
				{
					size_t seed = 0;

					for ( const auto & component : {key.position, key.normal, key.textureCoordinates} )
					{
						combine(seed, component[0]);
						combine(seed, component[1]);
						combine(seed, component[2]);
					}

					return seed;
				}

				/**
				 * @brief Returns the hash of a color key.
				 * @param key A reference to the key.
				 * @return size_t
				 */
				[[nodiscard]]
				size_t
				operator() (const ColorKey & key) const noexcept
				{
					size_t seed = 0;

					combine(seed, key.rgb[0]);
					combine(seed, key.rgb[1]);
					combine(seed, key.rgb[2]);
					combine(seed, key.alpha);

					return seed;
				}
			};

			/**
			 * @brief Snaps a value to the merge grid.
			 * @param value The value.
			 * @return int64_t
			 */
			[[nodiscard]]
			int64_t
			quantizeScalar (vertex_data_t value) const noexcept
			{
				return static_cast< int64_t >(std::round(value / m_mergeTolerance));
			}

			/**
			 * @brief Snaps a vector to the merge grid.
			 * @param vector A reference to the vector.
			 * @return QuantizedVector
			 */
			[[nodiscard]]
			QuantizedVector
			quantize (const Math::Vector< 3, vertex_data_t > & vector) const noexcept
			{
				return {this->quantizeScalar(vector[Math::X]), this->quantizeScalar(vector[Math::Y]), this->quantizeScalar(vector[Math::Z])};
			}

			/**
			 * @brief The unordered vertex index pair identifying an edge, whatever the winding of the
			 * triangle that declared it.
			 */
			struct EdgeKey final
			{
				index_data_t first{0};
				index_data_t second{0};

				/**
				 * @brief Compares two edge keys.
				 * @param other A reference to the other key.
				 * @return bool
				 */
				[[nodiscard]]
				bool
				operator== (const EdgeKey & other) const noexcept
				{
					return first == other.first && second == other.second;
				}
			};

			/**
			 * @brief Hashes an edge key.
			 */
			struct EdgeKeyHash final
			{
				/**
				 * @brief Returns the hash of an edge key.
				 * @param key A reference to the key.
				 * @return size_t
				 */
				[[nodiscard]]
				size_t
				operator() (const EdgeKey & key) const noexcept
				{
					const auto hashA = std::hash< index_data_t >{}(key.first);
					const auto hashB = std::hash< index_data_t >{}(key.second);

					return hashA ^ (hashB + 0x9E3779B9UL + (hashA << 6U) + (hashA >> 2U));
				}
			};

			/**
			 * @brief The half-edge already inserted for an edge key, and whether its mate has been
			 * inserted too.
			 */
			struct EdgeSlot final
			{
				index_data_t index{0};
				bool paired{false};
			};

			/**
			 * @brief Rebuilds the construction-time indexes from the stored data when they were released.
			 * @note The edge slots are replayed in insertion order: each key keeps its FIRST half-edge, and is
			 * paired once a second one exists — the state addEdge() left.
			 * @return void
			 */
			void
			restoreConstructionIndexes () noexcept
			{
				if ( !m_constructionIndexesReleased )
				{
					return;
				}

				m_constructionIndexesReleased = false;

				m_vertexIndex.reserve(m_vertices.size());

				for ( size_t index = 0; index < m_vertices.size(); ++index )
				{
					const auto & vertex = m_vertices[index];

					m_vertexIndex.try_emplace(VertexKey{this->quantize(vertex.position()), this->quantize(vertex.normal()), this->quantize(vertex.textureCoordinates())}, static_cast< index_data_t >(index));
				}

				m_vertexColorIndex.reserve(m_vertexColors.size());

				for ( size_t index = 0; index < m_vertexColors.size(); ++index )
				{
					const auto & color = m_vertexColors[index];

					m_vertexColorIndex.try_emplace(ColorKey{this->quantize(Math::Vector< 3, vertex_data_t >{color[Math::X], color[Math::Y], color[Math::Z]}), this->quantizeScalar(color[Math::W])}, static_cast< index_data_t >(index));
				}

				for ( size_t index = 0; index < m_edges.size(); ++index )
				{
					const auto & edge = m_edges[index];
					const EdgeKey key{std::min(edge.vertexIndexA(), edge.vertexIndexB()), std::max(edge.vertexIndexA(), edge.vertexIndexB())};

					if ( const auto [slotIt, inserted] = m_unpairedEdges.try_emplace(key, EdgeSlot{static_cast< index_data_t >(index), false}); !inserted )
					{
						slotIt->second.paired = true;
					}
				}
			}

			/**
			 * @brief Reads one vertex written by createIndexedVertexBuffer() / createVertexBuffer(), in their order.
			 * @param vertexBuffer The vertex attributes.
			 * @param offset The vertex's first element, advanced past it.
			 * @param vertex A reference to the vertex to fill.
			 * @param vertexColor A pointer to the colour to fill, or nullptr without colours.
			 * @param normalType The normal format.
			 * @param textureCoordinatesType The texture coordinates format.
			 * @param vertexColorType The vertex colour format.
			 * @param skeletalAnimationType The skeletal attributes.
			 * @param secondaryTextureCoordinatesType The secondary texture coordinates format.
			 * @return void
			 */
			static
			void
			readVertex (std::span< const vertex_data_t > vertexBuffer, size_t & offset, ShapeVertex< vertex_data_t > & vertex, Math::Vector< 4, vertex_data_t > * vertexColor, NormalType normalType, TextureCoordinatesType textureCoordinatesType, VertexColorType vertexColorType, SkeletalAnimationType skeletalAnimationType, TextureCoordinatesType secondaryTextureCoordinatesType) noexcept
			{
				const auto next = [&vertexBuffer, &offset] () {
					return vertexBuffer[offset++];
				};
				const auto next3 = [&next] () {
					const auto x = next();
					const auto y = next();
					const auto z = next();

					return Math::Vector< 3, vertex_data_t >{x, y, z};
				};

				vertex.setPosition(next3());

				switch ( normalType )
				{
					case NormalType::Normal :
						vertex.setNormal(next3());
						break;

					case NormalType::TangentNormal :
						vertex.setTangent(next3());
						vertex.setNormal(next3());
						break;

					case NormalType::TBNSpace :
					{
						const auto tangent = next3();
						const auto biNormal = next3();
						const auto normal = next3();

						vertex.setTangent(tangent);
						vertex.setNormal(normal);
						/* NOTE: The buffer holds the SIGNED bitangent: its handedness comes back from the sign. */
						vertex.setTangentHandedness(Math::Vector< 3, vertex_data_t >::dotProduct(Math::Vector< 3, vertex_data_t >::crossProduct(normal, tangent), biNormal) < 0 ? static_cast< vertex_data_t >(-1) : static_cast< vertex_data_t >(1));
						break;
					}

					case NormalType::None :
						break;
				}

				switch ( textureCoordinatesType )
				{
					case TextureCoordinatesType::UV :
					{
						const auto u = next();
						const auto v = next();

						vertex.setTextureCoordinates(Math::Vector< 2, vertex_data_t >{u, v});
						break;
					}

					case TextureCoordinatesType::UVW :
						vertex.setTextureCoordinates(next3());
						break;

					case TextureCoordinatesType::None :
						break;
				}

				switch ( secondaryTextureCoordinatesType )
				{
					case TextureCoordinatesType::UV :
					case TextureCoordinatesType::UVW :
					{
						const auto u = next();
						const auto v = next();

						vertex.setSecondaryTextureCoordinates(Math::Vector< 2, vertex_data_t >{u, v});

						/* NOTE: The writer pads a 3D request with W = 0. */
						if ( secondaryTextureCoordinatesType == TextureCoordinatesType::UVW )
						{
							++offset;
						}
						break;
					}

					case TextureCoordinatesType::None :
						break;
				}

				switch ( vertexColorType )
				{
					case VertexColorType::Gray :
					{
						const auto gray = next();

						*vertexColor = {gray, gray, gray, static_cast< vertex_data_t >(1)};
						break;
					}

					case VertexColorType::RGB :
					{
						const auto rgb = next3();

						*vertexColor = {rgb[Math::X], rgb[Math::Y], rgb[Math::Z], static_cast< vertex_data_t >(1)};
						break;
					}

					case VertexColorType::RGBA :
					{
						const auto rgb = next3();
						const auto alpha = next();

						*vertexColor = {rgb[Math::X], rgb[Math::Y], rgb[Math::Z], alpha};
						break;
					}

					case VertexColorType::None :
						break;
				}

				const auto influence = [&next] () {
					return static_cast< int32_t >(std::lround(next()));
				};

				switch ( skeletalAnimationType )
				{
					case SkeletalAnimationType::Average3 :
					{
						const auto a = influence();
						const auto b = influence();
						const auto c = influence();

						vertex.setInfluences(a, b, c);
						break;
					}

					case SkeletalAnimationType::Average4 :
					{
						const auto a = influence();
						const auto b = influence();
						const auto c = influence();
						const auto d = influence();

						vertex.setInfluences(a, b, c, d);
						break;
					}

					case SkeletalAnimationType::Weighted3 :
					{
						const auto a = influence();
						const auto b = influence();
						const auto c = influence();
						const auto weights = next3();

						vertex.setInfluences(a, b, c);
						vertex.setWeights(weights[Math::X], weights[Math::Y], weights[Math::Z]);
						break;
					}

					case SkeletalAnimationType::Weighted4 :
					{
						const auto a = influence();
						const auto b = influence();
						const auto c = influence();
						const auto d = influence();
						const auto wa = next();
						const auto wb = next();
						const auto wc = next();
						const auto wd = next();

						vertex.setInfluences(a, b, c, d);
						vertex.setWeights(wa, wb, wc, wd);
						break;
					}

					case SkeletalAnimationType::None :
						break;
				}
			}

			/**
			 * @brief Ends a read*VertexBuffer(): the groups, the declared attributes, the bounds.
			 * @param groups The groups read, (first triangle, triangle count); empty for one group.
			 * @param triangleCount The number of triangles read.
			 * @param textureCoordinatesDeclared Whether the buffer held texture coordinates.
			 * @param normalsDeclared Whether the buffer held normals.
			 * @return bool False (the shape emptied) when a group lies beyond the triangles.
			 */
			[[nodiscard]]
			bool
			finishReading (const std::vector< std::pair< index_data_t, index_data_t > > & groups, size_t triangleCount, bool textureCoordinatesDeclared, bool normalsDeclared) noexcept
			{
				m_groups.clear();

				if ( groups.empty() )
				{
					m_groups.emplace_back(index_data_t{0}, static_cast< index_data_t >(triangleCount));
				}
				else
				{
					for ( const auto & [first, count] : groups )
					{
						if ( static_cast< size_t >(first) + count > triangleCount )
						{
							this->clear();

							return false;
						}

						m_groups.emplace_back(first, count);
					}
				}

				m_textureCoordinatesDeclared = textureCoordinatesDeclared;
				m_normalsDeclared = normalsDeclared;

				this->updateProperties();

				return true;
			}

			/**
			 * @brief Estimates the bytes of a node-based hash index: one pointer per bucket, one node per element
			 * (the value, a link to the next node and a cached hash).
			 * @tparam map_t The type of the unordered map.
			 * @param map A reference to the map.
			 * @return size_t
			 */
			template< typename map_t >
			[[nodiscard]]
			static
			size_t
			hashIndexBytes (const map_t & map) noexcept
			{
				return (map.bucket_count() * sizeof(void *)) + (map.size() * (sizeof(typename map_t::value_type) + sizeof(void *) + sizeof(size_t)));
			}

			/* Flag names. */
			static constexpr auto TextureCoordinatesDeclared{0UL};
			static constexpr auto ComputeEdges{1UL};

			/* NOTE: first = offset, second = the number of vertices for this group. */
			std::vector< std::pair< index_data_t, index_data_t > > m_groups{1};
			std::vector< ShapeVertex< vertex_data_t > > m_vertices;
			std::vector< Math::Vector< 4, vertex_data_t > > m_vertexColors;
			std::vector< ShapeTriangle< vertex_data_t, index_data_t > > m_triangles;
			std::vector< ShapeEdge< index_data_t > > m_edges;
			/* NOTE: Construction-time index only, it holds no geometry: addEdge() uses it to pair the
			 * two half-edges of a shared edge in constant time. */
			std::unordered_map< EdgeKey, EdgeSlot, EdgeKeyHash > m_unpairedEdges;
			/* NOTE: Construction-time indexes only, they hold no geometry: addVertex() and
			 * addVertexColor() use them to merge in constant time instead of scanning everything
			 * already stored. */
			std::unordered_map< VertexKey, index_data_t, QuantizedHash > m_vertexIndex;
			std::unordered_map< ColorKey, index_data_t, QuantizedHash > m_vertexColorIndex;
			std::vector< BoundaryLoop< index_data_t > > m_boundaryLoops;
			Math::Space3D::AACuboid< vertex_data_t > m_boundingBox;
			Math::Space3D::Sphere< vertex_data_t > m_boundingSphere;
			/* NOTE: This is the max distance between [0,0,0] and the farthest vertex.
			 * This is different from the fourth component of the centroid (m_boundingSphere). */
			vertex_data_t m_farthestDistance{0};
			vertex_data_t m_mergeTolerance{static_cast< vertex_data_t >(1e-4)};
			bool m_textureCoordinatesDeclared{false};
			bool m_normalsDeclared{false};
			bool m_computeEdges{false};
			bool m_boundaryLoopsAnalyzed{false};
			bool m_constructionIndexesReleased{false};
	};
}
