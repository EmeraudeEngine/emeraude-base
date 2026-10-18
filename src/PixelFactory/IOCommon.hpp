/*
 * src/PixelFactory/IOCommon.hpp
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
#include <cstdint>
#include <limits>
#include <type_traits>

/* Local inclusions. */
#include "Pixmap.hpp"
#include "Processor.hpp"
#include "Types.hpp"

namespace EmEn::Base::PixelFactory
{
	/**
	 * @brief Applies read post-processing options to a decoded pixmap.
	 * @note This handles Y-axis inversion, channel conversion, and alpha premultiplication.
	 * Called by FileIO and StreamIO after format-specific decoding.
	 * @tparam pixel_data_t The pixel component type. Default uint8_t.
	 * @tparam dimension_t The dimension type. Default uint32_t.
	 * @param pixmap A reference to the pixmap to post-process.
	 * @param options The read options specifying which transformations to apply.
	 * @return bool True if post-processing succeeded.
	 */
	template< typename pixel_data_t = uint8_t, typename dimension_t = uint32_t >
	requires (std::is_arithmetic_v< pixel_data_t > && std::is_unsigned_v< dimension_t >)
	[[nodiscard]]
	bool
	applyReadOptions (Pixmap< pixel_data_t, dimension_t > & pixmap, const ReadOptions & options) noexcept
	{
		if ( !pixmap.isValid() )
		{
			return false;
		}

		/* Y-axis inversion (flip vertically for OpenGL convention). */
		if ( options.invertYAxis )
		{
			pixmap = Processor< pixel_data_t, dimension_t >::mirror(pixmap, MirrorMode::X);
		}

		/* Channel conversion. */
		if ( options.targetChannelMode != TargetChannelMode::KeepOriginal )
		{
			switch ( options.targetChannelMode )
			{
				case TargetChannelMode::Grayscale :
					if ( pixmap.channelMode() != ChannelMode::Grayscale )
					{
						pixmap = Processor< pixel_data_t, dimension_t >::toGrayscale(pixmap);
					}
					break;

				case TargetChannelMode::GrayscaleAlpha :
					/* NOTE: The source's alpha is kept (an RGBA source used to lose it through toGrayscale()). */
					if ( pixmap.channelMode() != ChannelMode::GrayscaleAlpha )
					{
						pixmap = Processor< pixel_data_t, dimension_t >::toGrayscaleAlpha(pixmap);
					}
					break;

				case TargetChannelMode::RGB :
					if ( pixmap.channelMode() != ChannelMode::RGB )
					{
						pixmap = Processor< pixel_data_t, dimension_t >::toRGB(pixmap);
					}
					break;

				case TargetChannelMode::RGBA :
					if ( pixmap.channelMode() != ChannelMode::RGBA )
					{
						pixmap = Processor< pixel_data_t, dimension_t >::toRGBA(pixmap);
					}
					break;

				default:
					break;
			}
		}

		/* Premultiply alpha. */
		if ( options.premultiplyAlpha &&
			(pixmap.channelMode() == ChannelMode::RGBA || pixmap.channelMode() == ChannelMode::GrayscaleAlpha) )
		{
			auto & data = pixmap.data();
			const auto colorCount = pixmap.template colorCount< size_t >();
			const auto alphaIndex = colorCount - 1;
			const auto totalPixels = static_cast< size_t >(pixmap.width()) * static_cast< size_t >(pixmap.height());

			for ( size_t pixelIndex = 0; pixelIndex < totalPixels; ++pixelIndex )
			{
				const auto offset = pixelIndex * colorCount;
				const auto alpha = data[offset + alphaIndex];

				for ( size_t channel = 0; channel < alphaIndex; ++channel )
				{
					if constexpr ( std::is_floating_point_v< pixel_data_t > )
					{
						data[offset + channel] = static_cast< pixel_data_t >(data[offset + channel] * alpha);
					}
					else if constexpr ( std::is_unsigned_v< pixel_data_t > )
					{
						/* NOTE: Divided by the type's full scale: 255 was right for uint8_t only, a uint16_t premultiplied
						 * 40000 by 0.5 into 28270 (2026-10-18). 64 bits hold the product of two uint32_t. */
						constexpr auto FullScale = static_cast< uint64_t >(std::numeric_limits< pixel_data_t >::max());

						data[offset + channel] = static_cast< pixel_data_t >((static_cast< uint64_t >(data[offset + channel]) * static_cast< uint64_t >(alpha)) / FullScale);
					}
					else
					{
						/* NOTE: A signed integer pixmap: its alpha in [0, max]. */
						constexpr auto FullScale = static_cast< int64_t >(std::numeric_limits< pixel_data_t >::max());

						data[offset + channel] = static_cast< pixel_data_t >((static_cast< int64_t >(data[offset + channel]) * static_cast< int64_t >(alpha)) / FullScale);
					}
				}
			}
		}

		return true;
	}
}
