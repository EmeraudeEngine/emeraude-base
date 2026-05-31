/*
 * src/Variant.cpp
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

#include "Variant.hpp"

/* STL inclusions. */
#include <ostream>
#include <string>
#include <type_traits>

/* Local inclusions. */
#include "Logging/Logging.hpp"

namespace EmEn::Base
{
	using namespace Math;
	using namespace PixelFactory;

	template< typename T >
	T
	Variant::as (const char * requested) const noexcept
	{
		if ( const auto * p = std::get_if< T >(&m_data) )
		{
			return *p;
		}

		Logging::error("Variant", std::string{"requested '"} + requested + "' but the variant holds '" + to_cstring(type()) + "'.");

		return T{};
	}

	int8_t
	Variant::asInteger8 () const noexcept
	{
		return this->as< int8_t >(Integer8String);
	}

	uint8_t
	Variant::asUnsignedInteger8 () const noexcept
	{
		return this->as< uint8_t >(UnsignedInteger8String);
	}

	int16_t
	Variant::asInteger16 () const noexcept
	{
		return this->as< int16_t >(Integer16String);
	}

	uint16_t
	Variant::asUnsignedInteger16 () const noexcept
	{
		return this->as< uint16_t >(UnsignedInteger16String);
	}

	int32_t
	Variant::asInteger32 () const noexcept
	{
		return this->as< int32_t >(Integer32String);
	}

	uint32_t
	Variant::asUnsignedInteger32 () const noexcept
	{
		return this->as< uint32_t >(UnsignedInteger32String);
	}

	int64_t
	Variant::asInteger64 () const noexcept
	{
		return this->as< int64_t >(Integer64String);
	}

	uint64_t
	Variant::asUnsignedInteger64 () const noexcept
	{
		return this->as< uint64_t >(UnsignedInteger64String);
	}

	float
	Variant::asFloat () const noexcept
	{
		return this->as< float >(FloatString);
	}

	double
	Variant::asDouble () const noexcept
	{
		return this->as< double >(DoubleString);
	}

	long double
	Variant::asLongDouble () const noexcept
	{
		return this->as< long double >(LongDoubleString);
	}

	bool
	Variant::asBool () const noexcept
	{
		return this->as< bool >(BooleanString);
	}

	Vector2F
	Variant::asVector2Float () const noexcept
	{
		return this->as< Vector2F >(Vector2FloatString);
	}

	Vector3F
	Variant::asVector3Float () const noexcept
	{
		return this->as< Vector3F >(Vector3FloatString);
	}

	Vector4F
	Variant::asVector4Float () const noexcept
	{
		return this->as< Vector4F >(Vector4FloatString);
	}

	Matrix2F
	Variant::asMatrix2Float () const noexcept
	{
		return this->as< Matrix2F >(Matrix2FloatString);
	}

	Matrix3F
	Variant::asMatrix3Float () const noexcept
	{
		return this->as< Matrix3F >(Matrix3FloatString);
	}

	Matrix4F
	Variant::asMatrix4Float () const noexcept
	{
		return this->as< Matrix4F >(Matrix4FloatString);
	}

	CartesianFrameF
	Variant::asCartesianFrameFloat () const noexcept
	{
		return this->as< CartesianFrameF >(CartesianFrameString);
	}

	ColorF
	Variant::asColor () const noexcept
	{
		return this->as< ColorF >(ColorString);
	}

	std::ostream &
	operator<< (std::ostream & out, const Variant & variant) noexcept
	{
		std::visit([&out] < typename value_t >(const value_t  & value) {
			using T = std::decay_t< value_t  >;

			if constexpr ( std::is_same_v< T, std::monostate > )
			{
				out << "Null";
			}
			else
			{
				out << value;
			}
		}, variant.m_data);

		return out;
	}

	const char *
	Variant::to_cstring (Type type) noexcept
	{
		switch ( type )
		{
			case Type::Integer8 :
				return Integer8String;

			case Type::UnsignedInteger8 :
				return UnsignedInteger8String;

			case Type::Integer16 :
				return Integer16String;

			case Type::UnsignedInteger16 :
				return UnsignedInteger16String;

			case Type::Integer32 :
				return Integer32String;

			case Type::UnsignedInteger32 :
				return UnsignedInteger32String;

			case Type::Integer64 :
				return Integer64String;

			case Type::UnsignedInteger64 :
				return UnsignedInteger64String;

			case Type::Float :
				return FloatString;

			case Type::Double :
				return DoubleString;

			case Type::LongDouble :
				return LongDoubleString;

			case Type::Boolean :
				return BooleanString;

			case Type::Vector2Float :
				return Vector2FloatString;

			case Type::Vector3Float :
				return Vector3FloatString;

			case Type::Vector4Float :
				return Vector4FloatString;

			case Type::Matrix2Float :
				return Matrix2FloatString;

			case Type::Matrix3Float :
				return Matrix3FloatString;

			case Type::Matrix4Float :
				return Matrix4FloatString;

			case Type::CartesianFrameFloat:
				return CartesianFrameString;

			case Type::Color :
				return ColorString;

			case Type::Null :
				return NullString;
		}

		return NullString;
	}
}
