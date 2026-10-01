/*
 * src/Math/Space3D/Contacts/BoxBox.hpp
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

/* Local inclusions for usages. */
#include "Math/Space3D/OrientedBox.hpp"
#include "Math/Vector.hpp"
#include "StaticVector.hpp"
#include "ContactManifold.hpp"

/*
 * Box ↔ box contact generation: the separating-axis test on the 15 candidate axes, then, for a face axis, the
 * incident face of the other box clipped against the side planes of the reference face (Sutherland-Hodgman) and reduced
 * to 4 points; for an edge axis, one point between the two closest edges.
 *
 * References:
 *  - D. Gregorius, "The Separating Axis Test between Convex Polyhedra" (GDC 2013) and "Robust Contact Creation for
 *    Physics Simulations" (GDC 2015): the axis choice biased towards faces, the clipping, the 4-point reduction.
 *  - C. Ericson, "Real-Time Collision Detection" (2005), § 4.4.1 (OBB-OBB SAT), § 5.1.8 (closest points of two
 *    segments).
 * No third-party code is copied here.
 */

namespace EmEn::Base::Math::Space3D
{
	namespace BoxBoxDetail
	{
		/** @brief A vertex of the clipped incident polygon, with the features it comes from. */
		template< typename precision_t >
		struct ClipVertex final
		{
			Vector< 3, precision_t > position;
			/** The identity of the point: an incident vertex (0-3), or 0x100 | (clip plane << 4) | edge tag. */
			uint16_t key{0};
			/** The edge leaving this vertex: an incident face edge (0-3), or 4 + the clip plane it runs along. */
			uint8_t outgoingEdge{0};
		};

		/** A convex quad clipped by 4 planes gains at most one vertex per plane: 4 + 4 = 8. */
		template< typename precision_t >
		using ClipPolygon = StaticVector< ClipVertex< precision_t >, 8 >;

		/** @brief +1 for a non-negative value, -1 otherwise (a zero picks a side deterministically). */
		template< typename precision_t >
		[[nodiscard]]
		constexpr
		precision_t
		signOf (precision_t value) noexcept
		{
			return value >= 0 ? static_cast< precision_t >(1) : static_cast< precision_t >(-1);
		}

		/**
		 * @brief Keeps the part of a polygon where dot(planeNormal, p) <= planeOffset.
		 * @param input The polygon to clip.
		 * @param planeNormal The outward normal of the clip plane.
		 * @param planeOffset The plane offset.
		 * @param planeIndex The index of the clip plane (0-3), recorded in the keys of the new vertices.
		 * @param output The clipped polygon (cleared first).
		 */
		template< typename precision_t >
		void
		clipByPlane (const ClipPolygon< precision_t > & input, const Vector< 3, precision_t > & planeNormal, precision_t planeOffset, uint8_t planeIndex, ClipPolygon< precision_t > & output) noexcept
		{
			output.clear();

			if ( input.empty() )
			{
				return;
			}

			for ( size_t index = 0; index < input.size(); ++index )
			{
				const auto & current = input[index];
				const auto & next = input[(index + 1) % input.size()];

				const precision_t currentDistance = Vector< 3, precision_t >::dotProduct(planeNormal, current.position) - planeOffset;
				const precision_t nextDistance = Vector< 3, precision_t >::dotProduct(planeNormal, next.position) - planeOffset;

				const bool currentInside = currentDistance <= 0;
				const bool nextInside = nextDistance <= 0;

				/* NOTE: every push is proven in capacity — a convex polygon crossing a plane gains at most one vertex,
				 * and the input never exceeds 4 + the planes already applied (≤ 7 here). The guard stays for safety. */
				if ( currentInside && !output.full() )
				{
					output.push_back(current);
				}

				if ( currentInside != nextInside && !output.full() )
				{
					/* The two distances have opposite signs here, so their difference cannot be zero. */
					const precision_t ratio = currentDistance / (currentDistance - nextDistance);

					ClipVertex< precision_t > crossing;
					crossing.position = current.position + ((next.position - current.position) * ratio);
					crossing.key = static_cast< uint16_t >(0x100U | (static_cast< uint32_t >(planeIndex) << 4U) | current.outgoingEdge);
					/* Leaving the inside: the polygon then runs along the clip plane. Entering: along the same edge. */
					crossing.outgoingEdge = currentInside ? static_cast< uint8_t >(4U + planeIndex) : current.outgoingEdge;

					output.push_back(crossing);
				}
			}
		}

		/** @brief A candidate contact point before the reduction. */
		template< typename precision_t >
		struct Candidate final
		{
			Vector< 3, precision_t > position;
			precision_t depth{0};
			uint32_t featureId{0};
		};

		/**
		 * @brief Picks at most 4 candidates that span the contact best (Gregorius, GDC 2015): the deepest, the farthest
		 * from it, then the two that make the largest triangles on either side of that segment.
		 * @note Ties break on the feature id, so the choice does not depend on the clipping order.
		 */
		template< typename precision_t >
		void
		reduceToFour (const StaticVector< Candidate< precision_t >, 8 > & candidates, const Vector< 3, precision_t > & normal, ContactManifold< precision_t > & manifold) noexcept
		{
			if ( candidates.size() <= ContactManifold< precision_t >::MaxPoints )
			{
				for ( const auto & candidate : candidates )
				{
					manifold.addPoint({candidate.position, candidate.depth, candidate.featureId});
				}

				return;
			}

			const auto better = [] (precision_t score, uint32_t id, precision_t bestScore, uint32_t bestId) {
				return score > bestScore || (score == bestScore && id < bestId);
			};

			/* 1. The deepest point. */
			size_t first = 0;

			for ( size_t index = 1; index < candidates.size(); ++index )
			{
				if ( better(candidates[index].depth, candidates[index].featureId, candidates[first].depth, candidates[first].featureId) )
				{
					first = index;
				}
			}

			/* 2. The farthest from it. */
			size_t second = first == 0 ? 1 : 0;
			precision_t secondScore = -1;

			for ( size_t index = 0; index < candidates.size(); ++index )
			{
				if ( index == first )
				{
					continue;
				}

				const precision_t score = (candidates[index].position - candidates[first].position).lengthSquared();

				if ( better(score, candidates[index].featureId, secondScore, candidates[second].featureId) )
				{
					second = index;
					secondScore = score;
				}
			}

			/* 3. and 4. The largest signed triangle areas on each side of the first segment. */
			const auto segment = candidates[second].position - candidates[first].position;

			size_t third = candidates.size();
			size_t fourth = candidates.size();
			precision_t thirdScore = 0;
			precision_t fourthScore = 0;

			for ( size_t index = 0; index < candidates.size(); ++index )
			{
				if ( index == first || index == second )
				{
					continue;
				}

				const auto toPoint = candidates[index].position - candidates[first].position;
				const precision_t area = Vector< 3, precision_t >::dotProduct(Vector< 3, precision_t >::crossProduct(segment, toPoint), normal);
				const uint32_t id = candidates[index].featureId;

				if ( area > 0 && (third == candidates.size() || better(area, id, thirdScore, candidates[third].featureId)) )
				{
					third = index;
					thirdScore = area;
				}

				if ( area < 0 && (fourth == candidates.size() || better(-area, id, fourthScore, candidates[fourth].featureId)) )
				{
					fourth = index;
					fourthScore = -area;
				}
			}

			for ( const auto index : {first, second, third, fourth} )
			{
				if ( index < candidates.size() )
				{
					const auto & candidate = candidates[index];

					manifold.addPoint({candidate.position, candidate.depth, candidate.featureId});
				}
			}
		}

		/**
		 * @brief The closest points of two segments given by their centre, unit direction and half length.
		 * @note Ericson § 5.1.8, written on centred segments; a parallel pair takes the middle of the overlap.
		 */
		template< typename precision_t >
		void
		closestPointsOfSegments (const Vector< 3, precision_t > & centerA, const Vector< 3, precision_t > & directionA, precision_t halfLengthA, const Vector< 3, precision_t > & centerB, const Vector< 3, precision_t > & directionB, precision_t halfLengthB, Vector< 3, precision_t > & pointA, Vector< 3, precision_t > & pointB) noexcept
		{
			const auto offset = centerB - centerA;
			const precision_t cosine = Vector< 3, precision_t >::dotProduct(directionA, directionB);
			const precision_t alongA = Vector< 3, precision_t >::dotProduct(directionA, offset);
			const precision_t alongB = Vector< 3, precision_t >::dotProduct(directionB, offset);
			const precision_t denominator = static_cast< precision_t >(1) - (cosine * cosine);

			precision_t parameterA = 0;

			if ( denominator > std::numeric_limits< precision_t >::epsilon() )
			{
				parameterA = (alongA - cosine * alongB) / denominator;
			}

			parameterA = std::clamp(parameterA, -halfLengthA, halfLengthA);

			precision_t parameterB = std::clamp((parameterA * cosine) - alongB, -halfLengthB, halfLengthB);

			parameterA = std::clamp((parameterB * cosine) + alongA, -halfLengthA, halfLengthA);

			pointA = centerA + (directionA * parameterA);
			pointB = centerB + (directionB * parameterB);
		}
	}

	/**
	 * @brief Generates the contact manifold of two oriented boxes.
	 * @note The normal points FROM A TO B. Each point lies halfway between the two surfaces and carries its own depth.
	 * @note Feature ids: bit 31 set for an edge ↔ edge contact (edge of A << 8 | edge of B | the 4 sign bits << 16);
	 * otherwise bit 30 tells whether B holds the reference face, bits 24-27 the reference face, 16-19 the incident
	 * face and 0-15 the clipped point (see BoxBoxDetail::ClipVertex::key).
	 * @param boxA A reference to box A. @pre boxA.isValid().
	 * @param boxB A reference to box B. @pre boxB.isValid().
	 * @param manifold A reference to the manifold, cleared first and filled when the boxes overlap.
	 * @return bool True when the boxes overlap (or touch); the manifold then holds at least one point.
	 */
	template< typename precision_t = float >
	[[nodiscard]]
	bool
	computeContactManifold (const OrientedBox< precision_t > & boxA, const OrientedBox< precision_t > & boxB, ContactManifold< precision_t > & manifold) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		using Vec3 = Vector< 3, precision_t >;
		using namespace BoxBoxDetail;

		/* Bias towards faces, then towards the faces of A (Gregorius, GDC 2013): an edge axis or a face of B wins only
		 * when it separates clearly more, so the choice does not flicker between nearly equal axes. Separations are
		 * negative when penetrating: scaling a negative value by < 1 moves it towards zero, which favours the face. */
		constexpr auto BoxBoxEdgeBias = static_cast< precision_t >(0.95);
		constexpr auto BoxBoxFaceBias = static_cast< precision_t >(0.98);
		constexpr auto BoxBoxAbsoluteBias = static_cast< precision_t >(1.0e-4);
		/* Under this squared length, two edges are parallel: their cross product is no axis. */
		constexpr auto BoxBoxParallelEdge = static_cast< precision_t >(1.0e-6);

		manifold.clear();

		const Vec3 offset = boxB.center() - boxA.center();

		/* 1. Face axes of A. */
		precision_t faceASeparation = -std::numeric_limits< precision_t >::max();
		size_t faceAIndex = 0;

		for ( size_t index = 0; index < 3; ++index )
		{
			const auto & axis = boxA.axis(index);
			const precision_t separation = std::abs(Vec3::dotProduct(offset, axis)) - (boxA.halfExtent(index) + boxB.projectedRadius(axis));

			if ( separation > 0 )
			{
				return false;
			}

			if ( separation > faceASeparation )
			{
				faceASeparation = separation;
				faceAIndex = index;
			}
		}

		/* 2. Face axes of B. */
		precision_t faceBSeparation = -std::numeric_limits< precision_t >::max();
		size_t faceBIndex = 0;

		for ( size_t index = 0; index < 3; ++index )
		{
			const auto & axis = boxB.axis(index);
			const precision_t separation = std::abs(Vec3::dotProduct(offset, axis)) - (boxB.halfExtent(index) + boxA.projectedRadius(axis));

			if ( separation > 0 )
			{
				return false;
			}

			if ( separation > faceBSeparation )
			{
				faceBSeparation = separation;
				faceBIndex = index;
			}
		}

		/* 3. Edge axes. */
		precision_t edgeSeparation = -std::numeric_limits< precision_t >::max();
		size_t edgeAIndex = 0;
		size_t edgeBIndex = 0;
		Vec3 edgeAxis;

		for ( size_t indexA = 0; indexA < 3; ++indexA )
		{
			for ( size_t indexB = 0; indexB < 3; ++indexB )
			{
				auto axis = Vec3::crossProduct(boxA.axis(indexA), boxB.axis(indexB));
				const precision_t lengthSquared = axis.lengthSquared();

				if ( lengthSquared < BoxBoxParallelEdge )
				{
					continue;
				}

				axis *= static_cast< precision_t >(1) / std::sqrt(lengthSquared);

				const precision_t separation = std::abs(Vec3::dotProduct(offset, axis)) - (boxA.projectedRadius(axis) + boxB.projectedRadius(axis));

				if ( separation > 0 )
				{
					return false;
				}

				if ( separation > edgeSeparation )
				{
					edgeSeparation = separation;
					edgeAIndex = indexA;
					edgeBIndex = indexB;
					edgeAxis = axis;
				}
			}
		}

		/* 4. Edge ↔ edge: one point between the two closest edges. */
		const precision_t bestFaceSeparation = std::max(faceASeparation, faceBSeparation);

		if ( edgeSeparation > (BoxBoxEdgeBias * bestFaceSeparation) + BoxBoxAbsoluteBias )
		{
			/* Orient the axis from A to B. */
			if ( Vec3::dotProduct(edgeAxis, offset) < 0 )
			{
				edgeAxis = -edgeAxis;
			}

			/* The edge of A parallel to its axis edgeAIndex, on the side facing B; the edge of B facing A. */
			Vec3 edgeCenterA = boxA.center();
			Vec3 edgeCenterB = boxB.center();
			uint32_t signBits = 0;

			for ( size_t index = 0; index < 3; ++index )
			{
				if ( index != edgeAIndex )
				{
					const precision_t side = signOf(Vec3::dotProduct(boxA.axis(index), edgeAxis));

					edgeCenterA += boxA.axis(index) * (side * boxA.halfExtent(index));
					signBits = (signBits << 1U) | (side > 0 ? 1U : 0U);
				}

				if ( index != edgeBIndex )
				{
					const precision_t side = signOf(Vec3::dotProduct(boxB.axis(index), edgeAxis));

					edgeCenterB -= boxB.axis(index) * (side * boxB.halfExtent(index));
					signBits = (signBits << 1U) | (side > 0 ? 1U : 0U);
				}
			}

			Vec3 pointA;
			Vec3 pointB;

			closestPointsOfSegments(edgeCenterA, boxA.axis(edgeAIndex), boxA.halfExtent(edgeAIndex), edgeCenterB, boxB.axis(edgeBIndex), boxB.halfExtent(edgeBIndex), pointA, pointB);

			const uint32_t featureId = 0x80000000U | (signBits << 16U) | (static_cast< uint32_t >(edgeAIndex) << 8U) | static_cast< uint32_t >(edgeBIndex);

			manifold.setNormal(edgeAxis);
			manifold.addPoint({(pointA + pointB) * static_cast< precision_t >(0.5), -edgeSeparation, featureId});

			return true;
		}

		/* 5. Face contact. The reference box owns the chosen face; the other one is the incident box. */
		const bool referenceIsB = faceBSeparation > (BoxBoxFaceBias * faceASeparation) + BoxBoxAbsoluteBias;
		const auto & reference = referenceIsB ? boxB : boxA;
		const auto & incident = referenceIsB ? boxA : boxB;
		const size_t referenceAxis = referenceIsB ? faceBIndex : faceAIndex;
		/* From the reference box towards the incident one. */
		const Vec3 towardsIncident = referenceIsB ? -offset : offset;
		const precision_t referenceSide = signOf(Vec3::dotProduct(towardsIncident, reference.axis(referenceAxis)));
		const Vec3 referenceNormal = reference.axis(referenceAxis) * referenceSide;
		const auto referenceFace = static_cast< uint32_t >((referenceAxis * 2U) + (referenceSide > 0 ? 0U : 1U));

		/* The incident face: the face of the incident box most anti-parallel to the reference normal. */
		size_t incidentAxis = 0;
		precision_t mostAntiParallel = 0;

		for ( size_t index = 0; index < 3; ++index )
		{
			const precision_t alignment = std::abs(Vec3::dotProduct(incident.axis(index), referenceNormal));

			if ( alignment > mostAntiParallel )
			{
				mostAntiParallel = alignment;
				incidentAxis = index;
			}
		}

		const precision_t incidentSide = -signOf(Vec3::dotProduct(incident.axis(incidentAxis), referenceNormal));
		const auto incidentFace = static_cast< uint32_t >((incidentAxis * 2U) + (incidentSide > 0 ? 0U : 1U));

		/* Its 4 corners, in a winding that walks around the face. */
		const size_t tangentU = (incidentAxis + 1) % 3;
		const size_t tangentV = (incidentAxis + 2) % 3;
		const Vec3 faceCenter = incident.center() + (incident.axis(incidentAxis) * (incidentSide * incident.halfExtent(incidentAxis)));
		const Vec3 stepU = incident.axis(tangentU) * incident.halfExtent(tangentU);
		const Vec3 stepV = incident.axis(tangentV) * incident.halfExtent(tangentV);

		ClipPolygon< precision_t > polygon;
		ClipPolygon< precision_t > clipped;

		const std::array< Vec3, 4 > corners{
			faceCenter + stepU + stepV,
			faceCenter - stepU + stepV,
			faceCenter - stepU - stepV,
			faceCenter + stepU - stepV
		};

		uint8_t cornerIndex = 0;

		for ( const auto & corner : corners )
		{
			polygon.push_back({corner, cornerIndex, cornerIndex});

			++cornerIndex;
		}

		/* Clip against the 4 side planes of the reference face. */
		uint8_t planeIndex = 0;

		for ( size_t sideAxisOffset = 1; sideAxisOffset <= 2; ++sideAxisOffset )
		{
			const size_t sideAxis = (referenceAxis + sideAxisOffset) % 3;
			const auto & direction = reference.axis(sideAxis);
			const precision_t centerProjection = Vec3::dotProduct(direction, reference.center());
			const precision_t halfExtent = reference.halfExtent(sideAxis);

			clipByPlane(polygon, direction, centerProjection + halfExtent, planeIndex++, clipped);
			clipByPlane(clipped, -direction, -centerProjection + halfExtent, planeIndex++, polygon);
		}

		/* Keep the points under the reference face; each one's depth is its distance below it. */
		const precision_t referencePlane = Vec3::dotProduct(referenceNormal, reference.center()) + reference.halfExtent(referenceAxis);
		const uint32_t faceBits = (referenceIsB ? 0x40000000U : 0U) | (referenceFace << 24U) | (incidentFace << 16U);

		StaticVector< Candidate< precision_t >, 8 > candidates;

		for ( const auto & vertex : polygon )
		{
			const precision_t separation = Vec3::dotProduct(referenceNormal, vertex.position) - referencePlane;

			if ( separation <= 0 && !candidates.full() )
			{
				/* Halfway between the incident point and its projection on the reference face. */
				candidates.push_back({vertex.position - (referenceNormal * (separation * static_cast< precision_t >(0.5))), -separation, faceBits | vertex.key});
			}
		}

		if ( candidates.empty() )
		{
			/* Numerically possible on a grazing contact the SAT still calls overlapping: no point, no contact. */
			return false;
		}

		/* The manifold normal points from A to B: the reference normal when A holds the face, its opposite otherwise. */
		const Vec3 normal = referenceIsB ? -referenceNormal : referenceNormal;

		manifold.setNormal(normal);

		reduceToFour(candidates, normal, manifold);

		return true;
	}
}
