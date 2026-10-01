/*
 * src/Math/Space3D/Contacts/BoxTriangle.hpp
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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

/* Local inclusions for usages. */
#include "Math/Space3D/OrientedBox.hpp"
#include "Math/Space3D/Triangle.hpp"
#include "Math/Vector.hpp"
#include "StaticVector.hpp"
#include "BoxBox.hpp"
#include "ContactManifold.hpp"
#include "SphereTriangle.hpp"

/*
 * Oriented box ↔ triangle contact generation (a box resting on a terrain or a mesh). The triangle is TWO-SIDED.
 * The separating-axis test on 13 axes — the triangle's normal, the box's 3 face normals, the 9 cross products of a box
 * axis and a triangle edge — biased towards the triangle's face, then the box's faces (Gregorius, GDC 2013). A face
 * axis clips: the triangle's face clips the box's incident face by its 3 edge planes; a box face clips the triangle by
 * its 4 side planes (Sutherland-Hodgman); the points are reduced to 4. An edge axis gives the point between the two
 * closest edges. References: D. Gregorius, GDC 2013 / 2015; C. Ericson, "Real-Time Collision Detection" (2005), § 5.2.9
 * (OBB vs triangle SAT). No third-party code.
 */

namespace EmEn::Base::Math::Space3D
{
	/**
	 * @brief Generates the contact manifold of an oriented box (A) and a triangle (B): 1 to 4 points.
	 * @note The normal points FROM the box TO the triangle. A degenerate (collinear) triangle gives no contact.
	 * @note Feature ids: 0x10000000 | box incident face << 16 | clipped point key (the triangle's face clips the box);
	 * 0x20000000 | box reference face << 16 | clipped point key (a box face clips the triangle); 0x80000000 | box axis
	 * << 8 | triangle edge for an edge contact.
	 * @param box A reference to the box (A). @pre box.isValid().
	 * @param triangle A reference to the triangle (B).
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const OrientedBox< precision_t > & box, const Triangle< precision_t > & triangle, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		using Vec3 = Vector< 3, precision_t >;
		using namespace BoxBoxDetail;

		constexpr auto BoxTriangleEdgeBias = static_cast< precision_t >(0.95);
		constexpr auto BoxTriangleFaceBias = static_cast< precision_t >(0.98);
		constexpr auto BoxTriangleAbsoluteBias = static_cast< precision_t >(1.0e-4);
		constexpr auto BoxTriangleParallelEdge = static_cast< precision_t >(1.0e-6);
		constexpr auto BoxTriangleHalf = static_cast< precision_t >(0.5);

		manifold.clear();

		Vec3 triangleNormal;

		if ( !TriangleDetail::unitNormal(triangle, triangleNormal) )
		{
			return false;
		}

		const std::array< Vec3, 3 > vertices{triangle.pointA(), triangle.pointB(), triangle.pointC()};
		/* Each edge with its start, in winding order: AB, BC, CA. */
		const std::array< std::pair< Vec3, Vec3 >, 3 > edges{{
			{triangle.pointA(), triangle.pointB() - triangle.pointA()},
			{triangle.pointB(), triangle.pointC() - triangle.pointB()},
			{triangle.pointC(), triangle.pointA() - triangle.pointC()}
		}};

		/* The separation of the box and the triangle along a unit axis (positive = apart), and the side of the box the
		 * triangle lies on along it (+1 or -1) — read from the projections, never from a centroid: on a large sloped
		 * triangle the centroid can stand above the box while the box rests on a lower part of it. */
		struct AxisSeparation final
		{
			precision_t separation{0};
			precision_t triangleSide{1};
		};

		const auto separationAlong = [&box, &vertices] (const Vec3 & axis) {
			const precision_t boxCenter = Vec3::dotProduct(box.center(), axis);
			const precision_t boxRadius = box.projectedRadius(axis);
			precision_t triangleMin = std::numeric_limits< precision_t >::max();
			precision_t triangleMax = -std::numeric_limits< precision_t >::max();

			for ( const auto & vertex : vertices )
			{
				const precision_t projection = Vec3::dotProduct(vertex, axis);

				triangleMin = std::min(triangleMin, projection);
				triangleMax = std::max(triangleMax, projection);
			}

			const precision_t abovePositive = triangleMin - (boxCenter + boxRadius);
			const precision_t belowNegative = (boxCenter - boxRadius) - triangleMax;

			return abovePositive >= belowNegative ? AxisSeparation{abovePositive, static_cast< precision_t >(1)} : AxisSeparation{belowNegative, static_cast< precision_t >(-1)};
		};

		/* 1. The triangle's face. */
		const precision_t triangleFaceSeparation = separationAlong(triangleNormal).separation;

		if ( triangleFaceSeparation > 0 )
		{
			return false;
		}

		/* 2. The box's faces. */
		precision_t boxFaceSeparation = -std::numeric_limits< precision_t >::max();
		precision_t boxFaceSide = 1;
		size_t boxFaceAxis = 0;

		for ( size_t index = 0; index < 3; ++index )
		{
			const auto [separation, triangleSide] = separationAlong(box.axis(index));

			if ( separation > 0 )
			{
				return false;
			}

			if ( separation > boxFaceSeparation )
			{
				boxFaceSeparation = separation;
				boxFaceSide = triangleSide;
				boxFaceAxis = index;
			}
		}

		/* 3. The edges. */
		precision_t edgeSeparation = -std::numeric_limits< precision_t >::max();
		size_t edgeBoxAxis = 0;
		size_t edgeTriangleEdge = 0;
		Vec3 edgeAxis;
		Vec3 bestEdgeStart;
		Vec3 bestEdgeVector;

		for ( size_t boxIndex = 0; boxIndex < 3; ++boxIndex )
		{
			size_t edgeIndex = 0;

			for ( const auto & [start, edge] : edges )
			{
				auto axis = Vec3::crossProduct(box.axis(boxIndex), edge);
				const precision_t lengthSquared = axis.lengthSquared();

				if ( lengthSquared > BoxTriangleParallelEdge * edge.lengthSquared() )
				{
					axis *= static_cast< precision_t >(1) / std::sqrt(lengthSquared);

					const auto [separation, triangleSide] = separationAlong(axis);

					if ( separation > 0 )
					{
						return false;
					}

					if ( separation > edgeSeparation )
					{
						edgeSeparation = separation;
						edgeBoxAxis = boxIndex;
						edgeTriangleEdge = edgeIndex;
						bestEdgeStart = start;
						bestEdgeVector = edge;
						/* Oriented from the box towards the triangle. */
						edgeAxis = axis * triangleSide;
					}
				}

				++edgeIndex;
			}
		}

		const precision_t bestFaceSeparation = std::max(triangleFaceSeparation, boxFaceSeparation);

		/* 4. Edge ↔ edge: one point between the box edge and the triangle edge. */
		if ( edgeSeparation > (BoxTriangleEdgeBias * bestFaceSeparation) + BoxTriangleAbsoluteBias )
		{
			/* The box edge parallel to edgeBoxAxis on the side facing the triangle. */
			Vec3 boxEdgeCenter = box.center();

			for ( size_t index = 0; index < 3; ++index )
			{
				if ( index != edgeBoxAxis )
				{
					const precision_t side = signOf(Vec3::dotProduct(box.axis(index), edgeAxis));

					boxEdgeCenter += box.axis(index) * (side * box.halfExtent(index));
				}
			}

			const Vec3 & edgeStart = bestEdgeStart;
			const Vec3 & edgeVector = bestEdgeVector;
			const precision_t edgeLength = edgeVector.length();

			Vec3 onBox;
			Vec3 onTriangle;

			closestPointsOfSegments(boxEdgeCenter, box.axis(edgeBoxAxis), box.halfExtent(edgeBoxAxis), edgeStart + (edgeVector * BoxTriangleHalf), edgeVector * (static_cast< precision_t >(1) / edgeLength), edgeLength * BoxTriangleHalf, onBox, onTriangle);

			manifold.setNormal(edgeAxis);
			manifold.addPoint({(onBox + onTriangle) * BoxTriangleHalf, -edgeSeparation, 0x80000000U | (static_cast< uint32_t >(edgeBoxAxis) << 8U) | static_cast< uint32_t >(edgeTriangleEdge)});

			return true;
		}

		StaticVector< Candidate< precision_t >, 8 > candidates;
		Vec3 manifoldNormal;

		if ( boxFaceSeparation > (BoxTriangleFaceBias * triangleFaceSeparation) + BoxTriangleAbsoluteBias )
		{
			/* 5a. A box face is the reference: clip the triangle by its 4 side planes. */
			const Vec3 referenceNormal = box.axis(boxFaceAxis) * boxFaceSide;
			const auto referenceFace = static_cast< uint32_t >((boxFaceAxis * 2U) + (boxFaceSide > 0 ? 0U : 1U));

			ClipPolygon< precision_t > polygon;
			ClipPolygon< precision_t > clipped;
			uint8_t vertexIndex = 0;

			for ( const auto & vertex : vertices )
			{
				polygon.push_back({vertex, vertexIndex, vertexIndex});

				++vertexIndex;
			}

			uint8_t planeIndex = 0;

			for ( size_t sideAxisOffset = 1; sideAxisOffset <= 2; ++sideAxisOffset )
			{
				const size_t sideAxis = (boxFaceAxis + sideAxisOffset) % 3;
				const auto & direction = box.axis(sideAxis);
				const precision_t centerProjection = Vec3::dotProduct(direction, box.center());
				const precision_t halfExtent = box.halfExtent(sideAxis);

				clipByPlane(polygon, direction, centerProjection + halfExtent, planeIndex++, clipped);
				clipByPlane(clipped, -direction, -centerProjection + halfExtent, planeIndex++, polygon);
			}

			const precision_t referencePlane = Vec3::dotProduct(referenceNormal, box.center()) + box.halfExtent(boxFaceAxis);

			for ( const auto & vertex : polygon )
			{
				const precision_t separation = Vec3::dotProduct(referenceNormal, vertex.position) - referencePlane;

				if ( separation <= 0 && !candidates.full() )
				{
					candidates.push_back({vertex.position - (referenceNormal * (separation * BoxTriangleHalf)), -separation, 0x20000000U | (referenceFace << 16U) | vertex.key});
				}
			}

			manifoldNormal = referenceNormal;
		}
		else
		{
			/* 5b. The triangle's face is the reference, on the box's side: clip the box's incident face by its edges. */
			const Vec3 faceTowardsBox = Vec3::dotProduct(box.center() - vertices[0], triangleNormal) >= 0 ? triangleNormal : -triangleNormal;

			size_t incidentAxis = 0;
			precision_t mostAntiParallel = 0;

			for ( size_t index = 0; index < 3; ++index )
			{
				const precision_t alignment = std::abs(Vec3::dotProduct(box.axis(index), faceTowardsBox));

				if ( alignment > mostAntiParallel )
				{
					mostAntiParallel = alignment;
					incidentAxis = index;
				}
			}

			const precision_t incidentSide = -signOf(Vec3::dotProduct(box.axis(incidentAxis), faceTowardsBox));
			const auto incidentFace = static_cast< uint32_t >((incidentAxis * 2U) + (incidentSide > 0 ? 0U : 1U));
			const size_t tangentU = (incidentAxis + 1) % 3;
			const size_t tangentV = (incidentAxis + 2) % 3;
			const Vec3 faceCenter = box.center() + (box.axis(incidentAxis) * (incidentSide * box.halfExtent(incidentAxis)));
			const Vec3 stepU = box.axis(tangentU) * box.halfExtent(tangentU);
			const Vec3 stepV = box.axis(tangentV) * box.halfExtent(tangentV);

			ClipPolygon< precision_t > polygon;
			ClipPolygon< precision_t > clipped;
			uint8_t cornerIndex = 0;

			for ( const auto & corner : {faceCenter + stepU + stepV, faceCenter - stepU + stepV, faceCenter - stepU - stepV, faceCenter + stepU - stepV} )
			{
				polygon.push_back({corner, cornerIndex, cornerIndex});

				++cornerIndex;
			}

			/* The 3 edge planes, normals pointing OUT of the triangle (the winding normal makes cross(edge, n) point out). */
			uint8_t planeIndex = 0;

			for ( const auto & [start, edge] : edges )
			{
				const Vec3 outward = Vec3::crossProduct(edge, triangleNormal);

				clipByPlane(polygon, outward, Vec3::dotProduct(outward, start), planeIndex++, clipped);
				std::swap(polygon, clipped);
			}

			const precision_t trianglePlane = Vec3::dotProduct(faceTowardsBox, vertices[0]);

			for ( const auto & vertex : polygon )
			{
				const precision_t separation = Vec3::dotProduct(faceTowardsBox, vertex.position) - trianglePlane;

				if ( separation <= 0 && !candidates.full() )
				{
					candidates.push_back({vertex.position - (faceTowardsBox * (separation * BoxTriangleHalf)), -separation, 0x10000000U | (incidentFace << 16U) | vertex.key});
				}
			}

			/* The triangle's face points towards the box: the normal from the box to the triangle is its opposite. */
			manifoldNormal = -faceTowardsBox;
		}

		if ( candidates.empty() )
		{
			return false;
		}

		manifold.setNormal(manifoldNormal);

		reduceToFour(candidates, manifoldNormal, manifold);

		return true;
	}

	/**
	 * @brief Generates the contact manifold of a triangle (A) and an oriented box (B): the box ↔ triangle one, flipped.
	 * @param triangle A reference to the triangle (A).
	 * @param box A reference to the box (B). @pre box.isValid().
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const Triangle< precision_t > & triangle, const OrientedBox< precision_t > & box, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		if ( !computeContactManifold(box, triangle, manifold) )
		{
			return false;
		}

		manifold.flip();

		return true;
	}
}
