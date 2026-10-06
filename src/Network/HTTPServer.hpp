/*
 * src/Network/HTTPServer.hpp
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
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>

/* Third-party inclusions. */
#include "asio.hpp"
#include "Network/asio_throw_exception.hpp"

/* Local inclusions for usages. */
#include "Network/GracefulCloser.hpp"

namespace EmEn::Base::Network
{
	class HTTPServer;
	class HTTPServerConnection;

	/**
	 * @brief What an HTTPServer is built with.
	 */
	struct HTTPServerOptions final
	{
		/** @brief The bind address (an IP literal). A non-loopback one needs a bearer token. */
		std::string address{"127.0.0.1"};

		/**
		 * @brief The token every request must carry as `Authorization: Bearer <token>`; empty = none.
		 * @note Mandatory on a non-loopback address: start() refuses without one.
		 */
		std::string bearerToken;

		/** @brief The name used in the log lines ("MCP server", "Resource sharing server"). */
		std::string name{"HTTP server"};

		/** @brief Maximum size of the request line and headers. */
		size_t maxHeaderBytes{16384};

		/** @brief Maximum size of a request body. */
		size_t maxBodyBytes{1048576};

		/** @brief Maximum number of simultaneous connections (streams included). */
		size_t maxConnections{16};

		/** @brief Time allowed to receive a whole request once its first byte arrived, and to write one chunk of
		 * a file, in seconds. */
		uint32_t requestTimeoutSeconds{30};

		/** @brief Time an idle keep-alive connection is kept open, in seconds. */
		uint32_t idleTimeoutSeconds{120};

		/** @brief The TCP port; 0 = an ephemeral one, read back with HTTPServer::port(). */
		uint16_t port{0};
	};

	/**
	 * @brief A parsed request, as the handler receives it.
	 */
	struct HTTPServerRequest final
	{
		std::string method;
		std::string target;
		std::string version;
		/** @brief Header fields, names lowercased. A duplicated framing or identity field was refused (400). */
		std::map< std::string, std::string > headers;
		std::string body;

		/**
		 * @brief Returns a header value, or an empty string.
		 * @param lowercaseName The header name, lowercase.
		 * @return std::string
		 */
		[[nodiscard]]
		std::string header (const std::string & lowercaseName) const noexcept;

		/**
		 * @brief Returns the target without its query string.
		 * @return std::string
		 */
		[[nodiscard]]
		std::string path () const noexcept;
	};

	/**
	 * @brief Returns the reason phrase of a status code.
	 * @param status The status code.
	 * @return const char *
	 */
	[[nodiscard]]
	const char * reasonPhrase (int status) noexcept;

	/**
	 * @brief Parses a single `Range: bytes=…` header against a resource size (RFC 9110 § 14.1.2).
	 * @note Multiple ranges, another unit, or a malformed value are IGNORED (std::nullopt): RFC 9110 lets a server
	 * answer the whole representation then. An unsatisfiable range is reported through 'unsatisfiable'.
	 * @param value The header value.
	 * @param size The resource size in bytes.
	 * @param unsatisfiable Set to true when the range is valid but lies outside the resource (answer 416).
	 * @return std::optional< std::pair< uint64_t, uint64_t > > The first byte and the length.
	 */
	[[nodiscard]]
	std::optional< std::pair< uint64_t, uint64_t > > parseByteRange (std::string_view value, uint64_t size, bool & unsatisfiable) noexcept;

	/**
	 * @brief One connection of an HTTPServer, handed to the request handler.
	 * @note NETWORK THREAD ONLY: every method must be called from the handler or from a task given to
	 * HTTPServer::post(). Hold it as a std::weak_ptr to answer later. Each request gets exactly ONE answer: a
	 * respond…() call, or startStream().
	 */
	class HTTPServerConnection final : public std::enable_shared_from_this< HTTPServerConnection >
	{
		public:

			/**
			 * @brief Constructs a connection (HTTPServer only).
			 * @param server The owning server.
			 * @param socket The accepted socket [std::move].
			 * @param id The connection's unique number.
			 */
			HTTPServerConnection (HTTPServer & server, asio::ip::tcp::socket socket, uint64_t id) noexcept;

			HTTPServerConnection (const HTTPServerConnection & copy) noexcept = delete;
			HTTPServerConnection (HTTPServerConnection && copy) noexcept = delete;
			HTTPServerConnection & operator= (const HTTPServerConnection & copy) noexcept = delete;
			HTTPServerConnection & operator= (HTTPServerConnection && copy) noexcept = delete;

			/**
			 * @brief Destructs the connection.
			 */
			~HTTPServerConnection () = default;

			/**
			 * @brief Returns the connection's unique number (HTTPServer's closed handler receives it).
			 * @return uint64_t
			 */
			[[nodiscard]]
			uint64_t
			id () const noexcept
			{
				return m_id;
			}

			/**
			 * @brief Returns the request in flight.
			 * @return const HTTPServerRequest &
			 */
			[[nodiscard]]
			const HTTPServerRequest &
			request () const noexcept
			{
				return m_request;
			}

			/**
			 * @brief Returns whether this connection was turned into a stream (startStream()).
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isStreaming () const noexcept
			{
				return m_streaming;
			}

			/**
			 * @brief Answers the request in flight with a body, then reads the next one (or closes). A HEAD request
			 * gets the headers only.
			 * @param status The HTTP status.
			 * @param contentType The media type.
			 * @param body The body.
			 * @param extraHeaders Complete header lines, each ending with CRLF.
			 * @return void
			 */
			void respond (int status, std::string_view contentType, std::string_view body, std::string_view extraHeaders = {}) noexcept;

			/**
			 * @brief Answers the request in flight with no body.
			 * @param status The HTTP status.
			 * @param extraHeaders Complete header lines, each ending with CRLF.
			 * @return void
			 */
			void respondEmpty (int status, std::string_view extraHeaders = {}) noexcept;

			/**
			 * @brief Answers the request in flight with a file, streamed in chunks (never loaded whole). Honours a
			 * single `Range` (206, or 416) unless an `If-Range` is present, and HEAD.
			 * @note The caller has CONFINED the path: this method serves whatever it is given. Not found or unreadable
			 * = 404. The chunks are read on the network thread (a blocking local read).
			 * @param filepath The file.
			 * @param contentType The media type.
			 * @param extraHeaders Complete header lines, each ending with CRLF.
			 * @return void
			 */
			void respondFile (const std::filesystem::path & filepath, std::string_view contentType, std::string_view extraHeaders = {}) noexcept;

			/**
			 * @brief Turns this connection into a server-to-client stream: a 200 head, no length, kept open.
			 * @param contentType The media type ("text/event-stream").
			 * @param extraHeaders Complete header lines, each ending with CRLF.
			 * @param keepAlivePayload Bytes written after keepAliveSeconds of silence (an SSE comment); empty = none.
			 * @param keepAliveSeconds The keep-alive period.
			 * @return void
			 */
			void startStream (std::string_view contentType, std::string_view extraHeaders, std::string keepAlivePayload, uint32_t keepAliveSeconds) noexcept;

			/**
			 * @brief Sends bytes on a stream.
			 * @param bytes The bytes [std::move].
			 * @return void
			 */
			void sendStream (std::string bytes) noexcept;

			/**
			 * @brief Writes bytes synchronously, then closes. For the shutdown handler only (the loop is stopping).
			 * @param bytes The bytes.
			 * @return void
			 */
			void writeNowAndClose (std::string_view bytes) noexcept;

			/**
			 * @brief Closes the socket and forgets the connection.
			 * @return void
			 */
			void close () noexcept;

			/**
			 * @brief Starts reading the first request (HTTPServer only).
			 * @return void
			 */
			void start () noexcept;

		private:

			/** @brief What the single timer is doing. */
			enum class TimerRole : uint8_t
			{
				RequestDeadline,
				Idle,
				KeepAlive,
				WriteDeadline
			};

			/** @brief A file being streamed as the current response. */
			struct FileBody
			{
				std::ifstream file;
				uint64_t remaining{0};
			};

			void armTimer (uint32_t seconds, TimerRole role) noexcept;
			void readRequest () noexcept;
			void readBody (size_t contentLength) noexcept;
			void handleRequest () noexcept;
			void watchForClose () noexcept;
			/**
			 * @brief Closes after a final answer: the socket goes to the server's GracefulCloser (FIN, bounded drain), so a
			 * client still sending (a refused body, a late request) does not get a RST that would discard the answer.
			 * @return void
			 */
			void closeGracefully () noexcept;
			void finishWith (std::string response) noexcept;
			void write (std::string bytes) noexcept;
			void writeNext () noexcept;
			[[nodiscard]] static std::string statusLine (int status) noexcept;
			[[nodiscard]] std::string connectionHeader () const noexcept;

			HTTPServer & m_server;
			asio::ip::tcp::socket m_socket;
			asio::steady_timer m_timer;
			asio::streambuf m_buffer;
			std::array< char, 512 > m_scratch{};
			HTTPServerRequest m_request;
			std::deque< std::string > m_writeQueue;
			std::optional< FileBody > m_fileBody;
			std::string m_keepAlivePayload;
			uint64_t m_id;
			uint32_t m_keepAliveSeconds{0};
			TimerRole m_timerRole{TimerRole::Idle};
			bool m_keepAlive{true};
			bool m_writing{false};
			bool m_closeAfterFlush{false};
			bool m_readAfterFlush{false};
			bool m_streaming{false};
			bool m_closed{false};
	};

	/**
	 * @brief A small HTTP/1.1 server on its own network thread: the engine's MCP server and resource sharing.
	 * @note Threading: every socket operation happens on the server's network thread; the handlers run there.
	 * Work for another thread is queued by the user, and the answer comes back through post().
	 * @note Security, checked before the handler runs: a non-loopback binding refuses to start without a bearer
	 * token; a loopback binding accepts only its own names in `Host` (DNS rebinding); a present `Origin` must be
	 * this server's own (a web page — a browser, CEF — can reach 127.0.0.1); the bearer is compared in constant
	 * time. Sizes, connections and idle time are bounded; duplicated framing headers, `Transfer-Encoding` and a
	 * POST without `Content-Length` are refused (request smuggling, RFC 9112 § 6.3).
	 */
	class HTTPServer final
	{
		public:

			/** @brief Receives each request that passed the checks. Network thread. */
			using RequestHandler = std::function< void (const std::shared_ptr< HTTPServerConnection > &) >;

			/** @brief Gives a stream its last words at shutdown (writeNowAndClose()). Network thread. */
			using StreamShutdownHandler = std::function< void (HTTPServerConnection &) >;

			/** @brief Told a connection closed, by its id. Network thread. */
			using ClosedHandler = std::function< void (uint64_t) >;

			/**
			 * @brief Constructs a stopped server.
			 * @param options The options.
			 */
			explicit HTTPServer (HTTPServerOptions options) noexcept;

			HTTPServer (const HTTPServer & copy) noexcept = delete;
			HTTPServer (HTTPServer && copy) noexcept = delete;
			HTTPServer & operator= (const HTTPServer & copy) noexcept = delete;
			HTTPServer & operator= (HTTPServer && copy) noexcept = delete;

			/**
			 * @brief Stops the server (stop()).
			 */
			~HTTPServer ();

			/**
			 * @brief Binds, listens and starts the network thread.
			 * @param onRequest The request handler.
			 * @param onStreamShutdown Optional: the last words of each stream at shutdown.
			 * @param onClosed Optional: told each closed connection's id.
			 * @return bool False (and logged) on an invalid address, a missing token, or a port that cannot be bound.
			 */
			[[nodiscard]]
			bool start (RequestHandler onRequest, StreamShutdownHandler onStreamShutdown = {}, ClosedHandler onClosed = {}) noexcept;

			/**
			 * @brief Stops the server: streams get their last words, every socket closes on the network thread, then
			 * the thread joins. Bounded: a stuck peer cannot hold it more than 3 seconds. Idempotent.
			 * @note Never from the network thread.
			 * @return void
			 */
			void stop () noexcept;

			/**
			 * @brief Returns whether the server accepts connections.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isRunning () const noexcept
			{
				return m_running;
			}

			/**
			 * @brief Returns the bound port (the ephemeral one when the options said 0), 0 when not running.
			 * @return uint16_t
			 */
			[[nodiscard]]
			uint16_t
			port () const noexcept
			{
				return m_running ? m_boundPort : 0;
			}

			/**
			 * @brief Returns the options.
			 * @return const HTTPServerOptions &
			 */
			[[nodiscard]]
			const HTTPServerOptions &
			options () const noexcept
			{
				return m_options;
			}

			/**
			 * @brief Returns "http://<address>:<port>" (IPv6 bracketed), empty when not running.
			 * @return std::string
			 */
			[[nodiscard]]
			std::string baseURL () const noexcept;

			/**
			 * @brief Runs a task on the network thread (an answer computed elsewhere). Dropped when not running.
			 * @note Thread-safe. The caller keeps this server alive while it may still call post().
			 * @param task The task.
			 * @return void
			 */
			void post (std::function< void () > task) noexcept;

			/**
			 * @brief Calls a function for every open connection. NETWORK THREAD ONLY.
			 * @param function The function.
			 * @return void
			 */
			void forEachConnection (const std::function< void (HTTPServerConnection &) > & function) const noexcept;

		private:

			friend class HTTPServerConnection;

			void accept () noexcept;
			void removeConnection (const std::shared_ptr< HTTPServerConnection > & connection) noexcept;
			[[nodiscard]] bool isAcceptedHost (const std::string & host) const noexcept;
			[[nodiscard]] bool isAcceptedOrigin (const std::string & origin) const noexcept;
			[[nodiscard]] bool isAuthorized (const std::string & authorization) const noexcept;

			HTTPServerOptions m_options;
			RequestHandler m_onRequest;
			StreamShutdownHandler m_onStreamShutdown;
			ClosedHandler m_onClosed;
			asio::io_context m_ioContext;
			std::optional< asio::executor_work_guard< asio::io_context::executor_type > > m_workGuard;
			std::unique_ptr< asio::ip::tcp::acceptor > m_acceptor;
			/** @brief The refusals and "Connection: close" answers end with a FIN, not a RST (network thread only). */
			GracefulCloser m_gracefulCloser;
			std::thread m_networkThread;
			/** @brief The open connections, touched on the network thread only. */
			std::set< std::shared_ptr< HTTPServerConnection > > m_connections;
			uint64_t m_nextConnectionId{1};
			uint16_t m_boundPort{0};
			/** @brief Read from any thread (post(), isRunning()); written by start() / stop() only. */
			std::atomic< bool > m_running{false};
			bool m_loopback{false};
	};
}
