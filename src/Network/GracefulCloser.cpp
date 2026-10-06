/*
 * src/Network/GracefulCloser.cpp
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

#include "GracefulCloser.hpp"

/* STL inclusions. */
#include <algorithm>

namespace EmEn::Base::Network
{
	/** @brief One socket being closed: FIN sent, the client's late bytes read and discarded, then closed. */
	class GracefulCloser::Lingering final : public std::enable_shared_from_this< Lingering >
	{
		public:

			Lingering (std::shared_ptr< asio::ip::tcp::socket > socket, size_t maxDrainBytes) noexcept
				: m_socket{std::move(socket)},
				m_timer{m_socket->get_executor()},
				m_maxDrainBytes{maxDrainBytes}
			{

			}

			/**
			 * @brief Sends the FIN, arms the deadline and starts draining.
			 * @param timeout The deadline.
			 * @return void
			 */
			void
			start (std::chrono::milliseconds timeout) noexcept
			{
				asio::error_code ec;
				m_socket->shutdown(asio::ip::tcp::socket::shutdown_send, ec);

				/* The client is already gone (or reset): nothing to wait for. */
				if ( ec )
				{
					this->finish();

					return;
				}

				m_timer.expires_after(timeout);
				m_timer.async_wait([self = this->shared_from_this()] (const asio::error_code & /*timerError*/) {
					/* Expired or cancelled by finish(): closing twice is a no-op. */
					self->finish();
				});

				this->drain();
			}

			/**
			 * @brief Closes the socket now.
			 * @return void
			 */
			void
			finish () noexcept
			{
				if ( m_finished )
				{
					return;
				}

				m_finished = true;

				asio::error_code ec;
				m_timer.cancel();
				m_socket->close(ec);
			}

		private:

			void
			drain () noexcept
			{
				m_socket->async_read_some(asio::buffer(m_scratch), [self = this->shared_from_this()] (const asio::error_code & ec, std::size_t bytes) {
					if ( self->m_finished )
					{
						return;
					}

					self->m_drainedBytes += bytes;

					/* EOF: the client closed, the FIN exchange is complete. Any error, or the byte budget spent: close now. */
					if ( ec || self->m_drainedBytes > self->m_maxDrainBytes )
					{
						self->finish();

						return;
					}

					self->drain();
				});
			}

			std::shared_ptr< asio::ip::tcp::socket > m_socket;
			asio::steady_timer m_timer;
			std::array< char, 4096 > m_scratch{};
			size_t m_maxDrainBytes;
			size_t m_drainedBytes{0};
			bool m_finished{false};
	};

	GracefulCloser::GracefulCloser (size_t maxLingering, std::chrono::milliseconds timeout, size_t maxDrainBytes) noexcept
		: m_timeout{timeout},
		m_maxLingering{maxLingering},
		m_maxDrainBytes{maxDrainBytes}
	{

	}

	void
	GracefulCloser::close (asio::ip::tcp::socket socket) noexcept
	{
		this->close(std::make_shared< asio::ip::tcp::socket >(std::move(socket)));
	}

	void
	GracefulCloser::close (const std::shared_ptr< asio::ip::tcp::socket > & socket) noexcept
	{
		if ( socket == nullptr || !socket->is_open() )
		{
			return;
		}

		this->prune();

		/* Beyond the cap (a flood of refusals): close at once, a RST is the accepted price. */
		if ( m_lingering.size() >= m_maxLingering )
		{
			asio::error_code ec;
			socket->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
			socket->close(ec);

			return;
		}

		auto lingering = std::make_shared< Lingering >(socket, m_maxDrainBytes);

		m_lingering.emplace_back(lingering);

		lingering->start(m_timeout);
	}

	void
	GracefulCloser::abortAll () noexcept
	{
		for ( const auto & weak : m_lingering )
		{
			if ( const auto lingering = weak.lock(); lingering != nullptr )
			{
				lingering->finish();
			}
		}

		m_lingering.clear();
	}

	size_t
	GracefulCloser::lingering () noexcept
	{
		this->prune();

		return m_lingering.size();
	}

	void
	GracefulCloser::prune () noexcept
	{
		std::erase_if(m_lingering, [] (const std::weak_ptr< Lingering > & weak) {
			return weak.expired();
		});
	}
}
