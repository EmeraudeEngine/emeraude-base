/*
 * src/Testing/test_MonotonicArena.cpp
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

/* STL inclusions. */
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "MonotonicArena.hpp"

/*
 * History: the shape decimator kept its adjacency lists in a std::pmr::monotonic_buffer_resource (2026-10-09). Apple's
 * libc++ exports std::pmr from macOS 14.0 only and does not mark it unavailable below: a downstream application targeting
 * macOS 12 and 13 built and linked, then the dynamic loader refused the library at launch on macOS 13.7 ("Symbol not
 * found: __ZNSt3__13pmr15memory_resourceD2Ev"; `nm -um` listed 3 std::pmr imports). MonotonicArena is std::pmr where it
 * exists, MonotonicBlockArena below macOS 14.0. MonotonicBlockArena is tested here on every platform; the loader failure
 * itself is checked on the macOS binary (no std::pmr import left), which a unit test cannot see.
 */

namespace
{
	using EmEn::Base::MonotonicBlockArena;

	/**
	 * @brief Tells whether an address is a multiple of an alignment.
	 * @param pointer The address.
	 * @param alignment The alignment, a power of two.
	 * @return bool
	 */
	[[nodiscard]]
	bool
	isAligned (const void * pointer, size_t alignment) noexcept
	{
		return std::bit_cast< uintptr_t >(pointer) % alignment == 0;
	}

	/** @brief A series of identical allocations. */
	struct AllocationSeries
	{
		size_t count;
		size_t size;
		size_t alignment;
	};

	/**
	 * @brief Counts the allocations whose bytes were overwritten by a later one: each is filled with its own index, then
	 * all are read back.
	 * @param arena The arena.
	 * @param series The allocations.
	 * @return size_t
	 */
	[[nodiscard]]
	size_t
	countOverlappingAllocations (MonotonicBlockArena & arena, const AllocationSeries & series)
	{
		const auto [count, size, alignment] = series;

		std::vector< unsigned char * > blocks;
		blocks.reserve(count);

		for ( size_t index = 0; index < count; ++index )
		{
			auto * block = static_cast< unsigned char * >(arena.allocate(size, alignment));
			std::memset(block, static_cast< int >(index % 251), size);
			blocks.push_back(block);
		}

		size_t overwritten = 0;

		for ( size_t index = 0; index < count; ++index )
		{
			const std::vector< unsigned char > expected(size, static_cast< unsigned char >(index % 251));

			if ( std::memcmp(blocks[index], expected.data(), size) != 0 )
			{
				++overwritten;
			}
		}

		return overwritten;
	}

	/**
	 * @brief Counts the allocations of an arena that do not honour their alignment.
	 * @param arena The arena.
	 * @return size_t
	 */
	[[nodiscard]]
	size_t
	countMisalignedAllocations (MonotonicBlockArena & arena)
	{
		size_t misaligned = 0;

		/* NOTE: An odd size before each request, so the cursor is never aligned by chance. */
		for ( size_t alignment = 1; alignment <= 256; alignment *= 2 )
		{
			static_cast< void >(arena.allocate(3, 1));

			if ( !isAligned(arena.allocate(24, alignment), alignment) )
			{
				++misaligned;
			}
		}

		return misaligned;
	}
}

TEST(MonotonicArena, AllocationsAreAlignedAndDistinct)
{
	MonotonicBlockArena arena{64};

	EXPECT_EQ(countMisalignedAllocations(arena), 0U);
	/* 1000 allocations of 40 bytes from a 64-byte first block: many blocks, and no allocation shares a byte. */
	EXPECT_EQ(countOverlappingAllocations(arena, {.count = 1000, .size = 40, .alignment = alignof(uint64_t)}), 0U);
	EXPECT_GT(arena.blockCount(), 1U);

	/* The default alignment is the one of std::max_align_t, like std::pmr. */
	EXPECT_TRUE(isAligned(arena.allocate(1), alignof(std::max_align_t)));
}

TEST(MonotonicArena, ZeroBytesGiveDistinctPointers)
{
	MonotonicBlockArena arena;

	const void * first = arena.allocate(0, 1);
	const void * second = arena.allocate(0, 1);

	EXPECT_NE(first, nullptr);
	EXPECT_NE(second, nullptr);
	EXPECT_NE(first, second);
}

TEST(MonotonicArena, BlocksGrowGeometrically)
{
	MonotonicBlockArena arena{100};

	EXPECT_EQ(arena.blockCount(), 0U);

	/* The first block is 100 bytes: 100 one-byte allocations fit in it, the 101st opens the second (200 bytes). */
	for ( size_t index = 0; index < 100; ++index )
	{
		static_cast< void >(arena.allocate(1, 1));
	}

	EXPECT_EQ(arena.blockCount(), 1U);

	static_cast< void >(arena.allocate(1, 1));

	EXPECT_EQ(arena.blockCount(), 2U);

	for ( size_t index = 0; index < 199; ++index )
	{
		static_cast< void >(arena.allocate(1, 1));
	}

	EXPECT_EQ(arena.blockCount(), 2U);

	/* A request larger than the next block (400) gets a block of its own size, usable whole. */
	auto * large = static_cast< unsigned char * >(arena.allocate(10000, 1));
	std::memset(large, 0xAB, 10000);

	EXPECT_EQ(arena.blockCount(), 3U);
}

TEST(MonotonicArena, ReleaseFreesEverythingAndTheArenaServesAgain)
{
	MonotonicBlockArena arena{32};

	EXPECT_EQ(countOverlappingAllocations(arena, {.count = 200, .size = 16, .alignment = 8}), 0U);
	EXPECT_GT(arena.blockCount(), 1U);

	arena.release();

	EXPECT_EQ(arena.blockCount(), 0U);
	/* After release(), the blocks start again from the initial size. */
	EXPECT_EQ(countOverlappingAllocations(arena, {.count = 2, .size = 16, .alignment = 8}), 0U);
	EXPECT_EQ(arena.blockCount(), 1U);
	EXPECT_EQ(countMisalignedAllocations(arena), 0U);
}

TEST(MonotonicArena, TheSelectedArenaServesTheLibrary)
{
	/* MonotonicArena is std::pmr::monotonic_buffer_resource or MonotonicBlockArena (EMERAUDE_STD_PMR_AVAILABLE); the
	 * library only calls allocate(bytes, alignment) on it. */
	EmEn::Base::MonotonicArena arena;

	const auto * first = static_cast< uint64_t * >(arena.allocate(sizeof(uint64_t) * 4, alignof(uint64_t)));
	const auto * second = static_cast< uint64_t * >(arena.allocate(sizeof(uint64_t) * 4, alignof(uint64_t)));

	EXPECT_NE(first, nullptr);
	EXPECT_NE(second, nullptr);
	EXPECT_NE(first, second);
	EXPECT_TRUE(isAligned(first, alignof(uint64_t)));
	EXPECT_TRUE(isAligned(second, alignof(uint64_t)));

	/* The alias follows the switch. */
	EXPECT_EQ((std::is_same_v< EmEn::Base::MonotonicArena, MonotonicBlockArena >), EMERAUDE_STD_PMR_AVAILABLE == 0);

#if defined(__APPLE__) && defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 140000
	/* Below macOS 14.0, the library must not touch std::pmr (the loader would refuse it). */
	EXPECT_EQ(EMERAUDE_STD_PMR_AVAILABLE, 0);
#endif
}
