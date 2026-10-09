/*
 * src/FlatHashMap.hpp
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
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace EmEn::Base
{
	/**
	 * @brief Mixes a 64-bit integer into a well-spread hash, the same on every platform.
	 * @note The splitmix64 finalizer (G. L. Steele Jr., D. Lea, C. H. Flood, "Fast Splittable Pseudorandom Number
	 * Generators", OOPSLA 2014; the constants of S. Vigna's public-domain splitmix64.c, https://prng.di.unimi.it/).
	 * std::hash is implementation-defined (the identity under libstdc++, FNV-1a under MSVC): a container hashed by it
	 * walks its entries in a different order on each standard library — and a floating-point sum taken in that order
	 * differs (base caution-points, portable random).
	 * @param value The value.
	 * @return uint64_t
	 */
	[[nodiscard]]
	constexpr
	uint64_t
	mixHash (uint64_t value) noexcept
	{
		value ^= value >> 30U;
		value *= UINT64_C(0xBF58476D1CE4E5B9);
		value ^= value >> 27U;
		value *= UINT64_C(0x94D049BB133111EB);
		value ^= value >> 31U;

		return value;
	}

	/**
	 * @brief The default hasher of FlatHashMap: mixHash() of an integral or enumeration key, the same on every platform.
	 * @tparam key_t An integral or enumeration type.
	 */
	template< typename key_t >
	requires (std::is_integral_v< key_t > || std::is_enum_v< key_t >)
	struct PortableHash final
	{
		[[nodiscard]]
		constexpr
		uint64_t
		operator() (key_t key) const noexcept
		{
			if constexpr ( std::is_enum_v< key_t > )
			{
				return mixHash(static_cast< uint64_t >(static_cast< std::underlying_type_t< key_t > >(key)));
			}
			else
			{
				return mixHash(static_cast< uint64_t >(key));
			}
		}
	};

	/**
	 * @brief A hash map stored in ONE flat array: open addressing with linear probing, a power-of-two capacity kept at
	 * most half full, no erase.
	 * @note Why (Ave Robustus II D7, 2026-10-08): a node-based std::unordered_map allocates one node per entry; a
	 * million-entry map costs a million allocations to fill and as many to release, and that release alone held a
	 * cancelled mesh decimation for 0.3 to 1.5 s. This map allocates once (reserve() it to the expected count and it
	 * never grows), releases in one free, and keeps its entries contiguous. Linear probing: D. E. Knuth, The Art of
	 * Computer Programming vol. 3, § 6.4 (Algorithm L).
	 * @note Deterministic: with PortableHash (the default) the same insertions give the same layout and the same
	 * forEach() order on every platform.
	 * @note Not thread-safe; references and pointers to values are invalidated by a growth (an insertion past the reserved
	 * count) and by clear().
	 * @tparam key_t The key type (default-constructible, copyable, equality-comparable).
	 * @tparam value_t The value type (default-constructible, movable).
	 * @tparam hash_t A hasher returning an integer; PortableHash< key_t > by default.
	 * @tparam equal_t The key equality.
	 */
	template< typename key_t, typename value_t, typename hash_t = PortableHash< key_t >, typename equal_t = std::equal_to< key_t > >
	requires std::default_initializable< key_t > && std::default_initializable< value_t > && std::copy_constructible< key_t >
	class FlatHashMap final
	{
		public:

			/** @brief Constructs an empty map (no allocation until the first insertion or reserve()). */
			FlatHashMap () noexcept = default;

			/**
			 * @brief Constructs an empty map sized for an expected count of entries (one allocation).
			 * @param expectedCount The number of entries it will hold without growing.
			 */
			explicit
			FlatHashMap (size_t expectedCount) noexcept
			{
				this->reserve(expectedCount);
			}

			/**
			 * @brief Constructs an empty map sized for an expected count, with a hasher and an equality that carry state
			 * (e.g. keys that are indices into an array the functors read: the slots then stay small).
			 * @param expectedCount The number of entries it will hold without growing.
			 * @param hash The hasher.
			 * @param equal The key equality.
			 */
			FlatHashMap (size_t expectedCount, hash_t hash, equal_t equal) noexcept
				: m_hash{std::move(hash)},
				m_equal{std::move(equal)}
			{
				this->reserve(expectedCount);
			}

			/**
			 * @brief Makes room for at least expectedCount entries without growing. Never shrinks.
			 * @param expectedCount The number of entries.
			 */
			void
			reserve (size_t expectedCount) noexcept
			{
				const auto wanted = FlatHashMap::capacityFor(expectedCount);

				if ( wanted > m_slots.size() )
				{
					this->rehash(wanted);
				}
			}

			/**
			 * @brief Inserts key → value if the key is absent; returns the stored value and whether it was inserted.
			 * @param key The key.
			 * @param value The value inserted when the key is absent (ignored otherwise).
			 * @return std::pair< value_t *, bool > The value stored for the key (never nullptr), true when inserted.
			 */
			[[nodiscard]]
			std::pair< value_t *, bool >
			tryEmplace (const key_t & key, value_t value) noexcept
			{
				if ( (m_size + 1) * 2 > m_slots.size() )
				{
					this->rehash(FlatHashMap::capacityFor(m_size + 1));
				}

				auto index = this->home(key);

				while ( m_slots[index].occupied )
				{
					if ( m_equal(m_slots[index].key, key) )
					{
						return {&m_slots[index].value, false};
					}

					index = (index + 1) & m_mask;
				}

				auto & slot = m_slots[index];
				slot.key = key;
				slot.value = std::move(value);
				slot.occupied = true;
				++m_size;

				return {&slot.value, true};
			}

			/**
			 * @brief Returns the value stored for a key, default-inserting it when absent (as std::unordered_map::operator[]).
			 * @param key The key.
			 * @return value_t &
			 */
			value_t &
			operator[] (const key_t & key) noexcept
			{
				return *this->tryEmplace(key, value_t{}).first;
			}

			/**
			 * @brief Returns the value stored for a key, or nullptr.
			 * @param key The key.
			 * @return const value_t *
			 */
			[[nodiscard]]
			const value_t *
			find (const key_t & key) const noexcept
			{
				const auto index = this->slotOf(key);

				return index == NotFound ? nullptr : &m_slots[index].value;
			}

			/**
			 * @brief Returns the value stored for a key, or nullptr.
			 * @param key The key.
			 * @return value_t *
			 */
			[[nodiscard]]
			value_t *
			find (const key_t & key) noexcept
			{
				const auto index = this->slotOf(key);

				return index == NotFound ? nullptr : &m_slots[index].value;
			}

			/**
			 * @brief Returns whether a key is stored.
			 * @param key The key.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			contains (const key_t & key) const noexcept
			{
				return this->find(key) != nullptr;
			}

			/**
			 * @brief Calls function(key, value) for every entry, in slot order (deterministic, see the class note).
			 * @tparam function_t A callable taking (const key_t &, value_t &).
			 * @param function The function (taken by value, as the standard algorithms do).
			 */
			template< typename function_t >
			void
			forEach (function_t function)
			{
				for ( auto & slot : m_slots )
				{
					if ( slot.occupied )
					{
						function(std::as_const(slot.key), slot.value);
					}
				}
			}

			/**
			 * @brief Calls function(key, value) for every entry, in slot order.
			 * @tparam function_t A callable taking (const key_t &, const value_t &).
			 * @param function The function (taken by value, as the standard algorithms do).
			 */
			template< typename function_t >
			void
			forEach (function_t function) const
			{
				for ( const auto & slot : m_slots )
				{
					if ( slot.occupied )
					{
						function(slot.key, slot.value);
					}
				}
			}

			/**
			 * @brief Calls function(key, value) for the entries in slot order while it returns true.
			 * @note For a long walk that must be interruptible (a stop request checked every N entries).
			 * @tparam function_t A callable taking (const key_t &, value_t &) and returning bool (false stops).
			 * @param function The function (taken by value, as the standard algorithms do).
			 * @return bool False when the function stopped the walk.
			 */
			template< typename function_t >
			bool
			forEachUntil (function_t function)
			{
				for ( auto & slot : m_slots )
				{
					if ( slot.occupied && !function(std::as_const(slot.key), slot.value) )
					{
						return false;
					}
				}

				return true;
			}

			/**
			 * @brief Returns the number of entries.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			size () const noexcept
			{
				return m_size;
			}

			/**
			 * @brief Returns whether the map holds no entry.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			empty () const noexcept
			{
				return m_size == 0;
			}

			/**
			 * @brief Returns the number of slots (a power of two, at least twice the entries; 0 before any allocation).
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			slotCount () const noexcept
			{
				return m_slots.size();
			}

			/** @brief Removes every entry and keeps the slots (no allocation, no release). */
			void
			clear () noexcept
			{
				for ( auto & slot : m_slots )
				{
					slot = Slot{};
				}

				m_size = 0;
			}

		private:

			/** @brief slotOf()'s answer for an absent key. */
			static constexpr size_t NotFound{static_cast< size_t >(-1)};

			/**
			 * @brief Returns the slot holding a key, or NotFound.
			 * @param key The key.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			slotOf (const key_t & key) const noexcept
			{
				if ( m_size == 0 )
				{
					return NotFound;
				}

				auto index = this->home(key);

				while ( m_slots[index].occupied )
				{
					if ( m_equal(m_slots[index].key, key) )
					{
						return index;
					}

					index = (index + 1) & m_mask;
				}

				return NotFound;
			}

			/** @brief One slot of the table. */
			struct Slot final
			{
				key_t key{};
				value_t value{};
				bool occupied{false};
			};

			/**
			 * @brief Returns the slot count for a number of entries: a power of two, at least 16, at most half full.
			 * @param entries The number of entries.
			 * @return size_t
			 */
			[[nodiscard]]
			static
			size_t
			capacityFor (size_t entries) noexcept
			{
				return std::bit_ceil(std::max< size_t >(entries * 2, 16));
			}

			/**
			 * @brief Returns the first slot probed for a key.
			 * @pre The table is allocated.
			 * @param key The key.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			home (const key_t & key) const noexcept
			{
				return static_cast< size_t >(static_cast< uint64_t >(m_hash(key))) & m_mask;
			}

			/**
			 * @brief Moves every entry into a table of slotCount slots (a power of two).
			 * @param slotCount The new slot count.
			 */
			void
			rehash (size_t slotCount) noexcept
			{
				std::vector< Slot > previous(slotCount);

				previous.swap(m_slots);
				m_mask = slotCount - 1;

				for ( auto & slot : previous )
				{
					if ( !slot.occupied )
					{
						continue;
					}

					auto index = this->home(slot.key);

					while ( m_slots[index].occupied )
					{
						index = (index + 1) & m_mask;
					}

					m_slots[index] = std::move(slot);
				}
			}

			std::vector< Slot > m_slots;
			size_t m_size{0};
			size_t m_mask{0};
			/* NOTE: A const-qualified functor type (decltype of a const lambda) is stored unqualified. */
			[[no_unique_address]] std::remove_cv_t< hash_t > m_hash{};
			[[no_unique_address]] std::remove_cv_t< equal_t > m_equal{};
	};
}
