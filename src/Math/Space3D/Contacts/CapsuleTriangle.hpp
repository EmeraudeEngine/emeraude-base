/*
 * src/Math/Space3D/Contacts/CapsuleTriangle.hpp
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
#include "Math/Space3D/Capsule.hpp"
#include "Math/Space3D/Triangle.hpp"
#include "Math/Vector.hpp"
#include "BoxBox.hpp"
#include "ContactManifold.hpp"
#include "SphereTriangle.hpp"

/*
 * Capsule ↔ triangle contact generation. The triangle is TWO-SIDED.
 *  - The EXACT closest points of the capsule's segment and the triangle: the segment piercing the triangle (distance
 *    0), else the best of its two ends against the triangle and of the segment against the three edges (Ericson
 *    § 5.1.9 / § 5.1.10).
 *  - Shallow (outside, nearer than the radius): one point; two when the closest feature is the face and the segment
 *    lies along it (within ~3°) — the segment clipped to the triangle's prism.
 *  - Deep (the segment touches or pierces the triangle): pushed along the face normal TOWARDS THE SIDE OF THE CAPSULE'S
 *    CENTRE, the segment clipped to the prism, each end's depth measured from the plane. (The overlap test of
 *    `Collisions/CapsuleTriangle.hpp` pushed a piercing capsule by the radius whatever its side and depth: item
 *    `collision-pair-test-defects`.)
 * No third-party code.
 */

namespace EmEn::Base::Math::Space3D
{
	namespace CapsuleTriangleDetail
	{
		/** @brief The closest points of a segment and a triangle. */
		template< typename precision_t >
		struct SegmentTriangleClosest final
		{
			Vector< 3, precision_t > onSegment;
			Vector< 3, precision_t > onTriangle;
			/** 0 when the segment pierces (or touches) the triangle. */
			precision_t distanceSquared{0};
			/** The triangle feature: a `TriangleDetail::Region` for an end of the segment, 0x10 | edge (AB 0, BC 1, CA 2). */
			uint32_t feature{0};
			bool pierces{false};
			/** Whether the closest triangle point lies inside the face (not on an edge or a vertex). */
			bool isFace{false};
		};

		/**
		 * @brief The EXACT closest points of a segment and a triangle: the segment piercing it (distance 0), else the
		 * best of its two ends against the triangle and of the segment against the three edges (Ericson § 5.1.10).
		 * @pre faceNormal is the triangle's unit winding normal (TriangleDetail::unitNormal() answered true).
		 * @param start A reference to the segment start.
		 * @param end A reference to the segment end.
		 * @param triangle A reference to the triangle.
		 * @param faceNormal A reference to its unit winding normal.
		 * @return SegmentTriangleClosest< precision_t >
		 */
		template< typename precision_t >
		[[nodiscard]]
		SegmentTriangleClosest< precision_t >
		closestOfSegmentAndTriangle (const Vector< 3, precision_t > & start, const Vector< 3, precision_t > & end, const Triangle< precision_t > & triangle, const Vector< 3, precision_t > & faceNormal) noexcept
		{
			using Vec3 = Vector< 3, precision_t >;
			using TriangleDetail::Region;

			constexpr auto Half = static_cast< precision_t >(0.5);
			constexpr auto TouchThreshold = static_cast< precision_t >(1.0e-6);

			SegmentTriangleClosest< precision_t > best;

			const Vec3 segmentCenter = (start + end) * Half;
			Vec3 direction = end - start;
			const precision_t length = direction.length();
			const precision_t halfLength = length > TouchThreshold ? length * Half : static_cast< precision_t >(0);

			if ( halfLength > 0 )
			{
				direction *= static_cast< precision_t >(1) / length;
			}

			/* 1. Does the segment pierce (or touch) the triangle? */
			const precision_t startHeight = Vec3::dotProduct(start - triangle.pointA(), faceNormal);
			const precision_t endHeight = Vec3::dotProduct(end - triangle.pointA(), faceNormal);

			if ( (startHeight <= 0 && endHeight >= 0) || (startHeight >= 0 && endHeight <= 0) )
			{
				const precision_t span = startHeight - endHeight;
				/* A segment in the plane (span 0) is tested at its middle; the edge pairs below catch the rest. */
				const Vec3 crossing = std::abs(span) > TouchThreshold ? start + ((end - start) * (startHeight / span)) : segmentCenter;
				Region crossingRegion{Region::Face};
				const Vec3 onTriangle = TriangleDetail::closestPointOnTriangle(crossing, triangle, crossingRegion);

				if ( (onTriangle - crossing).lengthSquared() <= TouchThreshold * TouchThreshold )
				{
					best.onSegment = crossing;
					best.onTriangle = crossing;
					best.distanceSquared = 0;
					best.feature = static_cast< uint32_t >(crossingRegion);
					best.pierces = true;
					best.isFace = crossingRegion == Region::Face;

					return best;
				}
			}

			/* 2. Both ends against the triangle, the segment against the 3 edges. */
			best.distanceSquared = std::numeric_limits< precision_t >::max();

			for ( const auto & endPoint : {start, end} )
			{
				Region region{Region::Face};
				const Vec3 onTriangle = TriangleDetail::closestPointOnTriangle(endPoint, triangle, region);
				const precision_t distanceSquared = (onTriangle - endPoint).lengthSquared();

				if ( distanceSquared < best.distanceSquared )
				{
					best.distanceSquared = distanceSquared;
					best.onSegment = endPoint;
					best.onTriangle = onTriangle;
					best.feature = static_cast< uint32_t >(region);
					best.isFace = region == Region::Face;
				}
			}

			if ( halfLength > 0 )
			{
				/* The edges in winding order: AB (index 0), BC (1), CA (2). */
				const std::array< std::pair< Vec3, Vec3 >, 3 > edges{{
					{triangle.pointA(), triangle.pointB()},
					{triangle.pointB(), triangle.pointC()},
					{triangle.pointC(), triangle.pointA()}
				}};
				uint32_t edgeIndex = 0;

				for ( const auto & [edgeStart, edgeEnd] : edges )
				{
					Vec3 edgeDirection = edgeEnd - edgeStart;
					const precision_t edgeLength = edgeDirection.length();

					edgeDirection *= static_cast< precision_t >(1) / edgeLength;

					Vec3 onSegment;
					Vec3 onEdge;

					BoxBoxDetail::closestPointsOfSegments(segmentCenter, direction, halfLength, (edgeStart + edgeEnd) * Half, edgeDirection, edgeLength * Half, onSegment, onEdge);

					const precision_t distanceSquared = (onEdge - onSegment).lengthSquared();

					if ( distanceSquared < best.distanceSquared )
					{
						best.distanceSquared = distanceSquared;
						best.onSegment = onSegment;
						best.onTriangle = onEdge;
						best.feature = 0x10U | edgeIndex;
						best.isFace = false;
					}

					++edgeIndex;
				}
			}

			return best;
		}

		/**
		 * @brief Clips a segment (centre + t·direction, t in [low, high]) to the prism of a triangle (its 3 edge planes,
		 * perpendicular to the triangle).
		 * @return bool False when nothing is left.
		 */
		template< typename precision_t >
		[[nodiscard]]
		bool
		clipSegmentToPrism (const Vector< 3, precision_t > & segmentCenter, const Vector< 3, precision_t > & direction, const Triangle< precision_t > & triangle, const Vector< 3, precision_t > & faceNormal, precision_t & low, precision_t & high) noexcept
		{
			using Vec3 = Vector< 3, precision_t >;

			constexpr auto SlopeThreshold = static_cast< precision_t >(1.0e-9);

			/* The edges in winding order: AB, BC, CA. */
			const std::array< std::pair< Vec3, Vec3 >, 3 > edges{{
				{triangle.pointA(), triangle.pointB()},
				{triangle.pointB(), triangle.pointC()},
				{triangle.pointC(), triangle.pointA()}
			}};

			for ( const auto & [start, end] : edges )
			{
				/* The winding normal makes cross(n, edge) point INTO the triangle. */
				const Vec3 inward = Vec3::crossProduct(faceNormal, end - start);
				const precision_t offset = Vec3::dotProduct(inward, segmentCenter - start);
				const precision_t slope = Vec3::dotProduct(inward, direction);

				if ( std::abs(slope) <= SlopeThreshold )
				{
					if ( offset < 0 )
					{
						return false;
					}

					continue;
				}

				/* inside: offset + slope t >= 0 */
				const precision_t crossing = -offset / slope;

				if ( slope > 0 )
				{
					low = std::max(low, crossing);
				}
				else
				{
					high = std::min(high, crossing);
				}
			}

			return low <= high;
		}
	}

	/**
	 * @brief Generates the contact manifold of a capsule (A) and a triangle (B): one or two points.
	 * @note The normal points FROM the capsule TO the triangle. A degenerate (collinear) triangle gives no contact.
	 * @note Feature ids: 0x1000 | end for a segment lying along the face; 0x2000 | region (`TriangleDetail::Region`) for
	 * one shallow point (0x2010 | edge index for a segment ↔ edge pair); 0x4000 | end for a deep contact.
	 * @param capsule A reference to the capsule (A). @pre capsule.isValid().
	 * @param triangle A reference to the triangle (B).
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const Capsule< precision_t > & capsule, const Triangle< precision_t > & triangle, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		using Vec3 = Vector< 3, precision_t >;
		using namespace TriangleDetail;
		using namespace CapsuleTriangleDetail;

		constexpr auto Half = static_cast< precision_t >(0.5);
		constexpr auto TouchThreshold = static_cast< precision_t >(1.0e-6);
		constexpr auto ParallelSine = static_cast< precision_t >(0.05);
		/* The contact normal counts as the face normal under ~2.6° (a tie between the face and an edge at the same
		 * distance must not hide the two-point face contact). */
		constexpr auto FaceAlignment = static_cast< precision_t >(0.999);

		manifold.clear();

		Vec3 faceNormal;

		if ( !unitNormal(triangle, faceNormal) )
		{
			return false;
		}

		const precision_t radius = capsule.radius();
		const Vec3 & start = capsule.startPoint();
		const Vec3 & end = capsule.endPoint();
		const Vec3 segmentCenter = (start + end) * Half;
		Vec3 direction = end - start;
		const precision_t length = direction.length();
		precision_t halfLength = 0;

		if ( length > TouchThreshold )
		{
			direction *= static_cast< precision_t >(1) / length;
			halfLength = length * Half;
		}
		else
		{
			direction = faceNormal;
		}

		/* 1. The exact closest points of the segment and the triangle (distance 0 when it pierces). */
		const auto closest = closestOfSegmentAndTriangle(start, end, triangle, faceNormal);
		const bool pierces = closest.pierces;
		const precision_t bestDistanceSquared = closest.distanceSquared;
		const Vec3 & bestOnSegment = closest.onSegment;
		const Vec3 & bestOnTriangle = closest.onTriangle;
		const uint32_t bestFeature = closest.feature;
		const bool bestIsFace = closest.isFace;

		if ( !pierces && bestDistanceSquared > radius * radius )
		{
			return false;
		}

		if ( !pierces && bestDistanceSquared > TouchThreshold * TouchThreshold )
		{
			/* 2. Shallow. */
			const precision_t distance = std::sqrt(bestDistanceSquared);
			const Vec3 normal = (bestOnTriangle - bestOnSegment) * (static_cast< precision_t >(1) / distance);

			/* Along the face: two points, the segment clipped to the prism. */
			if ( (bestIsFace || std::abs(Vec3::dotProduct(normal, faceNormal)) > FaceAlignment) && halfLength > 0 && std::abs(Vec3::dotProduct(direction, faceNormal)) < ParallelSine )
			{
				const Vec3 sideNormal = Vec3::dotProduct(bestOnSegment - bestOnTriangle, faceNormal) >= 0 ? faceNormal : -faceNormal;
				precision_t low = -halfLength;
				precision_t high = halfLength;

				if ( clipSegmentToPrism(segmentCenter, direction, triangle, faceNormal, low, high) && high - low > TouchThreshold )
				{
					manifold.setNormal(-sideNormal);

					uint32_t endIndex = 0;

					for ( const precision_t parameter : {low, high} )
					{
						const Vec3 point = segmentCenter + (direction * parameter);
						const precision_t height = Vec3::dotProduct(point - triangle.pointA(), sideNormal);

						if ( height < radius )
						{
							manifold.addPoint({((point - (sideNormal * height)) + (point - (sideNormal * radius))) * Half, radius - height, 0x1000U | endIndex});
						}

						++endIndex;
					}

					if ( !manifold.empty() )
					{
						return true;
					}
				}
			}

			manifold.setNormal(normal);
			manifold.addPoint({((bestOnSegment + (normal * radius)) + bestOnTriangle) * Half, radius - distance, 0x2000U | bestFeature});

			return true;
		}

		/* 3. Deep: the segment touches or pierces the triangle. Push towards the side of the capsule's centre. */
		const Vec3 sideNormal = Vec3::dotProduct(segmentCenter - triangle.pointA(), faceNormal) >= 0 ? faceNormal : -faceNormal;
		precision_t low = -halfLength;
		precision_t high = halfLength;

		if ( !clipSegmentToPrism(segmentCenter, direction, triangle, faceNormal, low, high) )
		{
			/* Touching an edge from outside the prism: the touching point of the segment carries the contact. */
			low = std::clamp(Vec3::dotProduct(bestOnSegment - segmentCenter, direction), -halfLength, halfLength);
			high = low;
		}

		manifold.setNormal(-sideNormal);

		uint32_t endIndex = 0;

		for ( const precision_t parameter : {low, high} )
		{
			if ( endIndex == 1 && high - low <= TouchThreshold )
			{
				break;
			}

			const Vec3 point = segmentCenter + (direction * parameter);
			const precision_t height = Vec3::dotProduct(point - triangle.pointA(), sideNormal);

			if ( height < radius )
			{
				manifold.addPoint({((point - (sideNormal * height)) + (point - (sideNormal * radius))) * Half, radius - height, 0x4000U | endIndex});
			}

			++endIndex;
		}

		if ( manifold.empty() )
		{
			manifold.addPoint({bestOnTriangle, radius, 0x4000U});
		}

		return true;
	}

	/**
	 * @brief Generates the contact manifold of a triangle (A) and a capsule (B): one or two points.
	 * @note The normal points FROM the triangle TO the capsule; otherwise identical to the capsule ↔ triangle overload.
	 * @param triangle A reference to the triangle (A).
	 * @param capsule A reference to the capsule (B). @pre capsule.isValid().
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const Triangle< precision_t > & triangle, const Capsule< precision_t > & capsule, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		if ( !computeContactManifold(capsule, triangle, manifold) )
		{
			return false;
		}

		manifold.flip();

		return true;
	}
}
