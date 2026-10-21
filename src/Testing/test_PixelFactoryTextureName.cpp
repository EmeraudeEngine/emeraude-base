/*
 * src/Testing/test_PixelFactoryTextureName.cpp
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

#include <gtest/gtest.h>

/* STL inclusions. */
#include <array>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

/* Local inclusions. */
#include "PixelFactory/TextureName.hpp"

using namespace EmEn::Base::PixelFactory;

namespace
{
	/**
	 * @brief Parses a name and returns its refusal (None when valid).
	 * @param name The resource name.
	 * @param store The store.
	 * @return TextureNameError
	 */
	TextureNameError
	refusal (std::string_view name, TextureStore store = TextureStore::GPU) noexcept
	{
		return TextureName::parse(name, store).error();
	}
}

/* The canonical layout (graphics § 40): every texture of it is a valid name, with its channels. */
TEST(PixelFactoryTextureName, parsesTheCanonicalLayout)
{
	const auto albedo = TextureName::parse("Grounds/Sand002.Color-Opacity", TextureStore::GPU);

	ASSERT_TRUE(albedo.isValid());
	EXPECT_EQ(albedo.name(), "Grounds/Sand002");
	EXPECT_EQ(albedo.channelCount(), 4U);
	EXPECT_TRUE(albedo.isColour());
	EXPECT_FALSE(albedo.isNormalMap());
	ASSERT_EQ(albedo.purposes().size(), 2U);
	EXPECT_EQ(albedo.purposes()[1].purpose, TexturePurpose::Opacity);
	EXPECT_EQ(albedo.purposes()[1].firstChannel, 3);

	const auto normal = TextureName::parse("Grounds/Sand002.NormalGL", TextureStore::GPU);

	ASSERT_TRUE(normal.isValid());
	EXPECT_TRUE(normal.isNormalMap());
	EXPECT_FALSE(normal.isColour());
	EXPECT_EQ(normal.channelCount(), 2U);

	const auto data = TextureName::parse("Grounds/Sand002.AmbientOcclusion-Roughness-Metalness-Displacement", TextureStore::GPU);

	ASSERT_TRUE(data.isValid());
	EXPECT_FALSE(data.isColour());
	EXPECT_EQ(data.channelCount(), 4U);

	const auto roughness = data.find(TexturePurpose::Roughness).value_or(TexturePurposeChannels{});

	EXPECT_EQ(roughness.purpose, TexturePurpose::Roughness);
	EXPECT_EQ(roughness.firstChannel, 1);
	EXPECT_EQ(roughness.channelCount, 1);
	EXPECT_FALSE(data.find(TexturePurpose::Color).has_value());

	EXPECT_EQ(refusal("Grounds/Sand002.Roughness"), TextureNameError::None);
	EXPECT_EQ(refusal("Grounds/Sand002.Color"), TextureNameError::None);
	EXPECT_EQ(refusal("Lamps/Neon.Emission-Opacity"), TextureNameError::None);
}

/* The purpose follows the LAST dot of the last path component: a texture name may hold dots, a directory too. */
TEST(PixelFactoryTextureName, splitsAtTheLastDotOfTheFileName)
{
	const auto dotted = TextureName::parse("Old.Pack/Bricks.v2.Color", TextureStore::GPU);

	ASSERT_TRUE(dotted.isValid());
	EXPECT_EQ(dotted.name(), "Old.Pack/Bricks.v2");

	EXPECT_EQ(refusal("Old.Pack/Bricks"), TextureNameError::NoPurpose);
	EXPECT_EQ(refusal("Bricks"), TextureNameError::NoPurpose);
	EXPECT_EQ(refusal("Walls/.Color"), TextureNameError::EmptyName);
	EXPECT_EQ(refusal(".Color"), TextureNameError::EmptyName);
	EXPECT_EQ(refusal(""), TextureNameError::NoPurpose);
}

/* Every token is checked: known (case-sensitive), not empty, not repeated. */
TEST(PixelFactoryTextureName, refusesBadTokens)
{
	EXPECT_EQ(refusal("Sand002.Rough"), TextureNameError::UnknownToken);
	EXPECT_EQ(refusal("Sand002.color"), TextureNameError::UnknownToken);
	EXPECT_EQ(refusal("Sand002.Normal"), TextureNameError::UnknownToken);
	EXPECT_EQ(refusal("Sand002."), TextureNameError::EmptyToken);
	EXPECT_EQ(refusal("Sand002.Roughness-"), TextureNameError::EmptyToken);
	EXPECT_EQ(refusal("Sand002.-Roughness"), TextureNameError::EmptyToken);
	EXPECT_EQ(refusal("Sand002.Roughness--Metalness"), TextureNameError::EmptyToken);
	EXPECT_EQ(refusal("Sand002.Roughness-Roughness"), TextureNameError::RepeatedToken);
}

/* The channel rules: four channels at most, the normal alone, a colour first and sharing its file with the opacity in A
 * only. */
TEST(PixelFactoryTextureName, refusesBadChannelLayouts)
{
	EXPECT_EQ(refusal("Sand002.AmbientOcclusion-Roughness-Metalness-Displacement-Opacity"), TextureNameError::TooManyChannels);
	EXPECT_EQ(refusal("Sand002.Color-Emission"), TextureNameError::TooManyChannels);
	EXPECT_EQ(refusal("Sand002.Color-Opacity-Roughness"), TextureNameError::TooManyChannels);
	EXPECT_EQ(refusal("Sand002.NormalGL-Displacement"), TextureNameError::NormalNotAlone);
	EXPECT_EQ(refusal("Sand002.Roughness-NormalGL"), TextureNameError::NormalNotAlone);
	EXPECT_EQ(refusal("Sand002.Opacity-Color"), TextureNameError::ColourNotFirst);
	EXPECT_EQ(refusal("Sand002.Color-Roughness"), TextureNameError::MixedColourSpace);
	EXPECT_EQ(refusal("Sand002.Emission-Metalness"), TextureNameError::MixedColourSpace);
}

/* The two stores: a Direct3D normal map is a source, refused in the GPU store (it is converted when baked). */
TEST(PixelFactoryTextureName, aDirectXNormalIsASourceOnly)
{
	EXPECT_EQ(refusal("Sand002.NormalDX", TextureStore::GPU), TextureNameError::DirectXNormal);

	const auto source = TextureName::parse("Sand002.NormalDX", TextureStore::Sources);

	ASSERT_TRUE(source.isValid());
	EXPECT_TRUE(source.isNormalMap());
	EXPECT_EQ(source.channelCount(), 2U);
}

/* A refused name keeps nothing: no texture name, no purpose, no channel. */
TEST(PixelFactoryTextureName, aRefusedNameIsEmpty)
{
	const auto refused = TextureName::parse("Sand002.Color-Roughness", TextureStore::GPU);

	EXPECT_FALSE(refused.isValid());
	EXPECT_TRUE(refused.name().empty());
	EXPECT_TRUE(refused.purposes().empty());
	EXPECT_EQ(refused.channelCount(), 0U);
	EXPECT_FALSE(refused.isColour());
	EXPECT_FALSE(refused.isNormalMap());
}

/* Every purpose round-trips through its token, and has its channel width. */
TEST(PixelFactoryTextureName, everyTokenRoundTrips)
{
	constexpr std::array< std::pair< TexturePurpose, unsigned >, TexturePurposeCount > Purposes{{
		{TexturePurpose::Color, 3}, {TexturePurpose::Opacity, 1}, {TexturePurpose::NormalGL, 2}, {TexturePurpose::NormalDX, 2},
		{TexturePurpose::Roughness, 1}, {TexturePurpose::Metalness, 1}, {TexturePurpose::AmbientOcclusion, 1},
		{TexturePurpose::Displacement, 1}, {TexturePurpose::Emission, 3}
	}};

	for ( const auto & [purpose, width] : Purposes )
	{
		EXPECT_EQ(texturePurposeFromToken(to_cstring(purpose)), std::optional< TexturePurpose >{purpose}) << to_cstring(purpose);
		EXPECT_EQ(texturePurposeChannelCount(purpose), width) << to_cstring(purpose);
	}

	EXPECT_FALSE(texturePurposeFromToken("Albedo").has_value());
	EXPECT_STRNE(to_cstring(TextureNameError::UnknownToken), "");
}
