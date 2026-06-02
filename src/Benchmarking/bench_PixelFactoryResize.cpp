/*
 * src/Benchmarking/bench_PixelFactoryResize.cpp
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

/* Ave robustus! (A.5 — performance, benchmark-gated): baseline + before/after numbers for the
 * three image-resize filters (Processor::resize → resizeNearest/resizeLinear/resizeCubic).
 * Each filter runs serially (pool = nullptr) and on a warmed-up ThreadPool (pool = &pool), so the
 * speedup of the A.5 parallelization is measured, never assumed. Sources are built in-memory
 * (no fixture file / working-dir dependency). Run in Release. */

using namespace EmEn::Base;
using namespace EmEn::Base::PixelFactory;

namespace
{
	/* A deterministic, non-uniform RGB source — touches every byte so the resize reads real data. */
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
	resizeBench (benchmark::State & state, FilteringMode mode, bool parallel)
	{
		const auto srcWidth = static_cast< uint32_t >(state.range(0));
		const auto srcHeight = static_cast< uint32_t >(state.range(1));
		const auto dstWidth = static_cast< uint32_t >(state.range(2));
		const auto dstHeight = static_cast< uint32_t >(state.range(3));

		const auto source = makeSource(srcWidth, srcHeight);

		/* Warm the pool up ONCE (outside the measured loop) when measuring the parallel path. */
		std::unique_ptr< ThreadPool > pool;

		if ( parallel )
		{
			pool = std::make_unique< ThreadPool >();
		}

		ThreadPool * poolPtr = pool.get();

		for ( auto _ : state )
		{
			auto output = Processor< uint8_t >::resize(source, dstWidth, dstHeight, mode, poolPtr);

			benchmark::DoNotOptimize(output.data().data());
			benchmark::ClobberMemory();
		}

		state.SetItemsProcessed(state.iterations() * static_cast< int64_t >(dstWidth) * static_cast< int64_t >(dstHeight));
	}
}

/* Args = {srcW, srcH, dstW, dstH}: a downscale (1920x1080 → 960x540) and an upscale
 * (1200x800 → 2400x1600). */
#define EMERAUDE_RESIZE_SERIAL(name, mode) \
	BENCHMARK_CAPTURE(resizeBench, name##_down, mode, false)->Args({1920, 1080, 960, 540})->Unit(benchmark::kMillisecond); \
	BENCHMARK_CAPTURE(resizeBench, name##_up, mode, false)->Args({1200, 800, 2400, 1600})->Unit(benchmark::kMillisecond);

#define EMERAUDE_RESIZE_POOL(name, mode) \
	BENCHMARK_CAPTURE(resizeBench, name##_pool_down, mode, true)->Args({1920, 1080, 960, 540})->Unit(benchmark::kMillisecond); \
	BENCHMARK_CAPTURE(resizeBench, name##_pool_up, mode, true)->Args({1200, 800, 2400, 1600})->Unit(benchmark::kMillisecond);

/* Serial baseline for the three filters. */
EMERAUDE_RESIZE_SERIAL(nearest, FilteringMode::Nearest)
EMERAUDE_RESIZE_SERIAL(linear, FilteringMode::Linear)
EMERAUDE_RESIZE_SERIAL(cubic, FilteringMode::Cubic)

/* Parallel path on the ThreadPool — all three filters. */
EMERAUDE_RESIZE_POOL(nearest, FilteringMode::Nearest)
EMERAUDE_RESIZE_POOL(linear, FilteringMode::Linear)
EMERAUDE_RESIZE_POOL(cubic, FilteringMode::Cubic)