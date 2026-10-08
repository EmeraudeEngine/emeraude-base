/*
 * src/Testing/test_GameToolsCardDeck.cpp
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

/* Local inclusions. */
#include "GameTools/CardDeck.hpp"

using namespace EmEn::Base::GameTools;

namespace
{
	/** @brief A value outside CardDeck::Where (valid for its uint8_t underlying type, so no UB in the cast). */
	constexpr auto OutOfRangeWhere{static_cast< CardDeck::Where >(99)};
}

/* Ave Robustus II warning pass (2026-10-08): pick() dereferenced a value-initialized (null) iterator for an
 * out-of-range 'where' (GCC -O2 -Wnull-dereference). It must refuse and leave everything in place. */
TEST(GameToolsCardDeck, pickOutOfRangeWhereRefused)
{
	CardDeck deck{1, 4};

	const auto & hand = deck.hands().front();

	EXPECT_FALSE(deck.pickFromCardDeck(hand, OutOfRangeWhere));
	EXPECT_EQ(deck.cardCount(), 4U);
	EXPECT_TRUE(hand->cards().empty());

	EXPECT_TRUE(deck.pickFromCardDeck(hand, CardDeck::Where::Bottom));
	EXPECT_EQ(deck.cardCount(), 3U);
	EXPECT_EQ(hand->cards().size(), 1U);
}

/* insert() removed the card from the hand BEFORE its switch: an out-of-range 'where' lost the card and
 * still answered true. */
TEST(GameToolsCardDeck, releaseOutOfRangeWhereKeepsTheCard)
{
	CardDeck deck{1, 4};

	const auto & hand = deck.hands().front();

	ASSERT_TRUE(deck.pickFromCardDeck(hand, CardDeck::Where::Top));

	const auto card = hand->cards().front();

	EXPECT_FALSE(deck.release(hand, card, OutOfRangeWhere));
	EXPECT_EQ(hand->cards().size(), 1U);
	EXPECT_EQ(deck.cardCount(), 3U);

	EXPECT_TRUE(deck.release(hand, card, CardDeck::Where::Bottom));
	EXPECT_TRUE(hand->cards().empty());
	EXPECT_EQ(deck.cardCount(), 4U);
	EXPECT_EQ(deck.cards().back(), card);
}

/* 'Randomly' into an EMPTY pile built uniform_int_distribution{0, -1}: undefined behaviour. */
TEST(GameToolsCardDeck, discardRandomlyIntoEmptyPile)
{
	CardDeck deck{1, 4};

	const auto & hand = deck.hands().front();

	ASSERT_TRUE(deck.pickFromCardDeck(hand, CardDeck::Where::Top));
	ASSERT_EQ(deck.discardedCardCount(), 0U);

	const auto card = hand->cards().front();

	EXPECT_TRUE(deck.discard(hand, card, CardDeck::Where::Randomly));
	EXPECT_EQ(deck.discardedCardCount(), 1U);
	EXPECT_EQ(deck.discardedCards().front(), card);
	EXPECT_TRUE(hand->cards().empty());
}
