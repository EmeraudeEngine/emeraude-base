/*
 * src/Math/Space2D/AARectangle.hpp
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
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>

/* Local inclusions for usages. */
#include "Point.hpp"

namespace EmEn::Base::Math::Space2D
{
	/**
	 * @brief Class for an axis-aligned rectangle in 2D space.
	 * @note The origin is top-left (Positive Y goes downward, Positive X goes rightward).
	 * @note Internally stored as a minimum (top-left) and a maximum (bottom-right) point, mirroring
	 * EmEn::Base::Math::Space3D::AACuboid. This makes point-cloud accumulation through merge() natural.
	 * @tparam precision_t The precision type. Default float.
	 */
	template< typename precision_t = float >
	requires (std::is_arithmetic_v< precision_t >)
	class AARectangle final
	{
		public:

			/**
			 * @brief Constructs a default (empty) rectangle.
			 * @note Like EmEn::Base::Math::Space3D::AACuboid, the default state is inverted/empty: isValid()
			 * returns false and the first merge() initializes the bounds. For a 0,0,1,1 unit box use AARectangle::Unit().
			 */
			constexpr AARectangle () noexcept = default;

			/**
			 * @brief Constructs a rectangle with dimensions.
			 * @note Negative width or height values will result in a dimension of 0.
			 * @param width The width of the rectangle.
			 * @param height The height of the rectangle.
			 */
			constexpr
			AARectangle (precision_t width, precision_t height) noexcept
				: m_minimum{0, 0},
				m_maximum{width < 0 ? 0 : width, height < 0 ? 0 : height}
			{

			}

			/**
			 * @brief Constructs a rectangle with position and dimensions.
			 * @note Negative width or height values will result in a dimension of 0.
			 * @param positionX The left position of the rectangle.
			 * @param positionY The top position of the rectangle.
			 * @param width The width of the rectangle.
			 * @param height The height of the rectangle.
			 */
			constexpr
			AARectangle (precision_t positionX, precision_t positionY, precision_t width, precision_t height) noexcept
				: m_minimum{positionX, positionY},
				m_maximum{positionX + (width < 0 ? 0 : width), positionY + (height < 0 ? 0 : height)}
			{

			}

			/**
			 * @brief Returns a unit rectangle (origin 0,0 with a 1x1 surface).
			 * @note Convenient canonical full-surface geometry (e.g. a screen-filling overlay).
			 * @return AARectangle
			 */
			[[nodiscard]]
			static constexpr
			AARectangle
			Unit () noexcept
			{
				return {0, 0, 1, 1};
			}

			/**
			 * @brief Returns a zero rectangle (origin 0,0 with no surface).
			 * @note Concrete origin-anchored seed for the position+size setters (setLeft/setWidth/...),
			 * which cannot build upon the empty/inverted default state. Distinct from the default ctor,
			 * which yields the empty/inverted state.
			 * @return AARectangle
			 */
			[[nodiscard]]
			static constexpr
			AARectangle
			Zero () noexcept
			{
				return {0, 0, 0, 0};
			}

			/**
			 * @brief Returns whether the area of another rectangle is bigger.
			 * @param operand A reference to another rectangle.
			 * @return bool
			 */
			[[nodiscard]]
			constexpr
			bool
			operator> (const AARectangle< precision_t > & operand) const noexcept
			{
				return this->width() * this->height() > operand.width() * operand.height();
			}

			/**
			 * @brief Returns whether the area of another rectangle is bigger or equal.
			 * @param operand A reference to another rectangle.
			 * @return bool
			 */
			[[nodiscard]]
			constexpr
			bool
			operator>= (const AARectangle< precision_t > & operand) const noexcept
			{
				return this->width() * this->height() >= operand.width() * operand.height();
			}

			/**
			 * @brief Returns whether the area of another rectangle is smaller.
			 * @param operand A reference to another rectangle.
			 * @return bool
			 */
			[[nodiscard]]
			constexpr
			bool
			operator< (const AARectangle< precision_t > & operand) const noexcept
			{
				return this->width() * this->height() < operand.width() * operand.height();
			}

			/**
			 * @brief Returns whether the area of another rectangle is smaller or equal.
			 * @param operand A reference to another rectangle.
			 * @return bool
			 */
			[[nodiscard]]
			constexpr
			bool
			operator<= (const AARectangle< precision_t > & operand) const noexcept
			{
				return this->width() * this->height() <= operand.width() * operand.height();
			}

			/**
			 * @brief Returns whether the area of another rectangle is equal.
			 * @param operand A reference to another rectangle.
			 * @return bool
			 */
			[[nodiscard]]
			constexpr
			bool
			operator== (const AARectangle< precision_t > & operand) const noexcept
			{
				return m_minimum == operand.m_minimum && m_maximum == operand.m_maximum;
			}

			/**
			 * @brief Returns whether the area of another rectangle is different.
			 * @param operand A reference to another rectangle.
			 * @return bool
			 */
			[[nodiscard]]
			constexpr
			bool
			operator!= (const AARectangle< precision_t > & operand) const noexcept
			{
				return !(*this == operand);
			}

			/**
			 * @brief Returns whether the rectangle is a coherent surface.
			 * @note For floating point types, this also rejects non-finite (NaN/Inf) dimensions.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isValid () const noexcept
			{
				const auto rectangleWidth = this->width();
				const auto rectangleHeight = this->height();

				if ( rectangleWidth <= 0 || rectangleHeight <= 0 )
				{
					return false;
				}

				if constexpr ( std::is_floating_point_v< precision_t > )
				{
					if ( !std::isfinite(rectangleWidth) || !std::isfinite(rectangleHeight) )
					{
						return false;
					}
				}

				return true;
			}

			/**
			 * @brief Returns the point array.
			 * @note The layout is  [topLeft, bottomLeft, topRight, bottomRight].
			 * @return std::array< Point< precision_t >, 4 >
			 */
			[[nodiscard]]
			std::array< Point< precision_t >, 4 >
			points () const noexcept
			{
				return {
					this->topLeft(),
					this->bottomLeft(),
					this->topRight(),
					this->bottomRight()
				};
			}

			/**
			 * @brief Sets the left (X-) coordinate of the rectangle, keeping its width.
			 * @param value A X-axis coordinate.
			 * @return void
			 */
			void
			setLeft (precision_t value) noexcept
			{
				const auto currentWidth = this->width();

				m_minimum[X] = value;
				m_maximum[X] = value + currentWidth;
			}

			/**
			 * @brief Sets the right (X+) coordinate of the rectangle.
			 * @note This will modify the rectangle width.
			 * @warning The right coordinate must be greater than the left coordinate, otherwise the method will ignore the new value.
			 * @param value A X-axis coordinate.
			 * @return void
			 */
			void
			setRight (precision_t value) noexcept
			{
				if ( value > m_minimum[X] )
				{
					m_maximum[X] = value;
				}
			}

			/**
			 * @brief Sets the top (Y-) coordinate of the rectangle, keeping its height.
			 * @param value A Y-axis coordinate.
			 * @return void
			 */
			void
			setTop (precision_t value) noexcept
			{
				const auto currentHeight = this->height();

				m_minimum[Y] = value;
				m_maximum[Y] = value + currentHeight;
			}

			/**
			 * @brief Sets the bottom (Y+) coordinate of the rectangle.
			 * @note This will modify the rectangle height.
			 * @warning The bottom coordinate must be greater than the top coordinate, otherwise the method will ignore the new value.
			 * @param value A Y-axis coordinate.
			 * @return void
			 */
			void
			setBottom (precision_t value) noexcept
			{
				if ( value > m_minimum[Y] )
				{
					m_maximum[Y] = value;
				}
			}

			/**
			 * @brief Sets the top-left coordinate of the rectangle, keeping its dimensions.
			 * @param position A reference to a vector.
			 * @return void
			 */
			void
			setPosition (const Point< precision_t > & position) noexcept
			{
				const auto currentWidth = this->width();
				const auto currentHeight = this->height();

				m_minimum[X] = position.x();
				m_minimum[Y] = position.y();
				m_maximum[X] = position.x() + currentWidth;
				m_maximum[Y] = position.y() + currentHeight;
			}

			/**
			 * @brief Sets the rectangle from two opposite corners.
			 * @note The corners are sorted, so the argument order does not matter.
			 * @param firstCorner A reference to a corner point.
			 * @param secondCorner A reference to the opposite corner point.
			 * @return void
			 */
			void
			set (const Point< precision_t > & firstCorner, const Point< precision_t > & secondCorner) noexcept
			{
				m_minimum[X] = std::min(firstCorner.x(), secondCorner.x());
				m_minimum[Y] = std::min(firstCorner.y(), secondCorner.y());
				m_maximum[X] = std::max(firstCorner.x(), secondCorner.x());
				m_maximum[Y] = std::max(firstCorner.y(), secondCorner.y());
			}

			/**
			 * @brief Sets the width of the rectangle from the left coordinate.
			 * @warning The value must be positive, otherwise the method will ignore the new value.
			 * @param value An X-axis size.
			 * @return void
			 */
			void
			setWidth (precision_t value) noexcept
			{
				if ( value > 0 )
				{
					m_maximum[X] = m_minimum[X] + value;
				}
			}

			/**
			 * @brief Sets the height of the rectangle from the top coordinate.
			 * @warning The value must be positive, otherwise the method will ignore the new value.
			 * @param value A Y-axis size.
			 * @return void
			 */
			void
			setHeight (precision_t value) noexcept
			{
				if ( value > 0 )
				{
					m_maximum[Y] = m_minimum[Y] + value;
				}
			}

			/**
			 * @brief Moves the rectangle top-left coordinate by a distance in X and Y.
			 * @param x A distance on X axis.
			 * @param y A distance on Y axis.
			 * @return void
			 */
			void
			move (precision_t x, precision_t y) noexcept
			{
				m_minimum[X] += x;
				m_maximum[X] += x;
				m_minimum[Y] += y;
				m_maximum[Y] += y;
			}

			/**
			 * @brief Modifies the width with a value to add or remove.
			 * @note The result will always be positive or 0.
			 * @param value The width difference.
			 * @return void
			 */
			void
			modifyWidthBy (precision_t value) noexcept
			{
				auto newWidth = this->width() + value;

				if ( newWidth < 0 )
				{
					newWidth = 0;
				}

				m_maximum[X] = m_minimum[X] + newWidth;
			}

			/**
			 * @brief Modifies the height with a value to add or remove.
			 * @note The result will always be positive or 0.
			 * @param value The height difference.
			 * @return void
			 */
			void
			modifyHeightBy (precision_t value) noexcept
			{
				auto newHeight = this->height() + value;

				if ( newHeight < 0 )
				{
					newHeight = 0;
				}

				m_maximum[Y] = m_minimum[Y] + newHeight;
			}

			/**
			 * @brief Returns the highest positive XY coordinates of the rectangle.
			 * @return const Point< precision_t > &
			 */
			[[nodiscard]]
			const Point< precision_t > &
			maximum () const noexcept
			{
				return m_maximum;
			}

			/**
			 * @brief Returns the highest positive coordinates of the rectangle on one axis.
			 * @return precision_t
			 */
			[[nodiscard]]
			precision_t
			maximum (size_t index) const noexcept
			{
				return m_maximum[index];
			}

			/**
			 * @brief Returns the lowest negative XY coordinates of the rectangle.
			 * @return const Point< precision_t > &
			 */
			[[nodiscard]]
			const Point< precision_t > &
			minimum () const noexcept
			{
				return m_minimum;
			}

			/**
			 * @brief Returns the highest lowest negative coordinates of the rectangle on one axis.
			 * @return precision_t
			 */
			[[nodiscard]]
			precision_t
			minimum (size_t index) const noexcept
			{
				return m_minimum[index];
			}

			/**
			 * @brief Returns the left coordinate (X-) of the rectangle.
			 * @return precision_t
			 */
			[[nodiscard]]
			constexpr
			precision_t
			left () const noexcept
			{
				return m_minimum[X];
			}

			/**
			 * @brief Returns the right coordinate (X+) of the rectangle.
			 * @return precision_t
			 */
			[[nodiscard]]
			constexpr
			precision_t
			right () const noexcept
			{
				return m_maximum[X];
			}

			/**
			 * @brief Returns the top coordinate (Y-) of the rectangle.
			 * @return precision_t
			 */
			[[nodiscard]]
			constexpr
			precision_t
			top () const noexcept
			{
				return m_minimum[Y];
			}

			/**
			 * @brief Returns the bottom coordinate (Y+) of the rectangle.
			 * @return precision_t
			 */
			[[nodiscard]]
			constexpr
			precision_t
			bottom () const noexcept
			{
				return m_maximum[Y];
			}

			/**
			 * @brief Returns the width of the rectangle.
			 * @return precision_t
			 */
			[[nodiscard]]
			constexpr
			precision_t
			width () const noexcept
			{
				return m_maximum[X] > m_minimum[X] ? m_maximum[X] - m_minimum[X] : static_cast< precision_t >(0);
			}

			/**
			 * @brief Returns the height of the rectangle.
			 * @return precision_t
			 */
			[[nodiscard]]
			constexpr
			precision_t
			height () const noexcept
			{
				return m_maximum[Y] > m_minimum[Y] ? m_maximum[Y] - m_minimum[Y] : static_cast< precision_t >(0);
			}

			/**
			 * @brief Returns the rectangle dimensions as a vector (width, height).
			 * @return Point< precision_t >
			 */
			[[nodiscard]]
			constexpr
			Point< precision_t >
			size () const noexcept
			{
				return {this->width(), this->height()};
			}

			/**
			 * @brief Returns the highest length between the width and the height.
			 * @return precision_t
			 */
			[[nodiscard]]
			constexpr
			precision_t
			highestLength () const noexcept
			{
				const auto rectangleWidth = this->width();
				const auto rectangleHeight = this->height();

				return rectangleWidth > rectangleHeight ? rectangleWidth : rectangleHeight;
			}

			/**
			 * @brief Returns the top-left coordinate of the rectangle as a vector.
			 * @return Point< precision_t >
			 */
			[[nodiscard]]
			constexpr
			Point< precision_t >
			topLeft () const noexcept
			{
				return {this->left(), this->top()};
			}

			/**
			 * @brief Returns the bottom-left coordinate of the rectangle as a vector.
			 * @return Point< precision_t >
			 */
			[[nodiscard]]
			constexpr
			Point< precision_t >
			bottomLeft () const noexcept
			{
				return {this->left(), this->bottom()};
			}

			/**
			 * @brief Returns the top-right coordinate of the rectangle as a vector.
			 * @return Point< precision_t >
			 */
			[[nodiscard]]
			constexpr
			Point< precision_t >
			topRight () const noexcept
			{
				return {this->right(), this->top()};
			}

			/**
			 * @brief Returns the bottom-right coordinate of the rectangle as a vector.
			 * @return Point< precision_t >
			 */
			[[nodiscard]]
			constexpr
			Point< precision_t >
			bottomRight () const noexcept
			{
				return {this->right(), this->bottom()};
			}

			/**
			 * @brief Returns the center of the rectangle.
			 * @return Point< precision_t >
			 */
			[[nodiscard]]
			constexpr
			Point< precision_t >
			centroid () const noexcept requires (std::is_floating_point_v< precision_t >)
			{
				return (m_minimum + m_maximum) * static_cast< precision_t >(0.5);
			}

			/**
			 * @brief Returns the farthest corner distance from the origin (0, 0).
			 * @return precision_t
			 */
			[[nodiscard]]
			precision_t
			farthestPoint () const noexcept requires (std::is_floating_point_v< precision_t >)
			{
				precision_t distance = 0;

				if ( m_maximum[X] > distance )
				{
					distance = m_maximum[X];
				}

				if ( m_maximum[Y] > distance )
				{
					distance = m_maximum[Y];
				}

				if ( std::abs(m_minimum[X]) > distance )
				{
					distance = std::abs(m_minimum[X]);
				}

				if ( std::abs(m_minimum[Y]) > distance )
				{
					distance = std::abs(m_minimum[Y]);
				}

				return distance;
			}

			/**
			 * @brief Returns the center of the rectangle (integer specialization).
			 * @note Integer midpoint computed with integer division, so it is truncated toward zero.
			 * This avoids the silent collapse to (0, 0) that a floating multiply by 0.5 would produce
			 * when cast back to an integer precision. The floating-point overload above keeps the exact
			 * (min + max) * 0.5 behaviour.
			 * @return Point< precision_t >
			 */
			[[nodiscard]]
			constexpr
			Point< precision_t >
			centroid () const noexcept requires (std::is_integral_v< precision_t >)
			{
				return {
					static_cast< precision_t >((m_minimum[X] + m_maximum[X]) / 2),
					static_cast< precision_t >((m_minimum[Y] + m_maximum[Y]) / 2)
				};
			}

			/**
			 * @brief Resets the rectangle to an empty (inverted) state, ready for point accumulation.
			 * @note Mirrors EmEn::Base::Math::Space3D::AACuboid::reset(). After this call, isValid() returns
			 * false and the first merge() with a point or a valid rectangle initializes the bounds. This is
			 * the idiomatic way to build a tight bounding rectangle from a set of points.
			 * @return void
			 */
			void
			reset () noexcept
			{
				m_minimum[X] = std::numeric_limits< precision_t >::max();
				m_minimum[Y] = std::numeric_limits< precision_t >::max();
				m_maximum[X] = std::numeric_limits< precision_t >::lowest();
				m_maximum[Y] = std::numeric_limits< precision_t >::lowest();
			}

			/**
			 * @brief Returns whether a point lies inside the rectangle (edges included).
			 * @param point A reference to a point.
			 * @return bool
			 */
			[[nodiscard]]
			constexpr
			bool
			contains (const Point< precision_t > & point) const noexcept
			{
				return
					point.x() >= m_minimum[X] && point.x() <= m_maximum[X] &&
					point.y() >= m_minimum[Y] && point.y() <= m_maximum[Y];
			}

			/**
			 * @brief Extends the surface of this rectangle to enclose another one.
			 * @param rectangle A reference to another rectangle.
			 * @return void
			 */
			void
			merge (const AARectangle< precision_t > & rectangle) noexcept
			{
				if ( this == &rectangle || !rectangle.isValid() )
				{
					return;
				}

				if ( !this->isValid() )
				{
					*this = rectangle;

					return;
				}

				m_minimum[X] = std::min(m_minimum[X], rectangle.m_minimum[X]);
				m_minimum[Y] = std::min(m_minimum[Y], rectangle.m_minimum[Y]);
				m_maximum[X] = std::max(m_maximum[X], rectangle.m_maximum[X]);
				m_maximum[Y] = std::max(m_maximum[Y], rectangle.m_maximum[Y]);
			}

			/**
			 * @brief Extends the surface to enclose a point.
			 * @note To build a bounding rectangle from scratch, call reset() first then merge() each point.
			 * @param point A reference to a point.
			 * @return void
			 */
			void
			merge (const Point< precision_t > & point) noexcept
			{
				this->mergeX(point.x());
				this->mergeY(point.y());
			}

			/**
			 * @brief Extends the surface to enclose a value on the X axis.
			 * @param value The coordinate on X.
			 * @return void
			 */
			void
			mergeX (precision_t value) noexcept
			{
				if ( value < m_minimum[X] )
				{
					m_minimum[X] = value;
				}

				if ( value > m_maximum[X] )
				{
					m_maximum[X] = value;
				}
			}

			/**
			 * @brief Extends the surface to enclose a value on the Y axis.
			 * @param value The coordinate on Y.
			 * @return void
			 */
			void
			mergeY (precision_t value) noexcept
			{
				if ( value < m_minimum[Y] )
				{
					m_minimum[Y] = value;
				}

				if ( value > m_maximum[Y] )
				{
					m_maximum[Y] = value;
				}
			}

			/**
			 * @brief Returns a new rectangle enclosing this one and another (non-mutating merge).
			 * @param rectangle A reference to another rectangle.
			 * @return AARectangle< precision_t >
			 */
			[[nodiscard]]
			AARectangle< precision_t >
			merged (const AARectangle< precision_t > & rectangle) const noexcept
			{
				AARectangle< precision_t > result{*this};

				result.merge(rectangle);

				return result;
			}

			/**
			 * @brief Reduces the sizes of the rectangle.
			 * @param bounds A reference to a rectangle.
			 * @return bool
			 */
			bool
			cropOnOverflow (const AARectangle< precision_t > & bounds) noexcept
			{
				if ( !this->isValid() || !bounds.isValid() )
				{
					return false;
				}

				const precision_t newLeft = std::max(bounds.left(), this->left());
				const precision_t newTop = std::max(bounds.top(), this->top());
				precision_t newRight = std::min(bounds.right(), this->right());
				precision_t newBottom = std::min(bounds.bottom(), this->bottom());

				if ( newRight < newLeft )
				{
					newRight = newLeft;
				}

				if ( newBottom < newTop )
				{
					newBottom = newTop;
				}

				const bool changed =
					m_minimum[X] != newLeft ||
					m_minimum[Y] != newTop ||
					m_maximum[X] != newRight ||
					m_maximum[Y] != newBottom;

				if ( changed )
				{
					m_minimum[X] = newLeft;
					m_minimum[Y] = newTop;
					m_maximum[X] = newRight;
					m_maximum[Y] = newBottom;
				}

				return changed;
			}

			/**
			 * @brief Reduces the sizes of the rectangle.
			 * @param width The final width.
			 * @param height The final height.
			 * @return bool
			 */
			bool
			cropOnOverflow (precision_t width, precision_t height) noexcept
			{
				return this->cropOnOverflow({0, 0, width, height});
			}

			/**
			 * @brief Returns the aspect ratio of the surface.
			 * @warning This function will return 0 for an invalid surface.
			 * @return precision_t
			 */
			[[nodiscard]]
			precision_t
			aspectRatio () const noexcept
			{
				if ( !this->isValid() )
				{
					return 0;
				}

				return this->width() / this->height();
			}

			/**
			 * @brief Reduces this rectangle to its intersection with another one.
			 * @note If no intersection occurs, the rectangle will not change.
			 * @param rectangle A reference to another rectangle.
			 * @return bool
			 */
			bool
			intersect (const AARectangle< precision_t > & rectangle) noexcept
			{
				if ( !this->isValid() || !rectangle.isValid() || this->isOutside(rectangle) )
				{
					return false;
				}

				const precision_t interLeft = std::max(this->left(), rectangle.left());
				const precision_t interTop = std::max(this->top(), rectangle.top());
				const precision_t interRight = std::min(this->right(), rectangle.right());
				const precision_t interBottom = std::min(this->bottom(), rectangle.bottom());

				const precision_t interWidth = interRight - interLeft;
				const precision_t interHeight = interBottom - interTop;

				if ( interWidth > 0 && interHeight > 0 )
				{
					m_minimum[X] = interLeft;
					m_minimum[Y] = interTop;
					m_maximum[X] = interRight;
					m_maximum[Y] = interBottom;

					return true;
				}

				return false;
			}

			/**
			 * @brief Returns the intersection of this rectangle with another one (non-mutating).
			 * @note If no intersection occurs, an invalid (empty) rectangle is returned.
			 * @param rectangle A reference to another rectangle.
			 * @return AARectangle< precision_t >
			 */
			[[nodiscard]]
			AARectangle< precision_t >
			intersection (const AARectangle< precision_t > & rectangle) const noexcept
			{
				if ( !this->isValid() || !rectangle.isValid() || this->isOutside(rectangle) )
				{
					return {};
				}

				const precision_t interLeft = std::max(this->left(), rectangle.left());
				const precision_t interTop = std::max(this->top(), rectangle.top());
				const precision_t interRight = std::min(this->right(), rectangle.right());
				const precision_t interBottom = std::min(this->bottom(), rectangle.bottom());

				if ( interRight > interLeft && interBottom > interTop )
				{
					AARectangle< precision_t > result;

					result.set({interLeft, interTop}, {interRight, interBottom});

					return result;
				}

				return {};
			}

			/**
			 * @brief Returns whether the rectangle does not intersect another one.
			 * @param rectangle A reference to another rectangle.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isOutside (const AARectangle< precision_t > & rectangle) const noexcept
			{
				return
					this->left() > rectangle.right() ||
					rectangle.left() > this->right() ||
					this->top() > rectangle.bottom() ||
					rectangle.top() > this->bottom();
			}

			/**
			 * @brief Returns whether the rectangle does not intersect with a rectangle where coordinates are 0,0.
			 * @param width The width of the rectangle. (x = 0)
			 * @param height The height of the rectangle. (y = 0)
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isOutside (precision_t width, precision_t height) const noexcept
			{
				return
					this->left() >= width ||
					this->right() <= 0 ||
					this->top() >= height ||
					this->bottom() <= 0;
			}

			/**
			 * @brief Returns whether the rectangle intersects another one.
			 * @param rectangle A reference to a rectangle.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isInside (const AARectangle< precision_t > & rectangle) const noexcept
			{
				if ( this->left() < rectangle.left() )
				{
					return false;
				}

				if ( this->right() > rectangle.right() )
				{
					return false;
				}

				if ( this->top() < rectangle.top() )
				{
					return false;
				}

				if ( this->bottom() > rectangle.bottom() )
				{
					return false;
				}

				return true;
			}

			/**
			 * @brief Returns whether the rectangle intersects with a rectangle where coordinates are 0,0.
			 * @param width The width of the rectangle. (x = 0)
			 * @param height The height of the rectangle. (y = 0)
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isInside (precision_t width, precision_t height) const noexcept
			{
				return this->left() >= 0 && this->right() <= width && this->top() >= 0 && this->bottom() <= height;
			}

			/**
			 * @brief Returns whether the rectangle is intersecting a rectangle.
			 * @param rectangle A reference to a rectangle.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isIntersect (const AARectangle< precision_t > & rectangle) const noexcept
			{
				return !this->isOutside(rectangle);
			}

			/**
			 * @brief Returns whether the rectangle is intersecting a rectangle.
			 * @param width The width of the rectangle. (x = 0)
			 * @param height The height of the rectangle. (y = 0)
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isIntersect (precision_t width, precision_t height) const noexcept
			{
				return !(this->left() >= width  || this->right() <= 0 || this->top() >= height || this->bottom() <= 0);
			}

			/**
			 * @brief Returns the rectangle perimeter.
			 * @return precision_t
			 */
			[[nodiscard]]
			constexpr
			precision_t
			getPerimeter () const noexcept
			{
				return (this->width() + this->height()) * 2;
			}

			/**
			 * @brief Returns the rectangle area.
			 * @return precision_t
			 */
			[[nodiscard]]
			constexpr
			precision_t
			getArea () const noexcept
			{
				return this->width() * this->height();
			}

			/**
			 * @brief STL streams printable object.
			 * @param out A reference to the stream output.
			 * @param obj A reference to the object to print.
			 * @return std::ostream &
			 */
			friend
			std::ostream &
			operator<< (std::ostream & out, const AARectangle & obj) noexcept
			{
				out <<
					"Axis-Aligned rectangle data :" "\n"
					"Position (top-left) : X " << obj.left() << ", Y " << obj.top() << "\n"
					"Position (bottom-right) : X " << obj.right() << ", Y " << obj.bottom() << "\n"
					"Dimensions : " << obj.width() << " X " << obj.height() << '\n';

				return out;
			}

			/**
			 * @brief Stringifies the object.
			 * @param obj A reference to the object to print.
			 * @return std::string
			 */
			friend
			std::string
			to_string (const AARectangle & obj) noexcept
			{
				std::stringstream output;

				output << obj;

				return output.str();
			}

		private:

			Point< precision_t > m_minimum{std::numeric_limits< precision_t >::max(), std::numeric_limits< precision_t >::max()};
			Point< precision_t > m_maximum{std::numeric_limits< precision_t >::lowest(), std::numeric_limits< precision_t >::lowest()};
	};
}
