/*
 * src/Math/CurveTessellation.hpp
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
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

/* Local inclusions. */
#include "BSpline.hpp"
#include "Vector.hpp"

/*
 * Adaptive tessellation of curves into polylines (for Scenes::Component::Path, 2026-09-29).
 *
 * Every curve kind is converted into CUBIC BÉZIER spans, then each span is split by de Casteljau at t = 1/2 until it
 * is flat: its two inner control points lie within `tolerance` of the chord. The curve lies inside the convex hull of
 * its control points, so the polyline never strays further than `tolerance` from the curve (a chord error, in the
 * curve's units — metres for a Path). A straight span costs one segment, a tight bend as many as it needs.
 *
 * - polyline(): the points as they are.
 * - bezierPath(): a Math::BSpline — anchors with handles (offsets from the anchor), a curve type per span (None =
 *   straight, BezierQuadratic elevated to a cubic, BezierCubic). Its segment counts are ignored.
 * - uniformBSpline(): a uniform cubic B-spline (C2, does not pass through its control points). Open curves clamp to
 *   their end points (tripled end points); closed ones wrap.
 * - catmullRom(): a Catmull-Rom spline through every point; alpha 0.5 = CENTRIPETAL (no cusp nor self-intersection
 *   within a span: Yuksel, Schaefer, Keyser, "Parameterization and applications of Catmull-Rom curves", CAD 2011).
 *
 * Consecutive coincident points are dropped first (they would make a zero-length span or a division by zero).
 *
 * Along a tessellated curve (for Scenes::Component::Beam, 2026-09-30):
 * - subdivided(): every segment split into equal pieces no longer than the curve's length / N — the corners kept
 *   exactly, at least N segments over the whole curve (an arc needs regular stations, a laser only the corners).
 * - rotationMinimizingNormals(): a normal per point that does not twist around the curve — the double reflection
 *   method (W. Wang, B. Jüttler, D. Zheng, Y. Liu, "Computation of Rotation Minimizing Frames", ACM Transactions on
 *   Graphics 27(1), 2008). On a straight line it is constant.
 */
namespace EmEn::Base::Math::CurveTessellation
{
	/** @brief The chord tolerance used when none is given: 1 cm for curves in metres. */
	template< std::floating_point precision_t >
	constexpr precision_t DefaultTolerance{static_cast< precision_t >(0.01)};

	/** @brief The deepest subdivision of one span (2^16 segments): a guard against a degenerate span, never a target. */
	constexpr uint32_t MaxDepth{16};

	/**
	 * @brief Returns the distance from a point to a segment.
	 * @tparam precision_t The floating point type.
	 * @param point The point.
	 * @param start The segment start.
	 * @param end The segment end.
	 * @return precision_t
	 */
	template< std::floating_point precision_t >
	[[nodiscard]]
	precision_t
	distanceToSegment (const Vector< 3, precision_t > & point, const Vector< 3, precision_t > & start, const Vector< 3, precision_t > & end) noexcept
	{
		const auto segment = end - start;
		const auto lengthSquared = Vector< 3, precision_t >::dotProduct(segment, segment);

		if ( lengthSquared <= std::numeric_limits< precision_t >::min() )
		{
			return (point - start).length();
		}

		const auto t = std::clamp(Vector< 3, precision_t >::dotProduct(point - start, segment) / lengthSquared, static_cast< precision_t >(0), static_cast< precision_t >(1));

		return (point - (start + segment * t)).length();
	}

	/**
	 * @brief Appends a cubic Bézier span to a polyline, EXCLUDING its first point (the previous span's last one).
	 * @tparam precision_t The floating point type.
	 * @param p0 The start.
	 * @param p1 The first inner control point.
	 * @param p2 The second inner control point.
	 * @param p3 The end.
	 * @param tolerance The chord tolerance (> 0).
	 * @param polyline The polyline to append to.
	 * @param depth The current subdivision depth.
	 */
	template< std::floating_point precision_t >
	void
	appendCubic (const Vector< 3, precision_t > & p0, const Vector< 3, precision_t > & p1, const Vector< 3, precision_t > & p2, const Vector< 3, precision_t > & p3, precision_t tolerance, std::vector< Vector< 3, precision_t > > & polyline, uint32_t depth = 0) noexcept
	{
		/* Flat: the convex hull, hence the curve, is within the tolerance of the chord. */
		if ( depth >= MaxDepth || std::max(distanceToSegment(p1, p0, p3), distanceToSegment(p2, p0, p3)) <= tolerance )
		{
			polyline.emplace_back(p3);

			return;
		}

		/* de Casteljau at t = 1/2. */
		constexpr auto Half = static_cast< precision_t >(0.5);

		const auto p01 = (p0 + p1) * Half;
		const auto p12 = (p1 + p2) * Half;
		const auto p23 = (p2 + p3) * Half;
		const auto p012 = (p01 + p12) * Half;
		const auto p123 = (p12 + p23) * Half;
		const auto middle = (p012 + p123) * Half;

		appendCubic(p0, p01, p012, middle, tolerance, polyline, depth + 1);
		appendCubic(middle, p123, p23, p3, tolerance, polyline, depth + 1);
	}

	/**
	 * @brief Returns the points without their consecutive duplicates.
	 * @tparam precision_t The floating point type.
	 * @param points The points.
	 * @param closed Whether the last point also joins the first one (a closing duplicate is dropped too).
	 * @return std::vector< Vector< 3, precision_t > >
	 */
	template< std::floating_point precision_t >
	[[nodiscard]]
	std::vector< Vector< 3, precision_t > >
	withoutDuplicates (std::span< const Vector< 3, precision_t > > points, bool closed = false) noexcept
	{
		constexpr auto Epsilon = static_cast< precision_t >(1.0E-6);

		std::vector< Vector< 3, precision_t > > unique;
		unique.reserve(points.size());

		for ( const auto & point : points )
		{
			if ( unique.empty() || (point - unique.back()).length() > Epsilon )
			{
				unique.emplace_back(point);
			}
		}

		if ( closed && unique.size() > 1 && (unique.back() - unique.front()).length() <= Epsilon )
		{
			unique.pop_back();
		}

		return unique;
	}

	/**
	 * @brief Returns a polyline as it is (its consecutive duplicates dropped).
	 * @tparam precision_t The floating point type.
	 * @param points The points.
	 * @param closed Whether the polyline closes (the first point is repeated at the end).
	 * @return std::vector< Vector< 3, precision_t > >
	 */
	template< std::floating_point precision_t >
	[[nodiscard]]
	std::vector< Vector< 3, precision_t > >
	polyline (std::span< const Vector< 3, precision_t > > points, bool closed = false) noexcept
	{
		auto result = withoutDuplicates(points, closed);

		if ( closed && result.size() > 2 )
		{
			result.emplace_back(result.front());
		}

		return result;
	}

	/**
	 * @brief Tessellates a piecewise Bézier path (Math::BSpline: anchors, handles, a curve type per span).
	 * @tparam precision_t The floating point type.
	 * @param path The path.
	 * @param tolerance The chord tolerance (> 0).
	 * @return std::vector< Vector< 3, precision_t > >
	 */
	template< std::floating_point precision_t >
	[[nodiscard]]
	std::vector< Vector< 3, precision_t > >
	bezierPath (const BSpline< 3, precision_t > & path, precision_t tolerance = DefaultTolerance< precision_t >) noexcept
	{
		const auto & points = path.points();

		std::vector< Vector< 3, precision_t > > result;

		if ( points.empty() )
		{
			return result;
		}

		result.emplace_back(points.front().position());

		constexpr auto TwoThirds = static_cast< precision_t >(2) / static_cast< precision_t >(3);

		for ( size_t index = 0; index + 1 < points.size(); ++index )
		{
			const auto & from = points[index];
			const auto & to = points[index + 1];
			const auto & p0 = from.position();
			const auto & p3 = to.position();

			switch ( from.curveType() )
			{
				case CurveType::BezierQuadratic :
				{
					/* Degree elevation: the quadratic's single control point (the out handle) as two cubic ones. */
					const auto control = p0 + from.handleOut();

					appendCubic(p0, p0 + (control - p0) * TwoThirds, p3 + (control - p3) * TwoThirds, p3, tolerance, result);
				}
					continue;

				case CurveType::BezierCubic :
					appendCubic(p0, p0 + from.handleOut(), p3 + to.handleIn(), p3, tolerance, result);
					continue;

				case CurveType::None :
					break;
			}

			/* NOTE: A straight segment: CurveType::None, or an out-of-range value (a cast integer). */
			result.emplace_back(p3);
		}

		return result;
	}

	/**
	 * @brief Tessellates a uniform cubic B-spline.
	 * @note Each span of four control points (Q0, Q1, Q2, Q3) is the cubic Bézier ((Q0 + 4 Q1 + Q2) / 6, (2 Q1 + Q2) / 3,
	 * (Q1 + 2 Q2) / 3, (Q1 + 4 Q2 + Q3) / 6). An open curve triples its end points: it starts on the first control point
	 * and ends on the last.
	 * @tparam precision_t The floating point type.
	 * @param controlPoints The control points (2 at least).
	 * @param tolerance The chord tolerance (> 0).
	 * @param closed Whether the curve closes on itself (3 control points at least).
	 * @return std::vector< Vector< 3, precision_t > >
	 */
	template< std::floating_point precision_t >
	[[nodiscard]]
	std::vector< Vector< 3, precision_t > >
	uniformBSpline (std::span< const Vector< 3, precision_t > > controlPoints, precision_t tolerance = DefaultTolerance< precision_t >, bool closed = false) noexcept
	{
		const auto unique = withoutDuplicates(controlPoints, closed);

		if ( unique.size() < 2 || (closed && unique.size() < 3) )
		{
			return polyline(std::span< const Vector< 3, precision_t > >{unique}, false);
		}

		/* The span sequence: wrapped when closed, the end points tripled when open. */
		std::vector< Vector< 3, precision_t > > sequence;

		if ( closed )
		{
			sequence = unique;
			sequence.emplace_back(unique[0]);
			sequence.emplace_back(unique[1]);
			sequence.emplace_back(unique[2]);
		}
		else
		{
			sequence.emplace_back(unique.front());
			sequence.emplace_back(unique.front());
			sequence.insert(sequence.end(), unique.begin(), unique.end());
			sequence.emplace_back(unique.back());
			sequence.emplace_back(unique.back());
		}

		constexpr auto Sixth = static_cast< precision_t >(1) / static_cast< precision_t >(6);
		constexpr auto Third = static_cast< precision_t >(1) / static_cast< precision_t >(3);
		constexpr auto Four = static_cast< precision_t >(4);
		constexpr auto Two = static_cast< precision_t >(2);

		std::vector< Vector< 3, precision_t > > result;

		for ( size_t index = 0; index + 3 < sequence.size(); ++index )
		{
			const auto & q0 = sequence[index];
			const auto & q1 = sequence[index + 1];
			const auto & q2 = sequence[index + 2];
			const auto & q3 = sequence[index + 3];

			const auto b0 = (q0 + q1 * Four + q2) * Sixth;
			const auto b1 = (q1 * Two + q2) * Third;
			const auto b2 = (q1 + q2 * Two) * Third;
			const auto b3 = (q1 + q2 * Four + q3) * Sixth;

			if ( result.empty() )
			{
				result.emplace_back(b0);
			}

			appendCubic(b0, b1, b2, b3, tolerance, result);
		}

		return result;
	}

	/**
	 * @brief Tessellates a Catmull-Rom spline, which passes through every point.
	 * @note Span P1 → P2 with its neighbours P0 and P3, knot intervals d = |ΔP|^alpha, becomes the cubic Bézier
	 * (P1, P1 + m1 / 3, P2 − m2 / 3, P2), where m1 = (P2 − P1) + d12 ((P1 − P0) / d01 − (P2 − P0) / (d01 + d12)) and
	 * m2 = (P2 − P1) + d12 ((P3 − P2) / d23 − (P3 − P1) / (d12 + d23)) — the Barry-Goldman tangents rescaled to the span.
	 * An open curve extends its ends by mirrored phantom points.
	 * @tparam precision_t The floating point type.
	 * @param points The points the curve passes through (2 at least).
	 * @param tolerance The chord tolerance (> 0).
	 * @param alpha 0 uniform, 0.5 centripetal (the default: no cusp, no self-intersection within a span), 1 chordal.
	 * @param closed Whether the curve closes on itself (3 points at least).
	 * @return std::vector< Vector< 3, precision_t > >
	 */
	template< std::floating_point precision_t >
	[[nodiscard]]
	std::vector< Vector< 3, precision_t > >
	catmullRom (std::span< const Vector< 3, precision_t > > points, precision_t tolerance = DefaultTolerance< precision_t >, precision_t alpha = static_cast< precision_t >(0.5), bool closed = false) noexcept
	{
		const auto unique = withoutDuplicates(points, closed);

		/* Two points: the curve through them is their segment. */
		if ( unique.size() < 3 )
		{
			return polyline(std::span< const Vector< 3, precision_t > >{unique}, false);
		}

		const auto count = unique.size();
		const auto at = [&unique, count, closed] (std::ptrdiff_t index) noexcept -> Vector< 3, precision_t > {
			const auto size = static_cast< std::ptrdiff_t >(count);

			if ( closed )
			{
				return unique[static_cast< size_t >(((index % size) + size) % size)];
			}

			/* Mirrored phantom points beyond the ends. */
			if ( index < 0 )
			{
				return unique[0] * static_cast< precision_t >(2) - unique[1];
			}

			if ( index >= size )
			{
				return unique[count - 1] * static_cast< precision_t >(2) - unique[count - 2];
			}

			return unique[static_cast< size_t >(index)];
		};

		const auto interval = [alpha] (const Vector< 3, precision_t > & from, const Vector< 3, precision_t > & to) noexcept {
			return std::pow((to - from).length(), alpha);
		};

		constexpr auto Third = static_cast< precision_t >(1) / static_cast< precision_t >(3);

		const auto spanCount = static_cast< std::ptrdiff_t >(closed ? count : count - 1);

		std::vector< Vector< 3, precision_t > > result;
		result.emplace_back(unique.front());

		for ( std::ptrdiff_t span = 0; span < spanCount; ++span )
		{
			const auto p0 = at(span - 1);
			const auto p1 = at(span);
			const auto p2 = at(span + 1);
			const auto p3 = at(span + 2);

			const auto d01 = interval(p0, p1);
			const auto d12 = interval(p1, p2);
			const auto d23 = interval(p2, p3);

			const auto m1 = (p2 - p1) + ((p1 - p0) / d01 - (p2 - p0) / (d01 + d12)) * d12;
			const auto m2 = (p2 - p1) + ((p3 - p2) / d23 - (p3 - p1) / (d12 + d23)) * d12;

			appendCubic(p1, p1 + m1 * Third, p2 - m2 * Third, p2, tolerance, result);
		}

		return result;
	}
	/**
	 * @brief Returns a polyline whose every segment is split into equal pieces no longer than its total length divided
	 * by a segment count: every original point is kept, and the whole polyline has at least that many segments.
	 * @tparam precision_t The floating point type.
	 * @param polyline The polyline (its consecutive points distinct).
	 * @param minimumSegments The segment count over the whole polyline (1 keeps it as it is).
	 * @return std::vector< Vector< 3, precision_t > >
	 */
	template< std::floating_point precision_t >
	[[nodiscard]]
	std::vector< Vector< 3, precision_t > >
	subdivided (std::span< const Vector< 3, precision_t > > polyline, uint32_t minimumSegments) noexcept
	{
		std::vector< Vector< 3, precision_t > > result{polyline.begin(), polyline.end()};

		if ( polyline.size() < 2 || minimumSegments <= 1 )
		{
			return result;
		}

		precision_t total = 0;

		for ( size_t index = 1; index < polyline.size(); ++index )
		{
			total += (polyline[index] - polyline[index - 1]).length();
		}

		if ( total <= std::numeric_limits< precision_t >::min() )
		{
			return result;
		}

		const auto step = total / static_cast< precision_t >(minimumSegments);
		/* A segment exactly N steps long must not become N + 1 pieces through rounding. */
		constexpr auto RoundingSlack = static_cast< precision_t >(1.0E-4);

		result.clear();
		result.emplace_back(polyline.front());

		for ( size_t index = 1; index < polyline.size(); ++index )
		{
			const auto & from = polyline[index - 1];
			const auto & to = polyline[index];
			const auto pieces = std::max< uint32_t >(1U, static_cast< uint32_t >(std::ceil((to - from).length() / step - RoundingSlack)));

			for ( uint32_t piece = 1; piece < pieces; ++piece )
			{
				result.emplace_back(from + (to - from) * (static_cast< precision_t >(piece) / static_cast< precision_t >(pieces)));
			}

			result.emplace_back(to);
		}

		return result;
	}

	/**
	 * @brief Returns a unit normal per point of a polyline that does not twist around it: a rotation minimizing frame
	 * (the double reflection method, Wang, Jüttler, Zheng, Liu, ACM TOG 2008), the binormal being cross(tangent, normal).
	 * @note The tangent at a point is the direction from its previous point to its next one (one-sided at the ends). The
	 * first normal is cross(tangent, +Y) — +X when the tangent is within 8° of Y — normalized: a straight line keeps it.
	 * @tparam precision_t The floating point type.
	 * @param polyline The polyline (its consecutive points distinct).
	 * @return std::vector< Vector< 3, precision_t > > As many normals as points (empty below 2 points).
	 */
	template< std::floating_point precision_t >
	[[nodiscard]]
	std::vector< Vector< 3, precision_t > >
	rotationMinimizingNormals (std::span< const Vector< 3, precision_t > > polyline) noexcept
	{
		using V3 = Vector< 3, precision_t >;

		std::vector< V3 > normals;

		if ( polyline.size() < 2 )
		{
			return normals;
		}

		const auto count = polyline.size();
		const auto tangentAt = [&polyline, count] (size_t index) noexcept {
			const auto & before = polyline[index > 0 ? index - 1 : 0];
			const auto & after = polyline[index + 1 < count ? index + 1 : count - 1];

			return (after - before).normalized();
		};

		constexpr auto Two = static_cast< precision_t >(2);
		constexpr auto Epsilon = static_cast< precision_t >(1.0E-12);

		auto tangent = tangentAt(0);
		const auto reference = std::abs(tangent[Y]) < static_cast< precision_t >(0.99) ? V3::positiveY() : V3::positiveX();

		normals.reserve(count);
		normals.emplace_back(V3::crossProduct(tangent, reference).normalized());

		for ( size_t index = 0; index + 1 < count; ++index )
		{
			const auto & normal = normals.back();
			const auto nextTangent = tangentAt(index + 1);

			/* Reflection 1, by the plane bisecting the two points; reflection 2, by the plane bisecting the reflected
			 * tangent and the next one. */
			const auto v1 = polyline[index + 1] - polyline[index];
			const auto c1 = V3::dotProduct(v1, v1);

			if ( c1 <= Epsilon )
			{
				normals.emplace_back(normal);
				tangent = nextTangent;

				continue;
			}

			const auto reflectedNormal = normal - v1 * (Two / c1 * V3::dotProduct(v1, normal));
			const auto reflectedTangent = tangent - v1 * (Two / c1 * V3::dotProduct(v1, tangent));
			const auto v2 = nextTangent - reflectedTangent;
			const auto c2 = V3::dotProduct(v2, v2);

			auto next = c2 <= Epsilon ? reflectedNormal : reflectedNormal - v2 * (Two / c2 * V3::dotProduct(v2, reflectedNormal));

			/* Kept exactly perpendicular and unit: the rounding of a long curve does not accumulate. */
			next = (next - nextTangent * V3::dotProduct(next, nextTangent)).normalized();

			normals.emplace_back(next);
			tangent = nextTangent;
		}

		return normals;
	}
}
