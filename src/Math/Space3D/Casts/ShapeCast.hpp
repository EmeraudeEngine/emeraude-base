/*
 * src/Math/Space3D/Casts/ShapeCast.hpp
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
#include <sstream>
#include <string>
#include <type_traits>

/* Local inclusions for usages. */
#include "Math/Space3D/Capsule.hpp"
#include "Math/Space3D/Casts/ConvexDistance.hpp"
#include "Math/Space3D/Contacts/BoxBox.hpp"
#include "Math/Space3D/Contacts/CapsuleBox.hpp"
#include "Math/Space3D/Contacts/CapsuleTriangle.hpp"
#include "Math/Space3D/Contacts/RoundShapes.hpp"
#include "Math/Space3D/Contacts/SphereTriangle.hpp"
#include "Math/Space3D/OrientedBox.hpp"
#include "Math/Space3D/Sphere.hpp"
#include "Math/Space3D/Triangle.hpp"
#include "Math/Vector.hpp"

/*
 * Linear casts: a ray, a sphere or a capsule moved by a displacement against an oriented box, a triangle, a sphere or
 * a capsule — the first contact along the motion, its fraction, point and surface normal. What a character controller
 * sweeps its capsule with (collide and slide), and what a ground probe casts.
 *
 * Method: CONSERVATIVE ADVANCEMENT on the exact closest points. The caster is a point (ray, sphere) or a segment
 * (capsule) with a radius; at each step the exact distance d between the caster and the target is computed, with the
 * unit direction n from the caster towards the target; the caster then advances by d / (motion · n). For a translating
 * convex shape this never overshoots — the target lies beyond the separating plane of normal n — and converges on the
 * first contact (G. van den Bergen, "Ray Casting against General Convex Objects with Application to Continuous
 * Collision Detection", 2004; E. Catto, "Continuous Collision", GDC 2013). No third-party code.
 * A BOX caster (castBox(), physics overhaul P5, decision 14) uses the same advancement on the closest points GJK gives
 * between the moved box and the target's core (ConvexDistance.hpp): exact for a translating box, not its inscribed
 * sphere.
 */

namespace EmEn::Base::Math::Space3D
{
	/**
	 * @brief The first contact of a linear cast.
	 * @tparam precision_t The precision type. Default float.
	 */
	template< typename precision_t = float >
	requires (std::is_floating_point_v< precision_t >)
	class CastHit final
	{
		public:

			/**
			 * @brief Constructs an empty hit.
			 */
			constexpr CastHit () noexcept = default;

			/**
			 * @brief Constructs a hit.
			 * @param fraction The fraction of the motion travelled before the contact, in [0, 1].
			 * @param point A reference to the contact point, on the target's surface.
			 * @param normal A reference to the target's surface normal there, pointing back towards the caster (unit).
			 * @param startedInside Whether the caster already touched the target before moving (fraction 0; the normal
			 * is then the direction that separates it).
			 */
			constexpr
			CastHit (precision_t fraction, const Vector< 3, precision_t > & point, const Vector< 3, precision_t > & normal, bool startedInside) noexcept
				: m_point{point},
				m_normal{normal},
				m_fraction{fraction},
				m_startedInside{startedInside}
			{

			}

			/**
			 * @brief Returns the fraction of the motion travelled before the contact, in [0, 1].
			 * @return precision_t
			 */
			[[nodiscard]]
			constexpr
			precision_t
			fraction () const noexcept
			{
				return m_fraction;
			}

			/**
			 * @brief Returns the contact point, on the target's surface.
			 * @return const Vector< 3, precision_t > &
			 */
			[[nodiscard]]
			constexpr
			const Vector< 3, precision_t > &
			point () const noexcept
			{
				return m_point;
			}

			/**
			 * @brief Returns the target's surface normal at the contact, pointing back towards the caster (unit).
			 * @return const Vector< 3, precision_t > &
			 */
			[[nodiscard]]
			constexpr
			const Vector< 3, precision_t > &
			normal () const noexcept
			{
				return m_normal;
			}

			/**
			 * @brief Returns whether the caster already touched the target before moving.
			 * @return bool
			 */
			[[nodiscard]]
			constexpr
			bool
			startedInside () const noexcept
			{
				return m_startedInside;
			}

			/**
			 * @brief STL streams printable object.
			 * @param out A reference to the stream output.
			 * @param obj A reference to the object to print.
			 * @return std::ostream &
			 */
			friend
			std::ostream &
			operator<< (std::ostream & out, const CastHit & obj) noexcept
			{
				return out << "Cast hit at fraction " << obj.m_fraction << " point " << obj.m_point << " normal " << obj.m_normal << (obj.m_startedInside ? " (started inside)" : "") << '\n';
			}

			/**
			 * @brief Stringifies the object.
			 * @param obj A reference to the object to print.
			 * @return std::string
			 */
			friend
			std::string
			to_string (const CastHit & obj) noexcept
			{
				std::stringstream output;

				output << obj;

				return output.str();
			}

		private:

			Vector< 3, precision_t > m_point;
			Vector< 3, precision_t > m_normal;
			precision_t m_fraction{0};
			bool m_startedInside{false};
	};

	namespace ShapeCastDetail
	{
		/** @brief The closest points of the caster's core (a point or a segment) and the target's core. */
		template< typename precision_t >
		struct CoreClosest final
		{
			Vector< 3, precision_t > onCaster;
			Vector< 3, precision_t > onTarget;
			/** Whether the target is usable (a degenerate triangle is not). */
			bool valid{true};
		};

		/** @brief The closest points of a segment and an oriented box (exact). */
		template< typename precision_t >
		[[nodiscard]]
		CoreClosest< precision_t >
		coreClosest (const Vector< 3, precision_t > & start, const Vector< 3, precision_t > & end, const OrientedBox< precision_t > & box) noexcept
		{
			using Vec3 = Vector< 3, precision_t >;

			const Vec3 center = (start + end) * static_cast< precision_t >(0.5);
			Vec3 direction = end - start;
			const precision_t length = direction.length();
			precision_t halfLength = 0;

			if ( length > std::numeric_limits< precision_t >::epsilon() )
			{
				direction *= static_cast< precision_t >(1) / length;
				halfLength = length * static_cast< precision_t >(0.5);
			}

			const auto closest = CapsuleBoxDetail::closestOfSegmentAndBox(center, direction, halfLength, box);
			const Vec3 onSegment = center + (direction * closest.parameter);
			Vec3 onBox = box.center();

			for ( size_t index = 0; index < 3; ++index )
			{
				const precision_t local = Vec3::dotProduct(onSegment - box.center(), box.axis(index));

				onBox += box.axis(index) * std::clamp(local, -box.halfExtent(index), box.halfExtent(index));
			}

			return {onSegment, onBox, true};
		}

		/** @brief The closest points of a segment and a triangle (exact); invalid for a degenerate triangle. */
		template< typename precision_t >
		[[nodiscard]]
		CoreClosest< precision_t >
		coreClosest (const Vector< 3, precision_t > & start, const Vector< 3, precision_t > & end, const Triangle< precision_t > & triangle) noexcept
		{
			Vector< 3, precision_t > faceNormal;

			if ( !TriangleDetail::unitNormal(triangle, faceNormal) )
			{
				return {start, start, false};
			}

			const auto closest = CapsuleTriangleDetail::closestOfSegmentAndTriangle(start, end, triangle, faceNormal);

			return {closest.onSegment, closest.onTriangle, true};
		}

		/** @brief The closest points of a segment and a sphere's centre. */
		template< typename precision_t >
		[[nodiscard]]
		CoreClosest< precision_t >
		coreClosest (const Vector< 3, precision_t > & start, const Vector< 3, precision_t > & end, const Sphere< precision_t > & sphere) noexcept
		{
			using Vec3 = Vector< 3, precision_t >;

			const Vec3 segment = end - start;
			const precision_t lengthSquared = segment.lengthSquared();
			precision_t parameter = 0;

			if ( lengthSquared > std::numeric_limits< precision_t >::min() )
			{
				parameter = std::clamp(Vec3::dotProduct(sphere.position() - start, segment) / lengthSquared, static_cast< precision_t >(0), static_cast< precision_t >(1));
			}

			return {start + (segment * parameter), sphere.position(), true};
		}

		/** @brief The closest points of a segment and a capsule's axis. */
		template< typename precision_t >
		[[nodiscard]]
		CoreClosest< precision_t >
		coreClosest (const Vector< 3, precision_t > & start, const Vector< 3, precision_t > & end, const Capsule< precision_t > & capsule) noexcept
		{
			using Vec3 = Vector< 3, precision_t >;

			Vec3 centerA;
			Vec3 directionA;
			precision_t halfLengthA = 0;
			Vec3 centerB;
			Vec3 directionB;
			precision_t halfLengthB = 0;

			RoundShapesDetail::decompose(Capsule< precision_t >{start, end, static_cast< precision_t >(1)}, centerA, directionA, halfLengthA);
			RoundShapesDetail::decompose(capsule, centerB, directionB, halfLengthB);

			CoreClosest< precision_t > closest;

			BoxBoxDetail::closestPointsOfSegments(centerA, directionA, halfLengthA, centerB, directionB, halfLengthB, closest.onCaster, closest.onTarget);

			return closest;
		}

		/** @brief The radius that wraps the target's core (0 for a box or a triangle). */
		template< typename precision_t >
		[[nodiscard]]
		constexpr
		precision_t
		targetRadius (const OrientedBox< precision_t > & /*box*/) noexcept
		{
			return 0;
		}

		template< typename precision_t >
		[[nodiscard]]
		constexpr
		precision_t
		targetRadius (const Triangle< precision_t > & /*triangle*/) noexcept
		{
			return 0;
		}

		template< typename precision_t >
		[[nodiscard]]
		constexpr
		precision_t
		targetRadius (const Sphere< precision_t > & sphere) noexcept
		{
			return sphere.radius();
		}

		template< typename precision_t >
		[[nodiscard]]
		constexpr
		precision_t
		targetRadius (const Capsule< precision_t > & capsule) noexcept
		{
			return capsule.radius();
		}

		/**
		 * @brief The conservative advancement of a round core (segment start / end, radius) along a motion.
		 * @return bool True on a contact within the motion.
		 */
		template< typename precision_t, typename target_t >
		[[nodiscard]]
		bool
		castCore (const Vector< 3, precision_t > & start, const Vector< 3, precision_t > & end, precision_t radius, const Vector< 3, precision_t > & motion, const target_t & target, CastHit< precision_t > & hit) noexcept
		{
			using Vec3 = Vector< 3, precision_t >;

			/* A contact is declared within 0.1 mm; a grazing cast converges linearly, hence the generous step cap. */
			constexpr auto CastContactTolerance = static_cast< precision_t >(1.0e-4);
			constexpr auto CastDirectionThreshold = static_cast< precision_t >(1.0e-7);
			constexpr size_t CastMaxIterations{64};

			const precision_t reach = radius + targetRadius(target);
			precision_t fraction = 0;

			for ( size_t iteration = 0; iteration < CastMaxIterations; ++iteration )
			{
				const Vec3 offset = motion * fraction;
				const auto closest = coreClosest(start + offset, end + offset, target);

				if ( !closest.valid )
				{
					return false;
				}

				const Vec3 between = closest.onTarget - closest.onCaster;
				const precision_t coreDistance = between.length();
				const precision_t distance = coreDistance - reach;

				/* The direction from the target back towards the caster (the surface normal at the contact). */
				Vec3 outward;

				if ( coreDistance > CastDirectionThreshold )
				{
					outward = between * (static_cast< precision_t >(-1) / coreDistance);
				}
				else
				{
					/* The cores touch: undefined; back along the motion, or +Y without motion (deterministic). */
					const precision_t motionLength = motion.length();

					outward = motionLength > CastDirectionThreshold ? motion * (static_cast< precision_t >(-1) / motionLength) : Vec3{0, 1, 0};
				}

				if ( distance <= CastContactTolerance )
				{
					/* Touching at the start but moving away (a character leaving the ground): not a contact. A start
					 * that already PENETRATES is always reported (startedInside), for the caller to resolve. */
					if ( iteration == 0 && distance >= 0 && coreDistance > CastDirectionThreshold && Vec3::dotProduct(motion, outward) > CastDirectionThreshold )
					{
						return false;
					}

					/* Started inside: penetrating, or cores overlapping (a point inside a box or a triangle's plane has a
					 * core distance of 0, not a negative one). */
					const bool startedInside = iteration == 0 && (distance < 0 || coreDistance <= CastDirectionThreshold);

					hit = {fraction, closest.onTarget + (outward * targetRadius(target)), outward, startedInside};

					return true;
				}

				/* The speed at which the gap closes along the direction towards the target. */
				const precision_t approach = -Vec3::dotProduct(motion, outward);

				if ( approach <= CastDirectionThreshold )
				{
					/* Moving away or along the separating plane: no contact. */
					return false;
				}

				fraction += distance / approach;

				if ( fraction > static_cast< precision_t >(1) )
				{
					return false;
				}
			}

			/* Not converged within the step cap (a grazing pass): reported as no contact. */
			return false;
		}

		/** @brief The core polytope of a target (its radius apart): a box, a triangle, a sphere's centre, a capsule's axis. */
		template< typename precision_t >
		[[nodiscard]]
		ConvexPolytope< precision_t >
		corePolytope (const OrientedBox< precision_t > & box) noexcept
		{
			return ConvexPolytope< precision_t >{box};
		}

		template< typename precision_t >
		[[nodiscard]]
		ConvexPolytope< precision_t >
		corePolytope (const Triangle< precision_t > & triangle) noexcept
		{
			Vector< 3, precision_t > faceNormal;

			/* A degenerate triangle is no target (empty: GJK answers overlapping, refused below). */
			if ( !TriangleDetail::unitNormal(triangle, faceNormal) )
			{
				return {};
			}

			return ConvexPolytope< precision_t >{triangle};
		}

		template< typename precision_t >
		[[nodiscard]]
		ConvexPolytope< precision_t >
		corePolytope (const Sphere< precision_t > & sphere) noexcept
		{
			return ConvexPolytope< precision_t >{sphere.position()};
		}

		template< typename precision_t >
		[[nodiscard]]
		ConvexPolytope< precision_t >
		corePolytope (const Capsule< precision_t > & capsule) noexcept
		{
			return ConvexPolytope< precision_t >{capsule.startPoint(), capsule.endPoint()};
		}

		/** @brief The face normals a box cast's normal may snap to: a target box's, a triangle's (none for a round target). */
		template< typename precision_t >
		bool
		snapToFace (const OrientedBox< precision_t > & box, Vector< 3, precision_t > & normal, precision_t cosine) noexcept
		{
			for ( const auto & axis : box.axes() )
			{
				for ( const auto & face : {axis, -axis} )
				{
					if ( Vector< 3, precision_t >::dotProduct(normal, face) >= cosine )
					{
						normal = face;

						return true;
					}
				}
			}

			return false;
		}

		template< typename precision_t >
		bool
		snapToFace (const Triangle< precision_t > & triangle, Vector< 3, precision_t > & normal, precision_t cosine) noexcept
		{
			Vector< 3, precision_t > face;

			if ( !TriangleDetail::unitNormal(triangle, face) )
			{
				return false;
			}

			if ( Vector< 3, precision_t >::dotProduct(normal, face) >= cosine )
			{
				normal = face;

				return true;
			}

			if ( Vector< 3, precision_t >::dotProduct(normal, -face) >= cosine )
			{
				normal = -face;

				return true;
			}

			return false;
		}

		template< typename precision_t >
		bool
		snapToFace (const Sphere< precision_t > & /*sphere*/, Vector< 3, precision_t > & /*normal*/, precision_t /*cosine*/) noexcept
		{
			return false;
		}

		template< typename precision_t >
		bool
		snapToFace (const Capsule< precision_t > & /*capsule*/, Vector< 3, precision_t > & /*normal*/, precision_t /*cosine*/) noexcept
		{
			return false;
		}

		/**
		 * @brief The conservative advancement of a translating box: as castCore(), the closest points from GJK.
		 * @note A face against a face has no unique pair of closest points: the normal GJK gives leans by a few 1e-4 rad.
		 * Within 0.01 rad of a face normal — the target's, or the box's own facing it — it becomes that normal exactly.
		 */
		template< typename precision_t, typename target_t >
		[[nodiscard]]
		bool
		castBoxCore (const OrientedBox< precision_t > & box, const Vector< 3, precision_t > & motion, const target_t & target, CastHit< precision_t > & hit) noexcept
		{
			using Vec3 = Vector< 3, precision_t >;

			/* A box's contact is declared within 1 mm and approached to 0.5 mm: the direction between two closest points
			 * a few hundredths of a millimetre apart is noise in float (the normal of a wall leaned 0.0024 rad at x
			 * -0.1, z -89, and a bounced box drifted over the wall). */
			constexpr auto CastContactTolerance = static_cast< precision_t >(1.0e-3);
			constexpr auto CastDirectionThreshold = static_cast< precision_t >(1.0e-7);
			/* How far along its motion a box in exact contact is probed to tell leaving from pressing (m). */
			constexpr auto CastLeaveProbe = static_cast< precision_t >(1.0e-3);
			/* cos(0.01 rad): a normal this close to a face normal is that face's. */
			constexpr auto CastSnapCosine = static_cast< precision_t >(0.99995);
			constexpr size_t CastMaxIterations{64};

			const auto targetCore = corePolytope(target);

			if ( targetCore.empty() || !box.isValid() )
			{
				return false;
			}

			/* GJK runs about the box's start: coordinates of the order of the shapes, not of the world (precision). */
			const Vec3 & origin = box.center();
			const ConvexPolytope< precision_t > caster = ConvexPolytope< precision_t >{box}.translated(-origin);
			const auto localTarget = targetCore.translated(-origin);
			const precision_t reach = targetRadius(target);
			precision_t fraction = 0;

			for ( size_t iteration = 0; iteration < CastMaxIterations; ++iteration )
			{
				const auto closest = closestPoints(caster.translated(motion * fraction), localTarget);
				const Vec3 between = closest.onB - closest.onA;
				const precision_t coreDistance = closest.overlapping ? static_cast< precision_t >(0) : between.length();
				const precision_t distance = coreDistance - reach;
				Vec3 outward;

				if ( coreDistance > CastDirectionThreshold )
				{
					outward = between * (static_cast< precision_t >(-1) / coreDistance);
				}
				else
				{
					/* Overlapping cores: back along the motion, or +Y without motion (deterministic). */
					const precision_t motionLength = motion.length();

					outward = motionLength > CastDirectionThreshold ? motion * (static_cast< precision_t >(-1) / motionLength) : Vec3{0, 1, 0};
				}

				if ( distance <= CastContactTolerance )
				{
					/* Touching at the start but moving away: not a contact. */
					if ( iteration == 0 && distance >= 0 && coreDistance > CastDirectionThreshold && Vec3::dotProduct(motion, outward) > CastDirectionThreshold )
					{
						return false;
					}

					/* In exact contact at the start, GJK has no direction: a box that a millimetre along its motion is
					 * apart is leaving (a box resting on the ground and lifted). */
					if ( iteration == 0 && coreDistance <= CastDirectionThreshold )
					{
						const precision_t motionLength = motion.length();

						if ( motionLength > CastDirectionThreshold )
						{
							const auto probe = closestPoints(caster.translated(motion * (std::min(static_cast< precision_t >(1), CastLeaveProbe / motionLength))), localTarget);

							if ( !probe.overlapping && probe.distance > reach )
							{
								return false;
							}
						}
					}

					const bool startedInside = iteration == 0 && (distance < 0 || coreDistance <= CastDirectionThreshold);
					const Vec3 contact = origin + (coreDistance > CastDirectionThreshold ? closest.onB + (outward * reach) : motion * fraction);

					/* The target's face, else the box's own face turned to it (its outward normal is -outward). */
					if ( !snapToFace(target, outward, CastSnapCosine) )
					{
						Vec3 inward = -outward;

						if ( snapToFace(box, inward, CastSnapCosine) )
						{
							outward = -inward;
						}
					}

					hit = {fraction, contact, outward, startedInside};

					return true;
				}

				const precision_t approach = -Vec3::dotProduct(motion, outward);

				if ( approach <= CastDirectionThreshold )
				{
					return false;
				}

				/* To half the tolerance short of the contact: GJK has no closest points at a distance of exactly 0. */
				fraction += (distance - (CastContactTolerance * static_cast< precision_t >(0.5))) / approach;

				if ( fraction > static_cast< precision_t >(1) )
				{
					return false;
				}
			}

			return false;
		}
	}

	/**
	 * @brief Casts a ray (a moving point) along a motion against a target.
	 * @note The fraction is relative to the motion: the ray covers origin → origin + motion.
	 * @tparam target_t An OrientedBox, a Triangle, a Sphere or a Capsule.
	 * @param origin A reference to the ray origin.
	 * @param motion A reference to the motion (direction × length).
	 * @param target A reference to the target.
	 * @param hit A reference to the hit written on a contact.
	 * @return bool True on a contact within the motion.
	 */
	template< typename precision_t, typename target_t >
	[[nodiscard]]
	bool
	castRay (const Vector< 3, precision_t > & origin, const Vector< 3, precision_t > & motion, const target_t & target, CastHit< precision_t > & hit) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		return ShapeCastDetail::castCore(origin, origin, static_cast< precision_t >(0), motion, target, hit);
	}

	/**
	 * @brief Casts a sphere along a motion against a target.
	 * @tparam target_t An OrientedBox, a Triangle, a Sphere or a Capsule.
	 * @param sphere A reference to the sphere at its start. @pre sphere.isValid().
	 * @param motion A reference to the motion (direction × length).
	 * @param target A reference to the target.
	 * @param hit A reference to the hit written on a contact.
	 * @return bool True on a contact within the motion.
	 */
	template< typename precision_t, typename target_t >
	[[nodiscard]]
	bool
	castSphere (const Sphere< precision_t > & sphere, const Vector< 3, precision_t > & motion, const target_t & target, CastHit< precision_t > & hit) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		return ShapeCastDetail::castCore(sphere.position(), sphere.position(), sphere.radius(), motion, target, hit);
	}

	/**
	 * @brief Casts a capsule along a motion against a target (no rotation during the motion).
	 * @tparam target_t An OrientedBox, a Triangle, a Sphere or a Capsule.
	 * @param capsule A reference to the capsule at its start. @pre capsule.isValid().
	 * @param motion A reference to the motion (direction × length).
	 * @param target A reference to the target.
	 * @param hit A reference to the hit written on a contact.
	 * @return bool True on a contact within the motion.
	 */
	template< typename precision_t, typename target_t >
	[[nodiscard]]
	bool
	castCapsule (const Capsule< precision_t > & capsule, const Vector< 3, precision_t > & motion, const target_t & target, CastHit< precision_t > & hit) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		return ShapeCastDetail::castCore(capsule.startPoint(), capsule.endPoint(), capsule.radius(), motion, target, hit);
	}

	/**
	 * @brief Casts an oriented box (TRANSLATED, not turned) along a motion against a target (P5, decision 14).
	 * @note Exact for the box's faces, edges and corners — before, a fast box swept its inscribed sphere and stopped
	 * later than its faces. The same contract as castSphere(): the fraction of the motion at the first contact, the
	 * contact point on the target's surface, the target's surface normal (towards the caster), `startedInside`.
	 * @tparam precision_t The floating point type.
	 * @tparam target_t An OrientedBox, a Triangle, a Sphere or a Capsule.
	 * @param box A reference to the box at the start of the motion.
	 * @param motion A reference to the displacement.
	 * @param target A reference to the target.
	 * @param hit A reference to the hit written on a contact.
	 * @return bool
	 */
	template< typename precision_t, typename target_t >
	[[nodiscard]]
	bool
	castBox (const OrientedBox< precision_t > & box, const Vector< 3, precision_t > & motion, const target_t & target, CastHit< precision_t > & hit) noexcept requires (std::is_floating_point_v< precision_t >)
	{
		return ShapeCastDetail::castBoxCore(box, motion, target, hit);
	}
}
