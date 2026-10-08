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

/* STL inclusions. */
#include <algorithm>
#include <cstddef>

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

/* 2026-10-08 (owner decision): inserting Randomly may put the card in ANY slot, the one after the last card included.
 * The draw used to be in [0, size - 1]: the end slot was never chosen. One card left in the deck, one card released
 * Randomly: over 200 rounds both slots must appear (a miss has a probability of 2^-199). */
TEST(GameToolsCardDeck, releaseRandomlyReachesEverySlotTheEndIncluded)
{
	bool sawFront = false;
	bool sawEnd = false;

	for ( int round = 0; round < 200 && !(sawFront && sawEnd); ++round )
	{
		CardDeck deck{1, 2};

		const auto & hand = deck.hands().front();

		ASSERT_TRUE(deck.pickFromCardDeck(hand, CardDeck::Where::Top));

		const auto card = hand->cards().front();

		ASSERT_TRUE(deck.release(hand, card, CardDeck::Where::Randomly));
		ASSERT_EQ(deck.cardCount(), 2U);

		sawFront = sawFront || deck.cards().front() == card;
		sawEnd = sawEnd || deck.cards().back() == card;
	}

	EXPECT_TRUE(sawFront);
	EXPECT_TRUE(sawEnd);
}

/* 2026-10-08: the shuffles go through PortableRandom::shuffle(); a shuffle keeps every card exactly once. */
TEST(GameToolsCardDeck, shufflesKeepEveryCardOnce)
{
	CardDeck deck{1, 52};

	deck.shuffleCardDeck();

	auto cards = deck.cards();

	std::ranges::sort(cards);

	for ( size_t index = 0; index < cards.size(); ++index )
	{
		EXPECT_EQ(cards[index], index);
	}

	const auto & hand = deck.hands().front();

	for ( int pick = 0; pick < 10; ++pick )
	{
		ASSERT_TRUE(deck.pickFromCardDeck(hand, CardDeck::Where::Randomly));
	}

	hand->shuffle();

	EXPECT_EQ(hand->cardCount(), 10U);
	EXPECT_EQ(deck.cardCount(), 42U);
}
