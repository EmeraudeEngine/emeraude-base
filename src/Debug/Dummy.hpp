/*
 * src/Debug/Dummy.hpp
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
#include <iostream>
#include <sstream>
#include <utility>

/* Local inclusions. */
#include "Logging/Logging.hpp"

namespace EmEn::Base::Debug
{
	/**
	 * @brief Dummy class to identify special method in use.
	 */
	class Dummy final
	{
		public:

			inline static int s_instanceCount = 0;

			/**
			 * @brief Default constructor.
			 */
			Dummy () noexcept
			{
				s_instanceCount++;

				Logging::debug("Debug::Dummy", (std::stringstream{} << "Default constructor called! " << *this).str());
			}

			/**
			 * @brief Parametric constructor.
			 * @param value A value.
			 */
			explicit
			Dummy (int value) noexcept
				: m_value{value}
			{
				s_instanceCount++;

				Logging::debug("Debug::Dummy", (std::stringstream{} << "Parametric constructor called! " << *this).str());
			}

			/**
			 * @brief Copy constructor.
			 * @param copy A reference to the copied instance.
			 */
			Dummy (const Dummy & copy) noexcept
				: m_value{copy.m_value}
			{
				s_instanceCount++;

				Logging::debug("Debug::Dummy", (std::stringstream{} << "Copy constructor called! " << *this).str());
			}

			/**
			 * @brief Move constructor.
			 * @param other A reference to the copied instance.
			 */
			Dummy (Dummy && other) noexcept
				: m_value{std::exchange(other.m_value, -1)}
			{
				s_instanceCount++;

				Logging::debug("Debug::Dummy", (std::stringstream{} << "Move constructor called! " << *this).str());
			}

			/**
			 * @brief Assignment operator.
			 * @param other A reference to the copied instance.
			 * @return Dummy &
			 */
			Dummy &
			operator= (const Dummy & other) noexcept
			{
				Logging::debug("Debug::Dummy", (std::stringstream{} << "Copy assignment called (from old value : " << other.value() << ") ! " << *this).str());

				if ( this != &other )
				{
					m_value = other.m_value;
				}

				return *this;
			}

			/**
			 * @brief Move operator.
			 * @param other A reference to the copied instance.
			 * @return Dummy &
			 */
			Dummy &
			operator= (Dummy && other) noexcept
			{
				const int oldValueFrom = other.m_value;
				const int oldValueTo = m_value;

				if ( this != &other )
				{
					m_value = std::exchange(other.m_value, -1);
				}

				Logging::debug("Debug::Dummy", (std::stringstream{} << "Move assignment (from: " << oldValueFrom << " to: " << oldValueTo << ")").str());

				return *this;
			}

			/**
			 * @brief Destructor.
			 */
			~Dummy ()
			{
				s_instanceCount--;

				Logging::debug("Debug::Dummy", (std::stringstream{} << "Destructor called! " << *this).str());
			}

			/**
			 * @brief C++ spaceship operator.
			 * @param other A reference to another dummy.
			 * @return std::strong_ordering
			 */
			auto operator<=> (const Dummy & other) const = default;

			/**
			 * @brief Sets a value.
			 * @param value The value.
			 */
			void
			value (int value)
			{
				m_value = value;
			}

			/**
			 * @brief Returns the value.
			 * @return int
			 */
			[[nodiscard]]
			int
			value () const
			{
				return m_value;
			}

		private:

			/**
			 * @brief STL streams printable object.
			 * @param out A reference to the stream output.
			 * @param obj A reference to the object to print.
			 * @return std::ostream &
			 */
			friend std::ostream & operator<<(std::ostream & out, const Dummy & obj);

			int m_value{-1};
	};

	inline
	std::ostream &
	operator<< (std::ostream & out, const Dummy & obj)
	{
		return out << "This dummy value: " << obj.m_value << " (instance count: " << Dummy::s_instanceCount << ')';
	}
}
