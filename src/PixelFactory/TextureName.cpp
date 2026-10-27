/*
 * src/PixelFactory/TextureName.cpp
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

#include "TextureName.hpp"

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace EmEn::Base::PixelFactory
{
	namespace
	{
		/** @brief The tokens, in the enumeration's order. */
		constexpr std::array< std::string_view, TexturePurposeCount > Tokens{
			"Color", "Opacity", "NormalGL", "NormalDX", "Roughness", "Metalness", "AmbientOcclusion", "Displacement", "Emission"
		};

		/** @brief The most tokens a purpose holds: one per channel. */
		constexpr size_t MaxTokens{4};

		[[nodiscard]]
		bool
		isColourPurpose (TexturePurpose purpose) noexcept
		{
			return purpose == TexturePurpose::Color || purpose == TexturePurpose::Emission;
		}

		[[nodiscard]]
		bool
		isNormalPurpose (TexturePurpose purpose) noexcept
		{
			return purpose == TexturePurpose::NormalGL || purpose == TexturePurpose::NormalDX;
		}

		/**
		 * @brief Checks the channel rules of a parsed purpose list (see TextureName).
		 * @param purposes The purposes in channel order.
		 * @param store The store the file belongs to.
		 * @return TextureNameError
		 */
		[[nodiscard]]
		TextureNameError
		checkChannels (std::span< const TexturePurposeChannels > purposes, TextureStore store) noexcept
		{
			const auto hasNormal = std::ranges::any_of(purposes, [] (const auto & entry) { return isNormalPurpose(entry.purpose); });

			if ( hasNormal && purposes.size() > 1 )
			{
				return TextureNameError::NormalNotAlone;
			}

			if ( store == TextureStore::GPU && hasNormal && purposes.front().purpose == TexturePurpose::NormalDX )
			{
				return TextureNameError::DirectXNormal;
			}

			if ( !std::ranges::any_of(purposes, [] (const auto & entry) { return isColourPurpose(entry.purpose); }) )
			{
				return TextureNameError::None;
			}

			if ( !isColourPurpose(purposes.front().purpose) )
			{
				return TextureNameError::ColourNotFirst;
			}

			/* A colour file is sRGB: after its colour, only the opacity, in A (Vulkan keeps A linear in an sRGB format). */
			for ( const auto & entry : purposes.subspan(1) )
			{
				if ( entry.purpose != TexturePurpose::Opacity || entry.firstChannel != 3 )
				{
					return TextureNameError::MixedColourSpace;
				}
			}

			return TextureNameError::None;
		}
	}

	TextureName
	TextureName::parse (std::string_view resourceName, TextureStore store) noexcept
	{
		TextureName result;

		/* The purpose follows the LAST dot of the last path component. */
		const auto lastSlash = resourceName.find_last_of('/');
		const auto fileStart = lastSlash == std::string_view::npos ? 0 : lastSlash + 1;
		const auto dot = resourceName.find_last_of('.');

		if ( dot == std::string_view::npos || dot < fileStart )
		{
			result.m_error = TextureNameError::NoPurpose;

			return result;
		}

		if ( dot == fileStart )
		{
			result.m_error = TextureNameError::EmptyName;

			return result;
		}

		auto purpose = resourceName.substr(dot + 1);
		uint32_t channel = 0;

		while ( true )
		{
			const auto separator = purpose.find('-');
			const auto token = purpose.substr(0, separator);

			if ( token.empty() )
			{
				result.m_error = TextureNameError::EmptyToken;

				return result;
			}

			const auto parsed = texturePurposeFromToken(token);

			if ( !parsed.has_value() )
			{
				result.m_error = TextureNameError::UnknownToken;

				return result;
			}

			if ( result.find(*parsed).has_value() )
			{
				result.m_error = TextureNameError::RepeatedToken;

				return result;
			}

			const auto width = texturePurposeChannelCount(*parsed);

			if ( result.m_purposes.size() >= MaxTokens || channel + width > 4 )
			{
				result.m_error = TextureNameError::TooManyChannels;

				return result;
			}

			result.m_purposes.push_back({.purpose = *parsed, .firstChannel = static_cast< uint8_t >(channel), .channelCount = width});
			channel += width;

			if ( separator == std::string_view::npos )
			{
				break;
			}

			purpose.remove_prefix(separator + 1);
		}

		if ( const auto channelError = checkChannels(result.purposes(), store); channelError != TextureNameError::None )
		{
			result.m_error = channelError;
			result.m_purposes.clear();

			return result;
		}

		result.m_name = std::string{resourceName.substr(0, dot)};
		result.m_channelCount = channel;
		result.m_colour = isColourPurpose(result.m_purposes.front().purpose);
		result.m_error = TextureNameError::None;

		return result;
	}

	bool
	TextureName::isNormalMap () const noexcept
	{
		return !m_purposes.empty() && isNormalPurpose(m_purposes.front().purpose);
	}

	std::optional< TexturePurposeChannels >
	TextureName::find (TexturePurpose purpose) const noexcept
	{
		for ( const auto & entry : m_purposes )
		{
			if ( entry.purpose == purpose )
			{
				return entry;
			}
		}

		return std::nullopt;
	}

	const char *
	to_cstring (TexturePurpose purpose) noexcept
	{
		switch ( purpose )
		{
			case TexturePurpose::Color :
				return "Color";

			case TexturePurpose::Opacity :
				return "Opacity";

			case TexturePurpose::NormalGL :
				return "NormalGL";

			case TexturePurpose::NormalDX :
				return "NormalDX";

			case TexturePurpose::Roughness :
				return "Roughness";

			case TexturePurpose::Metalness :
				return "Metalness";

			case TexturePurpose::AmbientOcclusion :
				return "AmbientOcclusion";

			case TexturePurpose::Displacement :
				return "Displacement";

			case TexturePurpose::Emission :
				return "Emission";
		}

		return "Unknown";
	}

	const char *
	to_cstring (TextureNameError error) noexcept
	{
		switch ( error )
		{
			case TextureNameError::None :
				return "valid";

			case TextureNameError::NoPurpose :
				return "no '.<Purpose>' in the file name";

			case TextureNameError::EmptyName :
				return "no texture name before the purpose";

			case TextureNameError::EmptyToken :
				return "an empty token in the purpose";

			case TextureNameError::UnknownToken :
				return "a token outside the vocabulary (Color, Opacity, NormalGL, NormalDX, Roughness, Metalness, AmbientOcclusion, Displacement, Emission)";

			case TextureNameError::RepeatedToken :
				return "a token repeated";

			case TextureNameError::TooManyChannels :
				return "more than four channels";

			case TextureNameError::NormalNotAlone :
				return "a normal map shares its file";

			case TextureNameError::ColourNotFirst :
				return "a colour that does not start at the first channel";

			case TextureNameError::MixedColourSpace :
				return "a colour file holding linear data other than the opacity in A";

			case TextureNameError::DirectXNormal :
				return "a Direct3D normal map (NormalDX) is a source only: bake it to NormalGL";
		}

		return "unknown error";
	}

	std::optional< TexturePurpose >
	texturePurposeFromToken (std::string_view token) noexcept
	{
		for ( size_t index = 0; index < Tokens.size(); ++index )
		{
			if ( std::span{Tokens}[index] == token )
			{
				return static_cast< TexturePurpose >(index);
			}
		}

		return std::nullopt;
	}

	uint8_t
	texturePurposeChannelCount (TexturePurpose purpose) noexcept
	{
		if ( isColourPurpose(purpose) )
		{
			return 3;
		}

		if ( isNormalPurpose(purpose) )
		{
			return 2;
		}

		return 1;
	}
}
