/*
 * src/Math/Space3D/Contacts/CapsuleBox.hpp
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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

/* Local inclusions for usages. */
#include "Math/Space3D/Capsule.hpp"
#include "Math/Space3D/OrientedBox.hpp"
#include "Math/Vector.hpp"
#include "StaticVector.hpp"
#include "BoxBox.hpp"
#include "ContactManifold.hpp"

/*
 * Capsule ↔ box contact generation.
 *  - The capsule's segment does not touch the box: the EXACT closest points of the segment and the box (the squared
 *    distance along the segment is a convex piecewise quadratic with at most 6 breakpoints, minimised in closed form on
 *    each piece). One point, or two when the segment lies along the box face it is closest to (a capsule lying on a
 *    floor must not rock on one point).
 *  - The segment touches or crosses the box (a deep contact): the separating-axis test on the 3 face axes and the 3
 *    cross products of the segment with the box axes, biased towards faces; a face axis clips the segment to the face
 *    (two points), an edge axis gives the point between the segment and that box edge.
 * References: C. Ericson, "Real-Time Collision Detection" (2005), § 5.1.4, § 5.1.8, § 4.4; D. Gregorius, GDC 2013 / 2015
 * (the face bias and the clipping). No third-party code.
 */

namespace EmEn::Base::Math::Space3D
{
	namespace CapsuleBoxDetail
	{
		/** @brief The closest points of a segment and a box. */
		template< typename precision_t >
		struct SegmentBoxClosest final
		{
			/** The parameter along the segment, in [-halfLength, halfLength] around its centre. */
			precision_t parameter{0};
			/** The squared distance (0 when the segment touches the box). */
			precision_t distanceSquared{0};
		};

		/**
		 * @brief The exact closest point of a segment (centre, unit direction, half length) to a box.
		 * @note In the box frame, the coordinate along axis i is linear in t, so the squared distance is
		 * Σ max(|α_i + β_i t| - e_i, 0)²: a convex piecewise quadratic whose pieces end where |α_i + β_i t| = e_i.
		 */
		template< typename precision_t >
		[[nodiscard]]
		SegmentBoxClosest< precision_t >
		closestOfSegmentAndBox (const Vector< 3, precision_t > & segmentCenter, const Vector< 3, precision_t > & direction, precision_t halfLength, const OrientedBox< precision_t > & box) noexcept
		{
			using Vec3 = Vector< 3, precision_t >;

			constexpr auto CapsuleBoxSlope = static_cast< precision_t >(1.0e-9);

			const Vec3 offset = segmentCenter - box.center();
			/* The box-frame coordinate along axis i is alpha[i] + beta[i] t (Vector used as a plain triple). */
			Vec3 alpha;
			Vec3 beta;
			Vec3 extent;

			/* Every piece boundary: the 2 ends and up to 2 crossings per axis. */
			StaticVector< precision_t, 8 > breaks;
			breaks.push_back(-halfLength);
			breaks.push_back(halfLength);

			for ( size_t index = 0; index < 3; ++index )
			{
				alpha[index] = Vec3::dotProduct(offset, box.axis(index));
				beta[index] = Vec3::dotProduct(direction, box.axis(index));
				extent[index] = box.halfExtent(index);

				if ( std::abs(beta[index]) > CapsuleBoxSlope )
				{
					for ( const precision_t bound : {-extent[index], extent[index]} )
					{
						const precision_t crossing = (bound - alpha[index]) / beta[index];

						if ( crossing > -halfLength && crossing < halfLength && !breaks.full() )
						{
							breaks.push_back(crossing);
						}
					}
				}
			}

			std::sort(breaks.begin(), breaks.end());

			const auto squaredDistanceAt = [&] (precision_t parameter) {
				precision_t sum = 0;

				for ( size_t index = 0; index < 3; ++index )
				{
					const precision_t excess = std::abs(alpha[index] + (beta[index] * parameter)) - extent[index];

					sum += excess > 0 ? excess * excess : static_cast< precision_t >(0);
				}

				return sum;
			};

			SegmentBoxClosest< precision_t > best{-halfLength, squaredDistanceAt(-halfLength)};

			for ( size_t piece = 0; piece + 1 < breaks.size(); ++piece )
			{
				const precision_t low = breaks[piece];
				const precision_t high = breaks[piece + 1];
				const precision_t middle = (low + high) * static_cast< precision_t >(0.5);

				/* On this piece every axis is either inside its slab or beyond one bound: f(t) = a t² + b t + c. */
				precision_t quadratic = 0;
				precision_t linear = 0;

				for ( size_t index = 0; index < 3; ++index )
				{
					const precision_t coordinate = alpha[index] + (beta[index] * middle);

					if ( coordinate > extent[index] || coordinate < -extent[index] )
					{
						const precision_t bound = coordinate > 0 ? extent[index] : -extent[index];
						const precision_t constant = alpha[index] - bound;

						quadratic += beta[index] * beta[index];
						linear += static_cast< precision_t >(2) * beta[index] * constant;
					}
				}

				precision_t candidate = high;

				if ( quadratic > CapsuleBoxSlope )
				{
					candidate = std::clamp(-linear / (static_cast< precision_t >(2) * quadratic), low, high);
				}

				for ( const precision_t parameter : {candidate, high} )
				{
					const precision_t distanceSquared = squaredDistanceAt(parameter);

					if ( distanceSquared < best.distanceSquared )
					{
						best = {parameter, distanceSquared};
					}
				}
			}

			return best;
		}

		/**
		 * @brief Clips a segment (centre + t·direction, t in [low, high]) to the side planes of one box face.
		 * @return bool False when nothing is left.
		 */
		template< typename precision_t >
		[[nodiscard]]
		bool
		clipSegmentToFace (const Vector< 3, precision_t > & segmentCenter, const Vector< 3, precision_t > & direction, const OrientedBox< precision_t > & box, size_t faceAxis, precision_t & low, precision_t & high) noexcept
		{
			using Vec3 = Vector< 3, precision_t >;

			constexpr auto CapsuleBoxSlope = static_cast< precision_t >(1.0e-9);

			for ( size_t sideOffset = 1; sideOffset <= 2; ++sideOffset )
			{
				const size_t sideAxis = (faceAxis + sideOffset) % 3;
				const precision_t coordinate = Vec3::dotProduct(segmentCenter - box.center(), box.axis(sideAxis));
				const precision_t slope = Vec3::dotProduct(direction, box.axis(sideAxis));
				const precision_t extent = box.halfExtent(sideAxis);

				if ( std::abs(slope) <= CapsuleBoxSlope )
				{
					if ( std::abs(coordinate) > extent )
					{
						return false;
					}

					continue;
				}

				precision_t enter = (-extent - coordinate) / slope;
				precision_t leave = (extent - coordinate) / slope;

				if ( enter > leave )
				{
					std::swap(enter, leave);
				}

				low = std::max(low, enter);
				high = std::min(high, leave);
			}

			return low <= high;
		}
	}

	/**
	 * @brief Generates the contact manifold of a capsule (A) and an oriented box (B): one or two points.
	 * @note The normal points FROM the capsule TO the box. Points lie halfway between the two surfaces.
	 * @note Feature ids: 0x1000 | face << 4 | end (0 or 1) for a segment lying along a face; 0x2000 | the box region (base
	 * 3, as the sphere ↔ box one) for a single shallow point; 0x4000 | face << 4 | end for a deep face contact; 0x8000 |
	 * box axis for a deep edge contact.
	 * @param capsule A reference to the capsule (A). @pre capsule.isValid().
	 * @param box A reference to the box (B). @pre box.isValid().
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const Capsule< precision_t > & capsule, const OrientedBox< precision_t > & box, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		using Vec3 = Vector< 3, precision_t >;
		using namespace CapsuleBoxDetail;

		constexpr auto CapsuleBoxHalf = static_cast< precision_t >(0.5);
		/* Under this distance the segment touches the box: the shallow normal is undefined, the deep path takes over. */
		constexpr auto CapsuleBoxTouch = static_cast< precision_t >(1.0e-6);
		/* A segment counts as lying along a face when its direction is within ~3° of the face plane. */
		constexpr auto CapsuleBoxParallelSine = static_cast< precision_t >(0.05);
		constexpr auto CapsuleBoxFaceBias = static_cast< precision_t >(0.95);
		constexpr auto CapsuleBoxAbsoluteBias = static_cast< precision_t >(1.0e-4);
		constexpr auto CapsuleBoxParallelEdge = static_cast< precision_t >(1.0e-6);

		manifold.clear();

		const precision_t radius = capsule.radius();
		const Vec3 segmentCenter = (capsule.startPoint() + capsule.endPoint()) * CapsuleBoxHalf;
		Vec3 direction = capsule.endPoint() - capsule.startPoint();
		const precision_t length = direction.length();
		precision_t halfLength = 0;

		if ( length > CapsuleBoxTouch )
		{
			direction *= static_cast< precision_t >(1) / length;
			halfLength = length * CapsuleBoxHalf;
		}
		else
		{
			/* A degenerate capsule is a sphere: any direction does. */
			direction = Vec3{0, 1, 0};
		}

		const auto closest = closestOfSegmentAndBox(segmentCenter, direction, halfLength, box);

		if ( closest.distanceSquared > radius * radius )
		{
			return false;
		}

		if ( closest.distanceSquared > CapsuleBoxTouch * CapsuleBoxTouch )
		{
			/* 1. Shallow: the segment is outside the box, less than a radius away. */
			const Vec3 segmentPoint = segmentCenter + (direction * closest.parameter);
			const Vec3 offset = segmentPoint - box.center();
			Vec3 boxPoint = box.center();
			uint32_t region = 0;
			size_t clampedAxes = 0;
			size_t faceAxis = 0;

			for ( size_t index = 3; index-- > 0; )
			{
				const precision_t local = Vec3::dotProduct(offset, box.axis(index));
				const precision_t extent = box.halfExtent(index);

				region = (region * 3U) + ContactsDetail::regionDigit(local, extent);

				if ( local < -extent || local > extent )
				{
					++clampedAxes;
					faceAxis = index;
				}

				boxPoint += box.axis(index) * std::clamp(local, -extent, extent);
			}

			const precision_t distance = std::sqrt(closest.distanceSquared);
			const Vec3 normal = (boxPoint - segmentPoint) * (static_cast< precision_t >(1) / distance);

			manifold.setNormal(normal);

			/* The closest box feature is a face and the segment lies along it: two points, the segment clipped to it. */
			if ( clampedAxes == 1 && halfLength > 0 && std::abs(Vec3::dotProduct(direction, box.axis(faceAxis))) < CapsuleBoxParallelSine )
			{
				const precision_t faceSide = Vec3::dotProduct(offset, box.axis(faceAxis)) > 0 ? static_cast< precision_t >(1) : static_cast< precision_t >(-1);
				const Vec3 faceNormal = box.axis(faceAxis) * faceSide;
				const precision_t facePlane = Vec3::dotProduct(faceNormal, box.center()) + box.halfExtent(faceAxis);
				const auto faceId = static_cast< uint32_t >((faceAxis * 2U) + (faceSide > 0 ? 0U : 1U));

				precision_t low = -halfLength;
				precision_t high = halfLength;

				if ( clipSegmentToFace(segmentCenter, direction, box, faceAxis, low, high) && high - low > CapsuleBoxTouch )
				{
					manifold.setNormal(-faceNormal);

					uint32_t end = 0;

					for ( const precision_t parameter : {low, high} )
					{
						const Vec3 point = segmentCenter + (direction * parameter);
						const precision_t height = Vec3::dotProduct(faceNormal, point) - facePlane;

						if ( height < radius )
						{
							const Vec3 onFace = point - (faceNormal * height);
							const Vec3 onCapsule = point - (faceNormal * radius);

							manifold.addPoint({(onFace + onCapsule) * CapsuleBoxHalf, radius - height, 0x1000U | (faceId << 4U) | end});
						}

						++end;
					}

					if ( !manifold.empty() )
					{
						return true;
					}

					manifold.setNormal(normal);
				}
			}

			manifold.addPoint({((segmentPoint + (normal * radius)) + boxPoint) * CapsuleBoxHalf, radius - distance, 0x2000U | region});

			return true;
		}

		/* 2. Deep: the segment touches or crosses the box. Separating axes, biased towards the box faces. */
		const Vec3 offset = segmentCenter - box.center();

		precision_t faceSeparation = -std::numeric_limits< precision_t >::max();
		size_t faceAxis = 0;

		for ( size_t index = 0; index < 3; ++index )
		{
			const auto & axis = box.axis(index);
			const precision_t separation = std::abs(Vec3::dotProduct(offset, axis)) - (box.halfExtent(index) + (halfLength * std::abs(Vec3::dotProduct(direction, axis)))) - radius;

			if ( separation > faceSeparation )
			{
				faceSeparation = separation;
				faceAxis = index;
			}
		}

		precision_t edgeSeparation = -std::numeric_limits< precision_t >::max();
		size_t edgeAxisIndex = 0;
		Vec3 edgeAxis;

		for ( size_t index = 0; index < 3; ++index )
		{
			auto axis = Vec3::crossProduct(direction, box.axis(index));
			const precision_t lengthSquared = axis.lengthSquared();

			if ( halfLength <= 0 || lengthSquared < CapsuleBoxParallelEdge )
			{
				continue;
			}

			axis *= static_cast< precision_t >(1) / std::sqrt(lengthSquared);

			/* The segment projects to a point on an axis perpendicular to it. */
			const precision_t separation = std::abs(Vec3::dotProduct(offset, axis)) - box.projectedRadius(axis) - radius;

			if ( separation > edgeSeparation )
			{
				edgeSeparation = separation;
				edgeAxisIndex = index;
				edgeAxis = axis;
			}
		}

		if ( edgeSeparation > (CapsuleBoxFaceBias * faceSeparation) + CapsuleBoxAbsoluteBias )
		{
			/* From the capsule to the box. */
			if ( Vec3::dotProduct(edgeAxis, offset) > 0 )
			{
				edgeAxis = -edgeAxis;
			}

			/* The box edge parallel to that axis, on the side facing the capsule. */
			Vec3 edgeCenter = box.center();

			for ( size_t index = 0; index < 3; ++index )
			{
				if ( index != edgeAxisIndex )
				{
					const precision_t side = Vec3::dotProduct(box.axis(index), edgeAxis) >= 0 ? static_cast< precision_t >(-1) : static_cast< precision_t >(1);

					edgeCenter += box.axis(index) * (side * box.halfExtent(index));
				}
			}

			Vec3 onSegment;
			Vec3 onEdge;

			BoxBoxDetail::closestPointsOfSegments(segmentCenter, direction, halfLength, edgeCenter, box.axis(edgeAxisIndex), box.halfExtent(edgeAxisIndex), onSegment, onEdge);

			manifold.setNormal(edgeAxis);
			manifold.addPoint({((onSegment + (edgeAxis * radius)) + onEdge) * CapsuleBoxHalf, -edgeSeparation, 0x8000U | static_cast< uint32_t >(edgeAxisIndex)});

			return true;
		}

		/* The face of least penetration, facing the capsule. */
		const precision_t faceSide = Vec3::dotProduct(offset, box.axis(faceAxis)) >= 0 ? static_cast< precision_t >(1) : static_cast< precision_t >(-1);
		const Vec3 faceNormal = box.axis(faceAxis) * faceSide;
		const precision_t facePlane = Vec3::dotProduct(faceNormal, box.center()) + box.halfExtent(faceAxis);
		const auto faceId = static_cast< uint32_t >((faceAxis * 2U) + (faceSide > 0 ? 0U : 1U));

		precision_t low = -halfLength;
		precision_t high = halfLength;

		if ( !clipSegmentToFace(segmentCenter, direction, box, faceAxis, low, high) )
		{
			/* The segment crosses the box but not through this face's prism: fall back on the closest point. */
			low = std::clamp(closest.parameter, -halfLength, halfLength);
			high = low;
		}

		manifold.setNormal(-faceNormal);

		uint32_t end = 0;

		for ( const precision_t parameter : {low, high} )
		{
			if ( end == 1 && high - low <= CapsuleBoxTouch )
			{
				break;
			}

			const Vec3 point = segmentCenter + (direction * parameter);
			const precision_t height = Vec3::dotProduct(faceNormal, point) - facePlane;

			if ( height < radius )
			{
				const Vec3 onFace = point - (faceNormal * height);
				const Vec3 onCapsule = point - (faceNormal * radius);

				manifold.addPoint({(onFace + onCapsule) * CapsuleBoxHalf, radius - height, 0x4000U | (faceId << 4U) | end});
			}

			++end;
		}

		if ( manifold.empty() )
		{
			/* Numerically possible on a grazing contact: report the SAT depth on the face centre line. */
			const Vec3 point = segmentCenter + (direction * low);

			manifold.addPoint({point - (faceNormal * radius), -faceSeparation, 0x4000U | (faceId << 4U)});
		}

		return true;
	}

	/**
	 * @brief Generates the contact manifold of an oriented box (A) and a capsule (B): one or two points.
	 * @note The normal points FROM the box TO the capsule; otherwise identical to the capsule ↔ box overload.
	 * @param box A reference to the box (A). @pre box.isValid().
	 * @param capsule A reference to the capsule (B). @pre capsule.isValid().
	 * @param manifold A reference to the manifold, cleared first and filled when they overlap.
	 * @return bool True when they overlap (or touch).
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const OrientedBox< precision_t > & box, const Capsule< precision_t > & capsule, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		if ( !computeContactManifold(capsule, box, manifold) )
		{
			return false;
		}

		manifold.flip();

		return true;
	}
}
