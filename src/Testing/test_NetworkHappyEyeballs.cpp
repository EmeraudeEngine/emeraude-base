/*
 * src/Testing/test_NetworkHappyEyeballs.cpp
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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

/* Local inclusions. */
#include "Network/HappyEyeballs.hpp"

using namespace EmEn::Base;

namespace
{
	using Clock = std::chrono::steady_clock;

	asio::ip::tcp::endpoint
	makeEndpoint (const char * address, uint16_t port) noexcept
	{
		asio::error_code error;

		return {asio::ip::make_address(address, error), port};
	}

	/**
	 * @brief A loopback listener that never accepts: a connect to it completes in the kernel (accept queue).
	 */
	class SilentListener final
	{
		public:

			explicit
			SilentListener (int backlog = 4) noexcept
			{
				asio::error_code error;

				m_acceptor.open(asio::ip::tcp::v4(), error);

				if ( !error )
				{
					m_acceptor.bind(makeEndpoint("127.0.0.1", 0), error);
				}

				if ( !error )
				{
					m_acceptor.listen(backlog, error);
				}

				if ( !error )
				{
					m_endpoint = m_acceptor.local_endpoint(error);
				}

				m_ready = !error;
			}

			[[nodiscard]]
			bool
			ready () const noexcept
			{
				return m_ready;
			}

			[[nodiscard]]
			const asio::ip::tcp::endpoint &
			endpoint () const noexcept
			{
				return m_endpoint;
			}

		private:

			asio::io_context m_ioContext;
			asio::ip::tcp::acceptor m_acceptor{m_ioContext};
			asio::ip::tcp::endpoint m_endpoint;
			bool m_ready{false};
	};

	/**
	 * @brief A loopback endpoint whose connect does not complete: a listener whose accept queue is full drops (Linux,
	 * macOS) or slowly refuses (Windows retries the SYN for ~2 s) every new connection.
	 * @note Filled with connections until one does not complete within ProbeWait.
	 */
	class UnansweredEndpoint final
	{
		public:

			static constexpr size_t MaxFillers{512};
			static constexpr std::chrono::milliseconds ProbeWait{200};

			UnansweredEndpoint () noexcept
			{
				if ( !m_listener.ready() )
				{
					return;
				}

				/* ⚠️ Reserved once: a socket must not move while its connect is pending. */
				m_fillers.reserve(MaxFillers);

				while ( m_fillers.size() < MaxFillers )
				{
					auto & filler = m_fillers.emplace_back(m_ioContext);

					asio::error_code connectError{asio::error::would_block};

					filler.async_connect(m_listener.endpoint(), [&connectError] (const asio::error_code & error) {
						connectError = error;
					});

					m_ioContext.restart();
					m_ioContext.run_for(ProbeWait);

					if ( connectError == asio::error::would_block )
					{
						/* This one hangs: the queue is full. Close the probe and drain its handler. */
						asio::error_code closeError;

						filler.close(closeError);
						m_ioContext.run();

						m_saturated = true;

						return;
					}

					if ( connectError )
					{
						/* Refused at once: the listener does not behave as expected here. */
						return;
					}
				}
			}

			[[nodiscard]]
			bool
			saturated () const noexcept
			{
				return m_saturated;
			}

			[[nodiscard]]
			const asio::ip::tcp::endpoint &
			endpoint () const noexcept
			{
				return m_listener.endpoint();
			}

		private:

			SilentListener m_listener{0};
			asio::io_context m_ioContext;
			std::vector< asio::ip::tcp::socket > m_fillers;
			bool m_saturated{false};
	};

	/**
	 * @brief A loopback port nothing listens on (a listener bound then closed).
	 * @return asio::ip::tcp::endpoint
	 */
	asio::ip::tcp::endpoint
	refusingEndpoint () noexcept
	{
		asio::io_context ioContext;
		asio::ip::tcp::acceptor acceptor{ioContext};
		asio::error_code error;

		acceptor.open(asio::ip::tcp::v4(), error);
		acceptor.bind(makeEndpoint("127.0.0.1", 0), error);

		const auto endpoint = acceptor.local_endpoint(error);

		acceptor.close(error);

		return endpoint;
	}

	[[nodiscard]]
	int64_t
	millisecondsSince (Clock::time_point start) noexcept
	{
		return std::chrono::duration_cast< std::chrono::milliseconds >(Clock::now() - start).count();
	}
}

TEST(NetworkHappyEyeballs, interleaveAlternatesTheFamiliesFromTheFirstOne)
{
	const std::array endpoints{
		makeEndpoint("2001:db8::1", 443),
		makeEndpoint("2001:db8::2", 443),
		makeEndpoint("2001:db8::3", 443),
		makeEndpoint("192.0.2.1", 443),
		makeEndpoint("192.0.2.2", 443)
	};

	const auto ordered = Network::interleaveAddressFamilies(endpoints);

	ASSERT_EQ(ordered.size(), 5U);
	EXPECT_EQ(ordered[0], endpoints[0]);
	EXPECT_EQ(ordered[1], endpoints[3]);
	EXPECT_EQ(ordered[2], endpoints[1]);
	EXPECT_EQ(ordered[3], endpoints[4]);
	EXPECT_EQ(ordered[4], endpoints[2]);
}

TEST(NetworkHappyEyeballs, interleaveStartsWithIPv4WhenTheResolverPutsItFirst)
{
	const std::array endpoints{
		makeEndpoint("192.0.2.1", 80),
		makeEndpoint("2001:db8::1", 80),
		makeEndpoint("2001:db8::2", 80)
	};

	const auto ordered = Network::interleaveAddressFamilies(endpoints);

	ASSERT_EQ(ordered.size(), 3U);
	EXPECT_EQ(ordered[0], endpoints[0]);
	EXPECT_EQ(ordered[1], endpoints[1]);
	EXPECT_EQ(ordered[2], endpoints[2]);
}

TEST(NetworkHappyEyeballs, interleaveKeepsASingleFamilyAndAnEmptyListAsTheyAre)
{
	const std::array endpoints{
		makeEndpoint("192.0.2.2", 80),
		makeEndpoint("192.0.2.1", 80)
	};

	const auto ordered = Network::interleaveAddressFamilies(endpoints);

	ASSERT_EQ(ordered.size(), 2U);
	EXPECT_EQ(ordered[0], endpoints[0]);
	EXPECT_EQ(ordered[1], endpoints[1]);

	EXPECT_TRUE(Network::interleaveAddressFamilies({}).empty());
}

TEST(NetworkHappyEyeballs, refusesAnEmptyListAndAnOpenSocket)
{
	asio::io_context ioContext;
	asio::ip::tcp::socket socket{ioContext};

	EXPECT_EQ(Network::connectFirstReachable(ioContext, {}, socket, std::chrono::milliseconds{1000}), asio::error::invalid_argument);

	const SilentListener listener;
	ASSERT_TRUE(listener.ready());

	asio::error_code error;
	socket.open(asio::ip::tcp::v4(), error);
	ASSERT_FALSE(error);

	const std::array endpoints{listener.endpoint()};

	EXPECT_EQ(Network::connectFirstReachable(ioContext, endpoints, socket, std::chrono::milliseconds{1000}), asio::error::invalid_argument);
}

TEST(NetworkHappyEyeballs, aRefusedEndpointGivesWayAtOnce)
{
	const SilentListener listener;
	ASSERT_TRUE(listener.ready());

	asio::io_context ioContext;
	asio::ip::tcp::socket socket{ioContext};

	const std::array endpoints{refusingEndpoint(), listener.endpoint()};

	const auto error = Network::connectFirstReachable(ioContext, endpoints, socket, std::chrono::milliseconds{10000});

	ASSERT_FALSE(error) << error.message();

	asio::error_code peerError;
	EXPECT_EQ(socket.remote_endpoint(peerError), listener.endpoint());
}

TEST(NetworkHappyEyeballs, everyEndpointRefusedReturnsTheLastError)
{
	asio::io_context ioContext;
	asio::ip::tcp::socket socket{ioContext};

	const std::array endpoints{refusingEndpoint(), refusingEndpoint()};

	const auto error = Network::connectFirstReachable(ioContext, endpoints, socket, std::chrono::milliseconds{10000});

	EXPECT_EQ(error, asio::error::connection_refused) << error.message();
	EXPECT_FALSE(socket.is_open());
}

TEST(NetworkHappyEyeballs, anUnansweredEndpointGivesWayAfterTheAttemptDelay)
{
	const UnansweredEndpoint unanswered;

	if ( !unanswered.saturated() )
	{
		GTEST_SKIP() << "This system does not leave a connect to a full listen queue unanswered.";
	}

	const SilentListener listener;
	ASSERT_TRUE(listener.ready());

	asio::io_context ioContext;
	asio::ip::tcp::socket socket{ioContext};

	const std::array endpoints{unanswered.endpoint(), listener.endpoint()};

	const auto start = Clock::now();
	const auto error = Network::connectFirstReachable(ioContext, endpoints, socket, std::chrono::milliseconds{10000});
	const auto elapsed = millisecondsSince(start);

	ASSERT_FALSE(error) << error.message();

	asio::error_code peerError;
	EXPECT_EQ(socket.remote_endpoint(peerError), listener.endpoint());

	/* The second attempt started after the delay (250 ms), not after the first one's failure (seconds). */
	EXPECT_GE(elapsed, Network::ConnectionAttemptDelay.count() - 10);
	EXPECT_LT(elapsed, 1500);
}

TEST(NetworkHappyEyeballs, theTimeoutBoundsTheWholeRace)
{
	const UnansweredEndpoint unanswered;

	if ( !unanswered.saturated() )
	{
		GTEST_SKIP() << "This system does not leave a connect to a full listen queue unanswered.";
	}

	asio::io_context ioContext;
	asio::ip::tcp::socket socket{ioContext};

	const std::array endpoints{unanswered.endpoint(), unanswered.endpoint()};

	const auto start = Clock::now();
	const auto error = Network::connectFirstReachable(ioContext, endpoints, socket, std::chrono::milliseconds{400});
	const auto elapsed = millisecondsSince(start);

	EXPECT_EQ(error, asio::error::timed_out) << error.message();
	EXPECT_FALSE(socket.is_open());
	EXPECT_LT(elapsed, 1500);
}
