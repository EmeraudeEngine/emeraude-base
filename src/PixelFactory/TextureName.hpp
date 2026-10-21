/*
 * src/PixelFactory/TextureName.hpp
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
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

/* Local inclusions. */
#include "StaticVector.hpp"

namespace EmEn::Base::PixelFactory
{
	/**
	 * @brief The purposes a texture file can serve: ambientCG's vocabulary (the engine's graphics § 40, owner decision T12).
	 * @note The names are the tokens of a file name, exactly as written (case-sensitive): see to_cstring().
	 */
	enum class TexturePurpose : uint8_t
	{
		/** @brief The albedo (base colour): sRGB, three channels. */
		Color,
		/** @brief The opacity (coverage): one channel. */
		Opacity,
		/** @brief A normal map in the Khronos convention (+Y up, the engine's): two channels on the GPU (X, Y). */
		NormalGL,
		/** @brief A normal map in the Direct3D convention (+Y down): a SOURCE only, converted when baked. */
		NormalDX,
		/** @brief The roughness: one channel. */
		Roughness,
		/** @brief The metalness: one channel. */
		Metalness,
		/** @brief The ambient occlusion: one channel. */
		AmbientOcclusion,
		/** @brief The height (parallax, displacement): one channel. */
		Displacement,
		/** @brief The emission colour: sRGB, three channels. */
		Emission
	};

	/** @brief The number of purposes. */
	inline constexpr size_t TexturePurposeCount{9};

	/** @brief The reason a texture name is refused. */
	enum class TextureNameError : uint8_t
	{
		/** @brief The name is valid. */
		None,
		/** @brief The file name has no `.<Purpose>` part (no dot in its last path component). */
		NoPurpose,
		/** @brief The texture name before the purpose is empty. */
		EmptyName,
		/** @brief A token is empty (`..`, a leading, trailing or doubled `-`). */
		EmptyToken,
		/** @brief A token is not in the vocabulary. */
		UnknownToken,
		/** @brief A token appears twice. */
		RepeatedToken,
		/** @brief The tokens need more than four channels. */
		TooManyChannels,
		/** @brief A normal map shares its file with another purpose (it stands alone). */
		NormalNotAlone,
		/** @brief A colour purpose does not start at the first channel. */
		ColourNotFirst,
		/** @brief A colour purpose shares its file with linear data other than the opacity in the fourth channel. */
		MixedColourSpace,
		/** @brief A Direct3D normal map in the GPU store (it is converted when baked, never shipped). */
		DirectXNormal
	};

	/** @brief Where a texture file lives: the rules differ (the engine's graphics § 40, T2 and T13). */
	enum class TextureStore : uint8_t
	{
		/** @brief The sources (`Images`): any file name is accepted by the store; a conventional one is recognised. */
		Sources,
		/** @brief The GPU-ready textures (`GPUTextures`): a badly named file is refused. */
		GPU
	};

	/** @brief One purpose of a texture name and the channels it occupies. */
	struct TexturePurposeChannels final
	{
		/** @brief The purpose. */
		TexturePurpose purpose{TexturePurpose::Color};
		/** @brief Its first channel, 0 = R. */
		uint8_t firstChannel{0};
		/** @brief Its channel count. */
		uint8_t channelCount{0};
	};

	/**
	 * @brief A texture resource name split as `<TextureName>.<Purpose>`, the Purpose listing its components in channel
	 * order joined by `-` (the engine's graphics § 40, owner decisions T2, T3, T12).
	 * @note The name is the store's resource name: the file's path relative to its store, the extension removed
	 * (`Grounds/Sand002.Color-Opacity` for `GPUTextures/Grounds/Sand002.Color-Opacity.ktx2`). The purpose is after the LAST
	 * dot of the last path component, so a texture name may hold dots itself.
	 * @note Channel rules: a colour purpose (Color, Emission) takes three channels and starts at R; a normal map takes two
	 * and stands alone; every other purpose takes one. Consecutive channels from R, four at most. A colour file is sRGB:
	 * the only linear purpose it may hold is Opacity, in A.
	 */
	class TextureName final
	{
		public:

			/**
			 * @brief Parses a resource name.
			 * @param resourceName The resource name (no extension).
			 * @param store The store the file belongs to.
			 * @return TextureName Check isValid(), or error() for the reason of a refusal.
			 */
			[[nodiscard]]
			static TextureName parse (std::string_view resourceName, TextureStore store) noexcept;

			/**
			 * @brief Returns whether the name follows the convention.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isValid () const noexcept
			{
				return m_error == TextureNameError::None;
			}

			/**
			 * @brief Returns the reason of a refusal, TextureNameError::None for a valid name.
			 * @return TextureNameError
			 */
			[[nodiscard]]
			TextureNameError
			error () const noexcept
			{
				return m_error;
			}

			/**
			 * @brief Returns the texture name (the part before the purpose), empty for an invalid name.
			 * @return const std::string &
			 */
			[[nodiscard]]
			const std::string &
			name () const noexcept
			{
				return m_name;
			}

			/**
			 * @brief Returns the purposes in channel order, empty for an invalid name.
			 * @return std::span< const TexturePurposeChannels >
			 */
			[[nodiscard]]
			std::span< const TexturePurposeChannels >
			purposes () const noexcept
			{
				return {m_purposes.data(), m_purposes.size()};
			}

			/**
			 * @brief Returns the channels the purposes occupy, 1 to 4 (0 for an invalid name).
			 * @return uint32_t
			 */
			[[nodiscard]]
			uint32_t
			channelCount () const noexcept
			{
				return m_channelCount;
			}

			/**
			 * @brief Returns whether the file holds a colour (sRGB) purpose.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isColour () const noexcept
			{
				return m_colour;
			}

			/**
			 * @brief Returns whether the file is a normal map.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isNormalMap () const noexcept;

			/**
			 * @brief Returns the channels of a purpose in this file, when it holds it.
			 * @param purpose The purpose.
			 * @return std::optional< TexturePurposeChannels >
			 */
			[[nodiscard]]
			std::optional< TexturePurposeChannels > find (TexturePurpose purpose) const noexcept;

		private:

			std::string m_name;
			StaticVector< TexturePurposeChannels, 4 > m_purposes;
			uint32_t m_channelCount{0};
			TextureNameError m_error{TextureNameError::NoPurpose};
			bool m_colour{false};
	};

	/**
	 * @brief Returns the token of a purpose, as written in a file name.
	 * @param purpose The purpose.
	 * @return const char *
	 */
	[[nodiscard]]
	const char * to_cstring (TexturePurpose purpose) noexcept;

	/**
	 * @brief Returns a sentence describing a refusal, for a log.
	 * @param error The reason.
	 * @return const char *
	 */
	[[nodiscard]]
	const char * to_cstring (TextureNameError error) noexcept;

	/**
	 * @brief Returns the purpose of a token, when the token is in the vocabulary (case-sensitive).
	 * @param token The token.
	 * @return std::optional< TexturePurpose >
	 */
	[[nodiscard]]
	std::optional< TexturePurpose > texturePurposeFromToken (std::string_view token) noexcept;

	/**
	 * @brief Returns the channels a purpose occupies: three for a colour, two for a normal map, one otherwise.
	 * @param purpose The purpose.
	 * @return uint8_t
	 */
	[[nodiscard]]
	uint8_t texturePurposeChannelCount (TexturePurpose purpose) noexcept;
}
