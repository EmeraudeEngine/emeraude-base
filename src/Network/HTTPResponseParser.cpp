/*
 * src/Network/HTTPResponseParser.cpp
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

#include "HTTPResponseParser.hpp"

/* Project configuration. */
#include "emeraude_base_config.hpp"

/* STL inclusions. */
#include <algorithm>
#include <charconv>
#include <string_view>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "String.hpp"

namespace EmEn::Base::Network
{
	namespace
	{
		constexpr auto Tag{"Network::HTTPResponseParser"};

		/**
		 * @brief Strictly parses a decimal unsigned integer (every character a digit).
		 * @param text The text to parse.
		 * @param value The parsed value [out].
		 * @return bool
		 */
		bool
		parseDecimalStrict (std::string_view text, uint64_t & value) noexcept
		{
			if ( text.empty() )
			{
				return false;
			}

			const auto * begin = text.data();
			const auto * end = text.data() + text.size();

			const auto [pointer, errorCode] = std::from_chars(begin, end, value, 10);

			return errorCode == std::errc{} && pointer == end;
		}

		/**
		 * @brief Parses the hexadecimal size prefix of a chunk-size line (extensions ignored).
		 * @param line The chunk-size line (without CRLF).
		 * @param value The parsed value [out].
		 * @return bool
		 */
		bool
		parseChunkSize (std::string_view line, uint64_t & value) noexcept
		{
			/* Chunk extensions (';...') are legal and ignored (RFC 9112 §7.1.1). */
			const auto sizeEnd = line.find(';');
			const auto sizeText = line.substr(0, std::min(sizeEnd, line.size()));

			if ( sizeText.empty() )
			{
				return false;
			}

			const auto * begin = sizeText.data();
			const auto * end = sizeText.data() + sizeText.size();

			const auto [pointer, errorCode] = std::from_chars(begin, end, value, 16);

			return errorCode == std::errc{} && pointer == end;
		}

		/**
		 * @brief Counts case-insensitive occurrences of a header field name in a raw section.
		 * @note The header map silently drops duplicates (first wins) — but duplicated,
		 * potentially conflicting Content-Length fields MUST be rejected (RFC 9112 §6.3,
		 * request-smuggling defense). This scans the raw text the map cannot see.
		 * @param headerSection The raw header section.
		 * @param fieldName The lowercased field name, without the colon.
		 * @return size_t
		 */
		size_t
		countHeaderOccurrences (const std::string & headerSection, std::string_view fieldName) noexcept
		{
			const auto lowered = String::toLower(headerSection);

			size_t count = 0;
			size_t position = 0;

			while ( (position = lowered.find(fieldName, position)) != std::string::npos )
			{
				/* Only count real field starts: beginning of a line, followed by
				 * optional whitespace and a colon. */
				const auto atLineStart = position == 0 || lowered[position - 1] == '\n';

				auto colonPosition = position + fieldName.size();

				while ( colonPosition < lowered.size() && (lowered[colonPosition] == ' ' || lowered[colonPosition] == '\t') )
				{
					++colonPosition;
				}

				if ( atLineStart && colonPosition < lowered.size() && lowered[colonPosition] == ':' )
				{
					++count;
				}

				position += fieldName.size();
			}

			return count;
		}
	}

	HTTPResponseParser::Result
	HTTPResponseParser::fail (const char * message) noexcept
	{
		Logging::error(Tag, std::string{"parsing failed : "} + message);

		m_stage = Stage::Failed;

		return Result::Failure;
	}

	bool
	HTTPResponseParser::appendBody (const char * data, size_t size) noexcept
	{
		if ( size > m_limits.maxBodySize - m_bodyBytesDecoded )
		{
			return false;
		}

		m_body.append(data, size);
		m_bodyBytesDecoded += size;

		return true;
	}

	bool
	HTTPResponseParser::onHeaderSectionComplete (const std::string & headerSection) noexcept
	{
		m_response = HTTPResponse{};

		if ( !m_response.parse(headerSection) || !m_response.isValid() )
		{
			return false;
		}

		const auto statusCode = m_response.codeResponse();

		/* Interim responses (RFC 9110 §15.2): skip and await the final one. */
		if ( statusCode >= 100 && statusCode <= 199 )
		{
			if ( statusCode == 101 )
			{
				/* Protocol switch: out of this client's scope by design. */
				return false;
			}

			if ( ++m_interimResponseCount > m_limits.maxInterimResponses )
			{
				return false;
			}

			m_headerBuffer.clear();
			m_stage = Stage::StatusLineAndHeaders;

			return true;
		}

		/* Bodiless by protocol: HEAD request, 204 No Content, 304 Not Modified. */
		if ( m_bodilessExpected || statusCode == 204 || statusCode == 304 )
		{
			m_stage = Stage::Complete;

			return true;
		}

		/* Body framing selection (RFC 9112 §6.3) — Transfer-Encoding first: when
		 * present, any Content-Length MUST be ignored (smuggling defense). */
		const auto transferEncoding = String::toLower(String::trim(m_response.value(HTTPResponse::TransferEncoding)));

		if ( !transferEncoding.empty() )
		{
			if ( transferEncoding == "chunked" )
			{
				m_stage = Stage::ChunkedSize;
				m_lineBuffer.clear();

				return true;
			}

			/* Unknown final transfer coding: the body ends with the connection. */
			m_stage = Stage::UntilClose;

			return true;
		}

		const auto contentLengthValue = String::trim(m_response.value(HTTPResponse::ContentLength));

		if ( !contentLengthValue.empty() )
		{
			/* The header map keeps one value per field — reject duplicates at
			 * the raw-text level (conflicting lengths = smuggling vector). */
			if ( countHeaderOccurrences(headerSection, "content-length") > 1 )
			{
				return false;
			}

			uint64_t contentLength = 0;

			if ( !parseDecimalStrict(contentLengthValue, contentLength) )
			{
				return false;
			}

			if ( contentLength > m_limits.maxBodySize )
			{
				return false;
			}

			if ( contentLength == 0 )
			{
				m_stage = Stage::Complete;

				return true;
			}

			m_bodyRemaining = contentLength;
			m_stage = Stage::FixedBody;

			return true;
		}

		/* No explicit framing: the body runs until the peer closes. */
		m_stage = Stage::UntilClose;

		return true;
	}

	HTTPResponseParser::Result
	HTTPResponseParser::feed (const char * data, size_t size) noexcept
	{
		if ( m_stage == Stage::Failed )
		{
			return Result::Failure;
		}

		size_t offset = 0;

		while ( offset < size || m_stage == Stage::Complete )
		{
			switch ( m_stage )
			{
				case Stage::StatusLineAndHeaders :
				{
					/* Accumulate until the blank line, bounded. */
					const auto searchBase = m_headerBuffer.size() >= 3 ? m_headerBuffer.size() - 3 : 0;
					const auto capacity = m_limits.maxHeaderSectionSize - std::min(m_headerBuffer.size(), m_limits.maxHeaderSectionSize);
					const auto toAppend = std::min(capacity, size - offset);

					m_headerBuffer.append(data + offset, toAppend);
					offset += toAppend;

					const auto terminator = m_headerBuffer.find(HTTPHeaders::HeaderSeparator, searchBase);

					if ( terminator == std::string::npos )
					{
						if ( m_headerBuffer.size() >= m_limits.maxHeaderSectionSize )
						{
							return this->fail("header section exceeds the size limit");
						}

						/* All input consumed, headers still incomplete. */
						continue;
					}

					/* Give back the bytes read past the terminator. */
					const auto sectionEnd = terminator + 4;

					offset -= m_headerBuffer.size() - sectionEnd;

					const auto headerSection = m_headerBuffer.substr(0, terminator);

					m_headerBuffer.clear();

					if ( !this->onHeaderSectionComplete(headerSection) )
					{
						return this->fail("invalid header section");
					}

					break;
				}

				case Stage::FixedBody :
				{
					const auto available = size - offset;
					const auto toConsume = static_cast< size_t >(std::min< uint64_t >(m_bodyRemaining, available));

					if ( !this->appendBody(data + offset, toConsume) )
					{
						return this->fail("body exceeds the size limit");
					}

					offset += toConsume;
					m_bodyRemaining -= toConsume;

					if ( m_bodyRemaining == 0 )
					{
						m_stage = Stage::Complete;
					}

					break;
				}

				case Stage::ChunkedSize :
				{
					/* Accumulate the chunk-size line up to CRLF, bounded. */
					while ( offset < size )
					{
						if ( m_lineBuffer.size() >= m_limits.maxChunkSizeLineLength )
						{
							return this->fail("chunk-size line exceeds the length limit");
						}

						const auto character = data[offset++];

						m_lineBuffer.push_back(character);

						if ( character == '\n' )
						{
							break;
						}
					}

					if ( m_lineBuffer.empty() || m_lineBuffer.back() != '\n' )
					{
						/* Line still incomplete. */
						continue;
					}

					if ( m_lineBuffer.size() < 3 || m_lineBuffer[m_lineBuffer.size() - 2] != '\r' )
					{
						return this->fail("malformed chunk-size line terminator");
					}

					uint64_t chunkSize = 0;

					if ( !parseChunkSize(std::string_view{m_lineBuffer.data(), m_lineBuffer.size() - 2}, chunkSize) )
					{
						return this->fail("malformed chunk size");
					}

					m_lineBuffer.clear();

					if ( chunkSize == 0 )
					{
						m_stage = Stage::ChunkedTrailers;
					}
					else
					{
						m_bodyRemaining = chunkSize;
						m_stage = Stage::ChunkedData;
					}

					break;
				}

				case Stage::ChunkedData :
				{
					const auto available = size - offset;
					const auto toConsume = static_cast< size_t >(std::min< uint64_t >(m_bodyRemaining, available));

					if ( !this->appendBody(data + offset, toConsume) )
					{
						return this->fail("body exceeds the size limit");
					}

					offset += toConsume;
					m_bodyRemaining -= toConsume;

					if ( m_bodyRemaining == 0 )
					{
						m_stage = Stage::ChunkedDataTerminator;
					}

					break;
				}

				case Stage::ChunkedDataTerminator :
				{
					/* Exactly CRLF after the chunk data (RFC 9112 §7.1). */
					while ( offset < size && m_lineBuffer.size() < 2 )
					{
						m_lineBuffer.push_back(data[offset++]);
					}

					if ( m_lineBuffer.size() < 2 )
					{
						continue;
					}

					if ( m_lineBuffer[0] != '\r' || m_lineBuffer[1] != '\n' )
					{
						return this->fail("malformed chunk data terminator");
					}

					m_lineBuffer.clear();
					m_stage = Stage::ChunkedSize;

					break;
				}

				case Stage::ChunkedTrailers :
				{
					/* Trailer fields are read, bounded, and discarded — none is
					 * meaningful to this client. The section ends at a blank line. */
					while ( offset < size )
					{
						if ( m_lineBuffer.size() >= m_limits.maxTrailerSectionSize )
						{
							return this->fail("trailer section exceeds the size limit");
						}

						const auto character = data[offset++];

						m_lineBuffer.push_back(character);

						const auto length = m_lineBuffer.size();

						/* The zero-chunk line CRLF was already consumed: an
						 * immediate CRLF, or any line ending in CRLFCRLF, closes. */
						if ( length == 2 && m_lineBuffer[0] == '\r' && m_lineBuffer[1] == '\n' )
						{
							m_lineBuffer.clear();
							m_stage = Stage::Complete;

							break;
						}

						if ( length >= 4 && m_lineBuffer.compare(length - 4, 4, HTTPHeaders::HeaderSeparator) == 0 )
						{
							m_lineBuffer.clear();
							m_stage = Stage::Complete;

							break;
						}
					}

					if ( m_stage != Stage::Complete )
					{
						continue;
					}

					break;
				}

				case Stage::UntilClose :
				{
					if ( !this->appendBody(data + offset, size - offset) )
					{
						return this->fail("body exceeds the size limit");
					}

					offset = size;

					break;
				}

				case Stage::Complete :
				{
					if ( offset < size )
					{
						/* Data past the end of the response: this client never
						 * pipelines, a talkative peer forfeits connection reuse. */
						Logging::warning(Tag, "unexpected data past the end of the response.");
					}

					return Result::Complete;
				}

				case Stage::Failed :
					return Result::Failure;
			}
		}

		return m_stage == Stage::Complete ? Result::Complete : Result::NeedMoreData;
	}

	HTTPResponseParser::Result
	HTTPResponseParser::finish () noexcept
	{
		switch ( m_stage )
		{
			case Stage::UntilClose :
				m_stage = Stage::Complete;

				return Result::Complete;

			case Stage::Complete :
				return Result::Complete;

			default :
				return this->fail("the stream ended on a truncated response");
		}
	}
}
