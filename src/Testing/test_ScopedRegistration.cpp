/*
 * src/Testing/test_ScopedRegistration.cpp
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

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* STL inclusions. */
#include <type_traits>
#include <utility>
#include <vector>

/* Local inclusions. */
#include "ScopedRegistration.hpp"

namespace
{
	using EmEn::Base::ScopedRegistration;

	/**
	 * @brief A registry of integers: register() returns a token whose end removes the value.
	 */
	class IntegerRegistry final
	{
		public:

			[[nodiscard]]
			ScopedRegistration
			add (int value) noexcept
			{
				m_values.push_back(value);

				return ScopedRegistration{[this, value] () {
					std::erase(m_values, value);

					++m_removals;
				}};
			}

			[[nodiscard]]
			const std::vector< int > &
			values () const noexcept
			{
				return m_values;
			}

			[[nodiscard]]
			int
			removals () const noexcept
			{
				return m_removals;
			}

		private:

			std::vector< int > m_values;
			int m_removals{0};
	};
}

static_assert(!std::is_copy_constructible_v< ScopedRegistration >, "A registration has one owner.");
static_assert(!std::is_copy_assignable_v< ScopedRegistration >, "A registration has one owner.");
static_assert(std::is_nothrow_move_constructible_v< ScopedRegistration >);
static_assert(std::is_nothrow_move_assignable_v< ScopedRegistration >);

TEST(BaseScopedRegistration, anEmptyTokenDoesNothing)
{
	ScopedRegistration token;

	EXPECT_FALSE(token.active());

	token.reset();

	EXPECT_FALSE(token.active());

	const ScopedRegistration fromAnEmptyCallable{std::function< void () >{}};

	EXPECT_FALSE(fromAnEmptyCallable.active());
}

TEST(BaseScopedRegistration, theDestructorUnregistersOnce)
{
	IntegerRegistry registry;

	{
		const auto token = registry.add(7);

		EXPECT_TRUE(token.active());
		EXPECT_EQ(registry.values(), std::vector< int >{7});
	}

	EXPECT_TRUE(registry.values().empty());
	EXPECT_EQ(registry.removals(), 1);
}

TEST(BaseScopedRegistration, resetUnregistersNowAndOnlyOnce)
{
	IntegerRegistry registry;

	{
		auto token = registry.add(3);

		token.reset();

		EXPECT_FALSE(token.active());
		EXPECT_TRUE(registry.values().empty());

		token.reset();
	}

	EXPECT_EQ(registry.removals(), 1);
}

TEST(BaseScopedRegistration, aMoveTransfersTheRegistration)
{
	IntegerRegistry registry;

	{
		auto first = registry.add(1);
		auto second = std::move(first);

		/* cppcheck-suppress accessMoved ; the test asserts the moved-from state, part of the documented contract. */
		EXPECT_FALSE(first.active());
		EXPECT_TRUE(second.active());

		/* The moved-from token ends here without unregistering. */
		{
			/* cppcheck-suppress accessMoved ; the test asserts the moved-from state, part of the documented contract. */
			[[maybe_unused]] auto drop = std::move(first);
		}

		EXPECT_EQ(registry.values(), std::vector< int >{1});
		EXPECT_EQ(registry.removals(), 0);
	}

	EXPECT_TRUE(registry.values().empty());
	EXPECT_EQ(registry.removals(), 1);
}

TEST(BaseScopedRegistration, aMoveAssignmentUndoesTheReplacedRegistration)
{
	IntegerRegistry registry;

	auto token = registry.add(1);
	auto other = registry.add(2);

	/* cppcheck-suppress redundantInitialization ; the first registration is what the move assignment must undo. */
	token = std::move(other);

	EXPECT_EQ(registry.values(), std::vector< int >{2});
	EXPECT_EQ(registry.removals(), 1);
	/* cppcheck-suppress accessMoved ; the test asserts the moved-from state, part of the documented contract. */
	EXPECT_FALSE(other.active());
	EXPECT_TRUE(token.active());

	token.reset();

	EXPECT_TRUE(registry.values().empty());
	EXPECT_EQ(registry.removals(), 2);
}

TEST(BaseScopedRegistration, aSelfMoveAssignmentKeepsTheRegistration)
{
	IntegerRegistry registry;

	auto token = registry.add(5);
	auto & alias = token;

	token = std::move(alias);

	EXPECT_TRUE(token.active());
	EXPECT_EQ(registry.values(), std::vector< int >{5});
	EXPECT_EQ(registry.removals(), 0);
}

TEST(BaseScopedRegistration, anUndoThatResetsItsOwnTokenAgainFindsItEmpty)
{
	int calls = 0;
	ScopedRegistration token;

	token = ScopedRegistration{[&token, &calls] () {
		++calls;

		token.reset();
	}};

	token.reset();

	EXPECT_EQ(calls, 1);
	EXPECT_FALSE(token.active());
}
