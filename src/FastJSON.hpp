/*
 * src/FastJSON.hpp
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
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

/* Third-party inclusions. */
#ifndef JSON_USE_EXCEPTION
#define JSON_USE_EXCEPTION 0
#endif

#include "json/json.h"

/* Local inclusions. */
#include "Math/Matrix.hpp"
#include "PixelFactory/Color.hpp"

namespace EmEn::Base::FastJSON
{
	/* NOTE: Common JSON key. */
	constexpr auto TypeKey{"Type"};
	constexpr auto NameKey{"Name"};
	constexpr auto PositionKey{"Position"};
	constexpr auto OrientationKey{"Orientation"};
	constexpr auto ColorKey{"Color"};
	constexpr auto IntensityKey{"Intensity"};
	constexpr auto DataKey{"Data"};
	constexpr auto PropertiesKey{"Properties"};
	constexpr auto ScaleKey{"Scale"};
	constexpr auto SizeKey{"Size"};
	constexpr auto DivisionKey{"Division"};
	constexpr auto UVMultiplierKey{"UVMultiplier"};
	constexpr auto ModeKey{"Mode"};
	
	/**
	 * @brief Gets the root JSON node from a filepath.
	 * @param filepath A reference to a filesystem path.
	 * @param stackLimit The depth of JSON parsing. Default 16.
	 * @param quiet Do not print console message. Default false.
	 * @return std::optional< Json::Value >
	 */
	[[nodiscard]]
	std::optional< Json::Value > getRootFromFile (const std::filesystem::path & filepath, int stackLimit = 16, bool quiet = false);

	/**
	 * @brief Gets the root JSON node from a string.
	 * @param json A reference to a string.
	 * @param stackLimit The depth of JSON parsing. Default 16.
	 * @param quiet Do not print console message. Default false.
	 * @return std::optional< Json::Value >
	 */
	[[nodiscard]]
	std::optional< Json::Value > getRootFromString (const std::string & json, int stackLimit = 16, bool quiet = false);

	/**
	 * @brief Creates a compact standard string from a JSON node.
	 * @param root A reference to a JSON value.
	 * @return std::string
	 */
	[[nodiscard]]
	std::string stringify (const Json::Value & root);

	/**
	 * @brief Gets a JSON array from a JSON node.
	 * @param parentNode A reference to a JSON node.
	 * @param key The JSON key name.
	 * @return std::optional< Json::Value >
	 */
	[[nodiscard]]
	inline
	std::optional< Json::Value >
	getArray (const Json::Value & parentNode, const char * key) noexcept
	{
		if ( !parentNode.isObject() || !parentNode.isMember(key) )
		{
			if constexpr ( IsDebug )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is missing !" << std::endl;
			}

			return std::nullopt;
		}

		const auto & node = parentNode[key];

		if ( !node.isArray() )
		{
			if constexpr ( IsDebug )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is not an array !" << std::endl;
			}

			return std::nullopt;
		}

		return node;
	}

	/**
	 * @brief Gets a JSON object from a JSON node.
	 * @param parentNode A reference to a JSON node.
	 * @param key The JSON key name.
	 * @return std::optional< Json::Value >
	 */
	[[nodiscard]]
	inline
	std::optional< Json::Value >
	getObject (const Json::Value & parentNode, const char * key) noexcept
	{
		if ( !parentNode.isObject() || !parentNode.isMember(key) )
		{
			if constexpr ( IsDebug )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is missing !" << std::endl;
			}

			return std::nullopt;
		}

		const auto & node = parentNode[key];

		if ( !node.isObject() )
		{
			if constexpr ( IsDebug )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is not an object !" << std::endl;
			}

			return std::nullopt;
		}

		return node;
	}

	/**
	 * @brief Converts ONE JSON value to a number, a boolean or a string: the checked counterpart of jsoncpp's as*()
	 * accessors, which ABORT on a value of the wrong type or out of the target's range (the library throws
	 * Json::LogicError, std::terminate under -fno-exceptions).
	 * @note A boolean is read from a JSON boolean only, a string from a JSON string only, a number from a JSON number
	 * only (a boolean is not a number). An integral target refuses a value outside its range; a fractional value
	 * truncates toward zero. A floating-point target refuses a non-finite value (the parser accepts NaN and Infinity,
	 * and 1e999 reads as infinity) and, below double, a finite value beyond its range.
	 * @tparam value_t The type to read: an arithmetic type or std::string.
	 * @param node A reference to a JSON value.
	 * @return std::optional< value_t > Empty when the value does not convert.
	 */
	template< typename value_t >
	[[nodiscard]]
	std::optional< value_t >
	asValue (const Json::Value & node) noexcept requires (std::is_arithmetic_v< value_t > || std::is_same_v< value_t, std::string >)
	{
		if constexpr ( std::is_same_v< value_t, std::string > )
		{
			if ( !node.isString() )
			{
				return std::nullopt;
			}

			return node.asString();
		}
		else if constexpr ( std::is_same_v< value_t, bool > )
		{
			if ( !node.isBool() )
			{
				return std::nullopt;
			}

			return node.asBool();
		}
		else if constexpr ( std::is_integral_v< value_t > )
		{
			using Limits = std::numeric_limits< value_t >;

			if ( !node.isNumeric() )
			{
				return std::nullopt;
			}

			/* NOTE: A number holding an integer (an int, a uint, or a real such as 64.0) goes through jsoncpp's own
			 * range predicate before the 64-bit read, then against the target's limits. */
			if ( node.isIntegral() )
			{
				if constexpr ( std::is_signed_v< value_t > )
				{
					if ( !node.isInt64() )
					{
						return std::nullopt;
					}

					const auto value = node.asInt64();

					if constexpr ( sizeof(value_t) < sizeof(int64_t) )
					{
						if ( value < Limits::min() || value > Limits::max() )
						{
							return std::nullopt;
						}
					}

					return static_cast< value_t >(value);
				}
				else
				{
					if ( !node.isUInt64() )
					{
						return std::nullopt;
					}

					const auto value = node.asUInt64();

					if constexpr ( sizeof(value_t) < sizeof(uint64_t) )
					{
						if ( value > Limits::max() )
						{
							return std::nullopt;
						}
					}

					return static_cast< value_t >(value);
				}
			}

			/* A fractional number truncates toward zero, when the truncated value fits. */
			const auto value = node.asDouble();

			if ( !std::isfinite(value) || value <= static_cast< double >(Limits::min()) - 1.0 || value >= static_cast< double >(Limits::max()) + 1.0 )
			{
				return std::nullopt;
			}

			return static_cast< value_t >(value);
		}
		else
		{
			if ( !node.isNumeric() )
			{
				return std::nullopt;
			}

			const auto value = node.asDouble();

			if ( !std::isfinite(value) )
			{
				return std::nullopt;
			}

			if constexpr ( sizeof(value_t) < sizeof(double) )
			{
				if ( std::abs(value) > static_cast< double >(std::numeric_limits< value_t >::max()) )
				{
					return std::nullopt;
				}
			}

			return static_cast< value_t >(value);
		}
	}

	/**
	 * @brief Returns a number or a boolean from a JSON node.
	 * @note The checks of asValue() apply: a wrong type, an out-of-range integer or a non-finite float is absent.
	 * @tparam value_t The type of the number.
	 * @param parentNode A reference to a JSON node.
	 * @param key The JSON key name to look for.
	 * @return std::optional< value_t >
	 */
	template< typename value_t >
	[[nodiscard]]
	std::optional< value_t >
	getValue (const Json::Value & parentNode, const char * key) noexcept requires (std::is_arithmetic_v< value_t >)
	{
		if ( !parentNode.isObject() || !parentNode.isMember(key) )
		{
			if constexpr ( IsDebug )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is missing !" << std::endl;
			}

			return std::nullopt;
		}

		const auto value = asValue< value_t >(parentNode[key]);

		if constexpr ( IsDebug )
		{
			if ( !value.has_value() )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is not convertible to the requested type (wrong type, out of range or non-finite) !" "\n";
			}
		}

		return value;
	}

	/**
	 * @brief Returns a string from a JSON node.
	 * @tparam value_t The type of the variable. For overloading, this must be 'std::string'.
	 * @param parentNode A reference to a JSON node.
	 * @param key The JSON key name to look for.
	 * @return std::optional< value_t >
	 */
	template< typename value_t >
	[[nodiscard]]
	std::optional< value_t >
	getValue (const Json::Value & parentNode, const char * key) requires (std::is_same_v< value_t, std::string >)
	{
		if ( !parentNode.isObject() || !parentNode.isMember(key) )
		{
			if constexpr ( IsDebug )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is missing !" << std::endl;
			}

			return std::nullopt;
		}

		auto value = asValue< std::string >(parentNode[key]);

		if constexpr ( IsDebug )
		{
			if ( !value.has_value() )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is not a string !" "\n";
			}
		}

		return value;
	}

	/**
	 * @brief Builds an object of several numbers (a vector, a matrix, a color) from the first items of a JSON array.
	 * @tparam object_t The type of object to instantiate.
	 * @tparam precision_t The type of one item.
	 * @tparam Is The item indexes.
	 * @param node A reference to a JSON array holding at least sizeof...(Is) items.
	 * @return std::optional< object_t > Empty when one item is not a finite number (asValue()).
	 */
	template< typename object_t, typename precision_t, std::size_t... Is >
	[[nodiscard]]
	std::optional< object_t >
	createFromJsonImpl (const Json::Value & node, std::index_sequence< Is... > /*indexes*/) noexcept
	{
		std::array< precision_t, sizeof...(Is) > values{};
		Json::Value::ArrayIndex index = 0;

		for ( auto & slot : values )
		{
			const auto value = asValue< precision_t >(node[index++]);

			if ( !value.has_value() )
			{
				return std::nullopt;
			}

			slot = *value;
		}

		return object_t{values[Is]...};
	}

	/**
	 * @brief Returns a vector from a JSON node.
	 * @tparam value_t The type of vector.
	 * @param parentNode A reference to a JSON node.
	 * @param key The JSON key name to look for.
	 * @return std::optional< value_t >
	 */
	template< Math::VectorConcept value_t >
	[[nodiscard]]
	std::optional< value_t >
	getValue (const Json::Value & parentNode, const char * key) noexcept
	{
		using Traits = Math::VectorTraits< value_t >;

		if ( !parentNode.isObject() || !parentNode.isMember(key) )
		{
			if constexpr ( IsDebug )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is missing !" << std::endl;
			}

			return std::nullopt;
		}

		const auto & node = parentNode[key];

		if ( !node.isArray() || node.size() < Traits::dim )
		{
			if constexpr ( IsDebug )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is not an array of " << Traits::dim << " items !" << std::endl;
			}

			return std::nullopt;
		}

		return createFromJsonImpl< value_t, typename Traits::precision >(node, std::make_index_sequence< Traits::dim >());
	}

	/**
	 * @brief Returns a matrix from a JSON node.
	 * @tparam value_t The type of vector.
	 * @param parentNode A reference to a JSON node.
	 * @param key The JSON key name to look for.
	 * @return std::optional< value_t >
	 */
	template< Math::MatrixConcept value_t >
	[[nodiscard]]
	std::optional< value_t >
	getValue (const Json::Value & parentNode, const char * key) noexcept
	{
		using Traits = Math::MatrixTraits< value_t >;

		constexpr size_t element_count = Traits::dim * Traits::dim;

		if ( !parentNode.isObject() || !parentNode.isMember(key) )
		{
			if constexpr ( IsDebug )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is missing !" << std::endl;
			}

			return std::nullopt;
		}

		const auto & node = parentNode[key];

		if ( !node.isArray() || node.size() < element_count )
		{
			if constexpr ( IsDebug )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is not an array of " << element_count << " items !" << std::endl;
			}

			return std::nullopt;
		}

		return createFromJsonImpl< value_t, typename Traits::precision >(node, std::make_index_sequence<element_count>());
	}

	/**
	 * @brief Returns a color from a JSON node.
	 * @tparam value_t The type of vector.
	 * @param parentNode A reference to a JSON node.
	 * @param key The JSON key name to look for.
	 * @return std::optional< value_t >
	 */
	template< PixelFactory::ColorConcept value_t >
	[[nodiscard]]
	std::optional< value_t >
	getValue (const Json::Value & parentNode, const char * key) noexcept
	{
		using precision_t = typename PixelFactory::ColorTraits< value_t >::precision;

		if ( !parentNode.isObject() || !parentNode.isMember(key) )
		{
			if constexpr ( IsDebug )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is missing !" << std::endl;
			}

			return std::nullopt;
		}

		const auto & node = parentNode[key];

		if ( !node.isArray() )
		{
			if constexpr ( IsDebug )
			{
				std::cerr << "[FastJSON-DEBUG] Key '" << key << "' is not an array !" << std::endl;
			}

			return std::nullopt;
		}

		switch ( node.size() )
		{
			case 3:
				return createFromJsonImpl< value_t, precision_t >(node, std::make_index_sequence< 3 >());

			case 4:
				return createFromJsonImpl< value_t, precision_t >(node, std::make_index_sequence< 4 >());

			default:
				if constexpr ( IsDebug )
				{
					std::cerr << "[FastJSON-DEBUG] Key '" << key << "' cannot be converted to a color !" << std::endl;
				}

				return std::nullopt;
		}
	}

	/**
	 * @brief Gets a string from a JSON node using a list of valid terms.
	 * @param data A reference to a v value.
	 * @param key The JSON key name.
	 * @param possibleValues An array of allowed string values.
	 * @return std::optional< std::string >, The string value if it's found and valid, otherwise std::nullopt.
	 */
	template< size_t dim_t >
	[[nodiscard]]
	std::optional< std::string >
	getValidatedStringValue (const Json::Value & data, std::string_view key, const std::array< std::string_view, dim_t > & possibleValues) requires (dim_t > 0)
	{
		if ( const std::string keyString{key}; data.isObject() && data.isMember(keyString) )
		{
			if ( auto foundValue = asValue< std::string >(data[keyString]); foundValue.has_value() )
			{
				if ( std::ranges::any_of(possibleValues, [&foundValue] (std::string_view value) {return value == *foundValue;}) )
				{
					return foundValue;
				}
			}
		}

		return std::nullopt;
	}
}
