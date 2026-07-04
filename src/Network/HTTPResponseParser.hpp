/*
 * src/Network/HTTPResponseParser.hpp
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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

/* Local inclusions. */
#include "HTTPResponse.hpp"

namespace EmEn::Base::Network
{
	/**
	 * @brief Hardening limits for the HTTP/1.1 response parser.
	 * @note The header section and chunk-size line caps defend against
	 * memory-exhaustion from a hostile peer; the body cap is the caller's
	 * business (a download legitimately streams gigabytes, an API call does not).
	 */
	struct HTTPResponseParserLimits final
	{
		size_t maxHeaderSectionSize{65536};
		size_t maxChunkSizeLineLength{1024};
		size_t maxTrailerSectionSize{8192};
		size_t maxInterimResponses{4};
		uint64_t maxBodySize{std::numeric_limits< uint64_t >::max()};
	};

	/**
	 * @brief Incremental HTTP/1.1 response parser (the response side of the HTTP codec).
	 * @note This is an UNTRUSTED-INPUT boundary: every length is bounds-checked, every
	 * count capped, malformed input yields Result::Failure — never a crash (A.3 doctrine).
	 * @note Feed-based: the client loop pumps raw transport bytes into feed() and drains
	 * body() between calls (streaming). The parser is transport- and protocol-agnostic
	 * from the caller's point of view — chunked decoding, Content-Length framing and
	 * keep-alive semantics stay internal (h2-ready API, owner-ruled 2026-07-04).
	 * @note Body framing (RFC 9112 §6.3): Transfer-Encoding chunked > Content-Length >
	 * read-until-close. Interim 1xx responses are skipped (101 Upgrade is refused).
	 * Single-use: one response per instance.
	 */
	class HTTPResponseParser final
	{
		public:

			/** @brief Parsing outcome after each feed. */
			enum class Result : uint8_t
			{
				NeedMoreData,
				Complete,
				Failure
			};

			/**
			 * @brief Constructs an HTTP/1.1 response parser.
			 * @param limits The hardening limits. Default: 64 KiB headers, unbounded body.
			 */
			explicit
			HTTPResponseParser (const HTTPResponseParserLimits & limits = {}) noexcept
				: m_limits(limits)
			{

			}

			/**
			 * @brief Declares that the awaited response has no body by protocol.
			 * @note MUST be called before feeding when the request was a HEAD — the
			 * response then ends at the header section whatever Content-Length claims
			 * (RFC 9112 §6.3). 204/304 statuses are handled automatically.
			 * @return void
			 */
			void
			expectBodilessResponse () noexcept
			{
				m_bodilessExpected = true;
			}

			/**
			 * @brief Feeds raw transport bytes into the parser.
			 * @param data A pointer to the received bytes.
			 * @param size The number of received bytes.
			 * @return Result
			 */
			[[nodiscard]]
			Result feed (const char * data, size_t size) noexcept;

			/**
			 * @brief Signals the end of the transport stream (peer closed).
			 * @note Completes a read-until-close body; any other unfinished stage
			 * means the response was truncated and fails.
			 * @return Result
			 */
			[[nodiscard]]
			Result finish () noexcept;

			/**
			 * @brief Returns whether the status line and headers are fully parsed.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			headersComplete () const noexcept
			{
				return m_stage != Stage::StatusLineAndHeaders && m_stage != Stage::Failed;
			}

			/**
			 * @brief Returns the parsed response (status + headers). Valid once headersComplete().
			 * @return const HTTPResponse &
			 */
			[[nodiscard]]
			const HTTPResponse &
			response () const noexcept
			{
				return m_response;
			}

			/**
			 * @brief Returns the decoded body bytes accumulated so far.
			 * @note A streaming consumer may take/clear this buffer between feeds;
			 * bodyBytesDecoded() keeps the running total.
			 * @return std::string &
			 */
			[[nodiscard]]
			std::string &
			body () noexcept
			{
				return m_body;
			}

			/**
			 * @brief Returns the total number of decoded body bytes (survives body() draining).
			 * @return uint64_t
			 */
			[[nodiscard]]
			uint64_t
			bodyBytesDecoded () const noexcept
			{
				return m_bodyBytesDecoded;
			}

		private:

			/** @brief Internal parsing stage. */
			enum class Stage : uint8_t
			{
				StatusLineAndHeaders,
				FixedBody,
				ChunkedSize,
				ChunkedData,
				ChunkedDataTerminator,
				ChunkedTrailers,
				UntilClose,
				Complete,
				Failed
			};

			/**
			 * @brief Parses the completed header section and selects the body framing.
			 * @param headerSection The raw status line + headers (without the blank line).
			 * @return bool
			 */
			[[nodiscard]]
			bool onHeaderSectionComplete (const std::string & headerSection) noexcept;

			/**
			 * @brief Appends decoded body bytes, enforcing the body cap.
			 * @param data A pointer to the decoded bytes.
			 * @param size The number of decoded bytes.
			 * @return bool
			 */
			[[nodiscard]]
			bool appendBody (const char * data, size_t size) noexcept;

			/**
			 * @brief Fails the parsing with a logged reason.
			 * @param message The failure reason.
			 * @return Result Always Result::Failure.
			 */
			Result fail (const char * message) noexcept;

			HTTPResponseParserLimits m_limits;
			HTTPResponse m_response;
			std::string m_headerBuffer;
			std::string m_lineBuffer;
			std::string m_body;
			uint64_t m_bodyRemaining{0};
			uint64_t m_bodyBytesDecoded{0};
			size_t m_interimResponseCount{0};
			Stage m_stage{Stage::StatusLineAndHeaders};
			bool m_bodilessExpected{false};
	};
}
