/*
 * src/Benchmarking/bench_PixelFactoryConvert.cpp
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
 */

/* Third-party inclusions. */
#include <benchmark/benchmark.h>

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <memory>

/* Local inclusions. */
#include "PixelFactory/Pixmap.hpp"
#include "PixelFactory/Processor.hpp"
#include "PixelFactory/Types.hpp"
#include "ThreadPool.hpp"

/* Ave robustus! (A.5): Processor::toGrayscale — per-pixel luminance (Color construction + weighted
 * conversion), arithmetic-bound unlike the pure copies, so a higher parallel ceiling than mirror.
 * Serial (pool = nullptr) vs ThreadPool. */

using namespace EmEn::Base;
using namespace EmEn::Base::PixelFactory;

namespace
{
	Pixmap< uint8_t >
	makeSource (uint32_t width, uint32_t height) noexcept
	{
		Pixmap< uint8_t > source{width, height, ChannelMode::RGB};

		auto & data = source.data();

		for ( size_t index = 0; index < data.size(); ++index )
		{
			data[index] = static_cast< uint8_t >((index * 2654435761U) >> 24);
		}

		return source;
	}

	void
	toGrayscaleBench (benchmark::State & state, bool parallel)
	{
		const auto width = static_cast< uint32_t >(state.range(0));
		const auto height = static_cast< uint32_t >(state.range(1));

		const auto source = makeSource(width, height);

		std::unique_ptr< ThreadPool > pool;

		if ( parallel )
		{
			pool = std::make_unique< ThreadPool >();
		}

		ThreadPool * poolPtr = pool.get();

		for ( auto _ : state )
		{
			auto output = Processor< uint8_t >::toGrayscale(source, GrayscaleConversionMode::LumaRec709, 0, poolPtr);

			benchmark::DoNotOptimize(output.data().data());
			benchmark::ClobberMemory();
		}

		state.SetItemsProcessed(state.iterations() * static_cast< int64_t >(width) * static_cast< int64_t >(height));
	}
}

BENCHMARK_CAPTURE(toGrayscaleBench, serial, false)->Args({3840, 2160})->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(toGrayscaleBench, pool, true)->Args({3840, 2160})->Unit(benchmark::kMillisecond);