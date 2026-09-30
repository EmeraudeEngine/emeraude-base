/*
 * src/Math/CurveShape.hpp
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
#include <concepts>
#include <cstdint>
#include <span>
#include <vector>

/* Local inclusions. */
#include "BSpline.hpp"
#include "CurveTessellation.hpp"
#include "Vector.hpp"

namespace EmEn::Base::Math
{
	/** @brief The kinds of curve a CurveShape describes (each tessellated by Math::CurveTessellation). */
	enum class CurveKind : uint8_t
	{
		/** @brief The points as they are. */
		Polyline,
		/** @brief A piecewise Bézier path (Math::BSpline: anchors, handles, a curve type per span). */
		BezierPath,
		/** @brief A uniform cubic B-spline (smooth, not through its control points). */
		UniformBSpline,
		/** @brief A Catmull-Rom spline through every point (centripetal by default). */
		CatmullRom
	};

	/**
	 * @brief Returns the name of a curve kind.
	 * @param kind The kind.
	 * @return const char *
	 */
	[[nodiscard]]
	constexpr
	const char *
	to_cstring (CurveKind kind) noexcept
	{
		switch ( kind )
		{
			case CurveKind::Polyline :
				return "Polyline";

			case CurveKind::BezierPath :
				return "BezierPath";

			case CurveKind::UniformBSpline :
				return "UniformBSpline";

			case CurveKind::CatmullRom :
				return "CatmullRom";
		}

		return "Unknown";
	}

	/**
	 * @brief The DESCRIPTION of a curve — its kind, its points, closed or not — and its tessellation into a polyline.
	 * @note Shared by the drawn curves of the engine (Scenes::Component::Path, Scenes::Component::Beam): one set of
	 * curve kinds, one tessellation (Math::CurveTessellation).
	 * @note The FIRST and LAST points can be moved alone (setFirstPoint(), setLastPoint()): a beam's endpoints, an end
	 * that follows another entity. For a Bézier path they are its first and last anchors, their handles kept.
	 * @tparam precision_t The floating point type.
	 */
	template< std::floating_point precision_t >
	class CurveShape final
	{
		public:

			using Point = Vector< 3, precision_t >;

			/**
			 * @brief Constructs an empty polyline.
			 */
			CurveShape () noexcept = default;

			/**
			 * @brief Describes a POLYLINE.
			 * @param points The points.
			 * @param closed Whether the last point joins the first.
			 * @return void
			 */
			void
			setPolyline (std::span< const Point > points, bool closed = false) noexcept
			{
				m_kind = CurveKind::Polyline;
				m_points.assign(points.begin(), points.end());
				m_closed = closed;
			}

			/**
			 * @brief Describes a piecewise BÉZIER path (its segment counts are ignored: the tessellation is adaptive).
			 * @param path The anchors, their handles (offsets from the anchor) and a curve type per span.
			 * @return void
			 */
			void
			setBezierPath (const BSpline< 3, precision_t > & path) noexcept
			{
				m_kind = CurveKind::BezierPath;
				m_bezierPath = path;
				m_points.clear();

				for ( const auto & point : path.points() )
				{
					m_points.emplace_back(point.position());
				}

				m_closed = false;
			}

			/**
			 * @brief Describes a uniform cubic B-SPLINE. An open one starts and ends on its end points.
			 * @param controlPoints The control points.
			 * @param closed Whether the curve closes on itself.
			 * @return void
			 */
			void
			setUniformBSpline (std::span< const Point > controlPoints, bool closed = false) noexcept
			{
				m_kind = CurveKind::UniformBSpline;
				m_points.assign(controlPoints.begin(), controlPoints.end());
				m_closed = closed;
			}

			/**
			 * @brief Describes a CATMULL-ROM spline (through every point).
			 * @param points The points.
			 * @param closed Whether the curve closes on itself.
			 * @param alpha 0 uniform, 0.5 centripetal (the default: no cusp, no loop), 1 chordal.
			 * @return void
			 */
			void
			setCatmullRom (std::span< const Point > points, bool closed = false, precision_t alpha = static_cast< precision_t >(0.5)) noexcept
			{
				m_kind = CurveKind::CatmullRom;
				m_points.assign(points.begin(), points.end());
				m_closed = closed;
				m_alpha = std::clamp(alpha, static_cast< precision_t >(0), static_cast< precision_t >(1));
			}

			/**
			 * @brief Moves the first point (the first anchor of a Bézier path). Nothing when there is no point.
			 * @param position The position.
			 * @return void
			 */
			void
			setFirstPoint (const Point & position) noexcept
			{
				if ( m_points.empty() )
				{
					return;
				}

				m_points.front() = position;

				if ( m_kind == CurveKind::BezierPath )
				{
					this->moveAnchor(0, position);
				}
			}

			/**
			 * @brief Moves the last point (the last anchor of a Bézier path). Nothing when there is no point.
			 * @param position The position.
			 * @return void
			 */
			void
			setLastPoint (const Point & position) noexcept
			{
				if ( m_points.empty() )
				{
					return;
				}

				m_points.back() = position;

				if ( m_kind == CurveKind::BezierPath )
				{
					this->moveAnchor(m_points.size() - 1, position);
				}
			}

			/**
			 * @brief Returns the kind of curve.
			 * @return CurveKind
			 */
			[[nodiscard]]
			CurveKind
			kind () const noexcept
			{
				return m_kind;
			}

			/**
			 * @brief Returns whether the curve is closed.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isClosed () const noexcept
			{
				return m_closed;
			}

			/**
			 * @brief Returns the Catmull-Rom parameterization (0 uniform, 0.5 centripetal, 1 chordal).
			 * @return precision_t
			 */
			[[nodiscard]]
			precision_t
			alpha () const noexcept
			{
				return m_alpha;
			}

			/**
			 * @brief Returns the points as given: a polyline's points, a spline's control points, a Bézier path's anchors.
			 * @return const std::vector< Point > &
			 */
			[[nodiscard]]
			const std::vector< Point > &
			points () const noexcept
			{
				return m_points;
			}

			/**
			 * @brief Returns the Bézier path (meaningful for CurveKind::BezierPath only).
			 * @return const BSpline< 3, precision_t > &
			 */
			[[nodiscard]]
			const BSpline< 3, precision_t > &
			bezierPath () const noexcept
			{
				return m_bezierPath;
			}

			/**
			 * @brief Tessellates the curve into a polyline within a chord tolerance (Math::CurveTessellation).
			 * @param tolerance The largest distance between the curve and its polyline (> 0).
			 * @return std::vector< Point >
			 */
			[[nodiscard]]
			std::vector< Point >
			tessellate (precision_t tolerance) const noexcept
			{
				const std::span< const Point > source{m_points};

				switch ( m_kind )
				{
					case CurveKind::Polyline :
						return CurveTessellation::polyline(source, m_closed);

					case CurveKind::BezierPath :
						return CurveTessellation::bezierPath(m_bezierPath, tolerance);

					case CurveKind::UniformBSpline :
						return CurveTessellation::uniformBSpline(source, tolerance, m_closed);

					case CurveKind::CatmullRom :
						return CurveTessellation::catmullRom(source, tolerance, m_alpha, m_closed);
				}

				return {};
			}

		private:

			/**
			 * @brief Moves one anchor of the Bézier path, its handles and curve types kept (a BSpline point is immutable:
			 * the path is rebuilt).
			 * @param anchorIndex The anchor index.
			 * @param position The new position.
			 * @return void
			 */
			void
			moveAnchor (size_t anchorIndex, const Point & position) noexcept
			{
				BSpline< 3, precision_t > moved{m_bezierPath.defaultSegments(), m_bezierPath.defaultCurveType()};
				const auto & anchors = m_bezierPath.points();

				for ( size_t index = 0; index < anchors.size(); ++index )
				{
					const auto & anchor = anchors[index];

					moved.addPoint(index == anchorIndex ? position : anchor.position(), anchor.handleIn(), anchor.handleOut())
						.setCurveType(anchor.curveType())
						.setSegments(anchor.segments());
				}

				m_bezierPath = moved;
			}

			BSpline< 3, precision_t > m_bezierPath;
			std::vector< Point > m_points;
			precision_t m_alpha{static_cast< precision_t >(0.5)};
			CurveKind m_kind{CurveKind::Polyline};
			bool m_closed{false};
	};
}
