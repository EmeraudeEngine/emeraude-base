/*
 * src/MonotonicArena.hpp
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
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <vector>

/*
 * Whether the standard library can serve std::pmr to this build.
 *
 * Apple's system libc++ exports std::pmr (memory_resource, monotonic_buffer_resource, get_default_resource) from
 * macOS 14.0 only, and its headers do NOT mark std::pmr with availability attributes (the SDK's
 * <__configuration/availability.h> defines _LIBCPP_AVAILABILITY_PMR empty, "TODO: Enable std::pmr markup once
 * https://llvm.org/PR40340 has been fixed"): a build targeting an older macOS compiles and links without a word, and the
 * dynamic loader refuses the binary at launch ("Symbol not found: __ZNSt3__13pmr15memory_resourceD2Ev", reported
 * on macOS 13.7). Every other platform of the project has it.
 *
 * Defined beforehand (a compile definition), it forces the choice: `-DEMERAUDE_STD_PMR_AVAILABLE=0` builds the
 * fallback on any platform, which is how the whole suite is run against it outside macOS.
 */
#ifndef EMERAUDE_STD_PMR_AVAILABLE
	#ifdef __APPLE__
		#if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 140000
			#define EMERAUDE_STD_PMR_AVAILABLE 1
		#else
			#define EMERAUDE_STD_PMR_AVAILABLE 0
		#endif
	#else
		#define EMERAUDE_STD_PMR_AVAILABLE 1
	#endif
#endif

#if EMERAUDE_STD_PMR_AVAILABLE
	#include <memory_resource>
#endif

namespace EmEn::Base
{
	/**
	 * @brief A memory arena that only grows: allocate() takes the request from the free room of the current block and
	 * opens a new block, twice the size of the previous one (or the request, if larger), when it is full. Nothing is
	 * freed before release() or the destruction: releasing a million small allocations is a few frees.
	 * @note The interface is the subset of std::pmr::monotonic_buffer_resource the library uses (allocate(), release()),
	 * so MonotonicArena can be either. It is compiled and tested on every platform, but MonotonicArena only selects it
	 * where std::pmr is not available (see EMERAUDE_STD_PMR_AVAILABLE).
	 * @note Not thread-safe, like std::pmr::monotonic_buffer_resource. Neither copyable nor movable: the allocations
	 * point into it.
	 */
	class MonotonicBlockArena final
	{
		public:

			/** @brief The size of the first block, in bytes. */
			static constexpr size_t DefaultInitialBlockSize{1024};

			/** @brief Constructs an arena whose first block is DefaultInitialBlockSize bytes. */
			MonotonicBlockArena () noexcept = default;

			/**
			 * @brief Constructs an arena with a chosen first block size.
			 * @param initialBlockSize The size of the first block, in bytes (0 is read as 1).
			 */
			explicit
			MonotonicBlockArena (size_t initialBlockSize) noexcept
				: m_initialBlockSize{std::max(initialBlockSize, size_t{1})},
				m_nextBlockSize{m_initialBlockSize}
			{

			}

			MonotonicBlockArena (const MonotonicBlockArena &) = delete;
			MonotonicBlockArena & operator= (const MonotonicBlockArena &) = delete;
			MonotonicBlockArena (MonotonicBlockArena &&) = delete;
			MonotonicBlockArena & operator= (MonotonicBlockArena &&) = delete;

			/** @brief Frees every block. */
			~MonotonicBlockArena () = default;

			/**
			 * @brief Returns memory from the arena, valid until release() or the destruction.
			 * @param bytes The size, in bytes (0 gives a valid, distinct pointer, like std::pmr).
			 * @param alignment The alignment, a power of two.
			 * @return void * Never null: a request the system cannot serve ends the program, like operator new in the
			 * library's build (-fno-exceptions) — and like std::pmr::monotonic_buffer_resource there.
			 */
			[[nodiscard]]
			void *
			allocate (size_t bytes, size_t alignment = alignof(std::max_align_t))
			{
				/* NOTE: Zero bytes still take one, so two such allocations differ (std::pmr does the same). */
				const auto size = std::max(bytes, size_t{1});

				/* NOTE: The private steps take the alignment as std::align_val_t, a type the size cannot be swapped with. */
				const auto align = std::align_val_t{alignment};

				if ( !this->fits(size, align) )
				{
					this->openBlock(size, align);
				}

				return this->carve(size, align);
			}

			/** @brief Frees every block. The arena can be used again, from a first block of the initial size. */
			void
			release () noexcept
			{
				m_blocks.clear();
				m_free = {};
				m_nextBlockSize = m_initialBlockSize;
			}

			/**
			 * @brief Returns the number of blocks the arena holds.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			blockCount () const noexcept
			{
				return m_blocks.size();
			}

		private:

			/**
			 * @brief Returns the bytes to skip in the free room of the current block to reach an alignment.
			 * @param alignment The alignment, a power of two.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			paddingFor (std::align_val_t alignment) const noexcept
			{
				const auto step = static_cast< size_t >(alignment);
				const auto misalignment = static_cast< size_t >(std::bit_cast< uintptr_t >(m_free.data()) % step);

				return misalignment == 0 ? 0 : step - misalignment;
			}

			/**
			 * @brief Tells whether the free room of the current block holds a request, alignment padding included.
			 * @param size The size, in bytes (> 0).
			 * @param alignment The alignment, a power of two.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			fits (size_t size, std::align_val_t alignment) const noexcept
			{
				if ( m_free.empty() )
				{
					return false;
				}

				const auto padding = this->paddingFor(alignment);

				/* NOTE: Compared as remaining room, so a huge size cannot wrap around. */
				return padding <= m_free.size() && size <= m_free.size() - padding;
			}

			/**
			 * @brief Takes a request from the free room of the current block.
			 * @pre fits(size, alignment).
			 * @param size The size, in bytes (> 0).
			 * @param alignment The alignment, a power of two.
			 * @return void *
			 */
			[[nodiscard]]
			void *
			carve (size_t size, std::align_val_t alignment) noexcept
			{
				const auto padding = this->paddingFor(alignment);
				const auto taken = m_free.subspan(padding, size);

				m_free = m_free.subspan(padding + size);

				return taken.data();
			}

			/**
			 * @brief Opens a block large enough for one request.
			 * @param size The size, in bytes (> 0).
			 * @param alignment The alignment, a power of two.
			 */
			void
			openBlock (size_t size, std::align_val_t alignment)
			{
				constexpr auto Limit = std::numeric_limits< size_t >::max();
				const auto padding = static_cast< size_t >(alignment);

				/* NOTE: The worst alignment padding, then the request; a sum that would wrap saturates, so the allocation
				 * fails (and ends the program) instead of returning a block too small. */
				const auto needed = size > Limit - padding ? Limit : padding + size;
				const auto blockSize = std::max(m_nextBlockSize, needed);

				/* NOTE: Raw storage, not a std::vector< std::byte >: the blocks are not zeroed (a large mesh fills tens of MB). */
				auto & block = m_blocks.emplace_back(static_cast< std::byte * >(::operator new(blockSize)));

				m_free = std::span< std::byte >{block.get(), blockSize};
				m_nextBlockSize = blockSize > Limit / 2 ? Limit : blockSize * 2;
			}

			/** @brief Gives a block back to ::operator delete. */
			struct BlockRelease final
			{
				void
				operator() (std::byte * block) const noexcept
				{
					::operator delete(block);
				}
			};

			std::vector< std::unique_ptr< std::byte, BlockRelease > > m_blocks;
			std::span< std::byte > m_free;
			size_t m_initialBlockSize{DefaultInitialBlockSize};
			size_t m_nextBlockSize{DefaultInitialBlockSize};
	};

#if EMERAUDE_STD_PMR_AVAILABLE
	/**
	 * @brief The library's monotonic arena: std::pmr::monotonic_buffer_resource where the standard library serves it.
	 * @note When every supported macOS is 14.0 or newer, EMERAUDE_STD_PMR_AVAILABLE is 1 everywhere and
	 * MonotonicBlockArena can go (docs/todo/drop-the-monotonic-block-arena-at-macos-14.md).
	 */
	using MonotonicArena = std::pmr::monotonic_buffer_resource;
#else
	/** @brief The library's monotonic arena: MonotonicBlockArena, where std::pmr is not available (macOS before 14.0). */
	using MonotonicArena = MonotonicBlockArena;
#endif
}
