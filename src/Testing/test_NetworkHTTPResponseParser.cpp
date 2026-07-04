/*
 * src/Testing/test_NetworkHTTPResponseParser.cpp
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
#include <string>

/* Local inclusions. */
#include "Network/HTTPResponseParser.hpp"

using namespace EmEn::Base::Network;

namespace
{
	/**
	 * @brief Feeds a whole payload in fixed-size slices (test helper).
	 * @param parser The parser under test.
	 * @param payload The raw response bytes.
	 * @param sliceSize The feeding granularity (0 = single feed).
	 * @return HTTPResponseParser::Result The last non-NeedMoreData result, or NeedMoreData.
	 */
	HTTPResponseParser::Result
	feedBySlices (HTTPResponseParser & parser, const std::string & payload, size_t sliceSize) noexcept
	{
		if ( sliceSize == 0 )
		{
			return parser.feed(payload.data(), payload.size());
		}

		auto result = HTTPResponseParser::Result::NeedMoreData;

		for ( size_t offset = 0; offset < payload.size(); offset += sliceSize )
		{
			const auto size = std::min(sliceSize, payload.size() - offset);

			result = parser.feed(payload.data() + offset, size);

			if ( result == HTTPResponseParser::Result::Failure )
			{
				return result;
			}
		}

		return result;
	}
}

TEST(NetworkHTTPResponseParser, simpleContentLengthResponse)
{
	HTTPResponseParser parser;

	const std::string payload{"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 5\r\n\r\nHello"};

	ASSERT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Complete);

	EXPECT_EQ(parser.response().codeResponse(), 200);
	EXPECT_EQ(parser.response().textResponse(), "OK");
	EXPECT_EQ(parser.body(), "Hello");
	EXPECT_EQ(parser.bodyBytesDecoded(), 5U);
	EXPECT_TRUE(parser.response().keepConnectionAlive());
}

TEST(NetworkHTTPResponseParser, byteByByteFeedingIsEquivalent)
{
	const std::string payload{"HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello"};

	HTTPResponseParser parser;

	ASSERT_EQ(feedBySlices(parser, payload, 1), HTTPResponseParser::Result::Complete);

	EXPECT_EQ(parser.response().codeResponse(), 200);
	EXPECT_EQ(parser.body(), "Hello");
}

TEST(NetworkHTTPResponseParser, chunkedResponse)
{
	const std::string payload{
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		"5\r\nHello\r\n"
		"6\r\n World\r\n"
		"0\r\n"
		"\r\n"
	};

	/* Whole feed AND byte-by-byte must decode identically. */
	for ( const size_t sliceSize : {size_t{0}, size_t{1}, size_t{3}} )
	{
		HTTPResponseParser parser;

		ASSERT_EQ(feedBySlices(parser, payload, sliceSize), HTTPResponseParser::Result::Complete);

		EXPECT_EQ(parser.response().codeResponse(), 200);
		EXPECT_EQ(parser.body(), "Hello World");
		EXPECT_EQ(parser.bodyBytesDecoded(), 11U);
	}
}

TEST(NetworkHTTPResponseParser, chunkedWithExtensionsAndTrailers)
{
	const std::string payload{
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		"5;name=value\r\nHello\r\n"
		"0\r\n"
		"X-Checksum: abc123\r\n"
		"\r\n"
	};

	HTTPResponseParser parser;

	ASSERT_EQ(feedBySlices(parser, payload, 1), HTTPResponseParser::Result::Complete);

	EXPECT_EQ(parser.body(), "Hello");
}

TEST(NetworkHTTPResponseParser, untilCloseResponse)
{
	HTTPResponseParser parser;

	const std::string payload{"HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nUnbounded body"};

	/* No framing header: the body only ends with the stream. */
	ASSERT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::NeedMoreData);
	ASSERT_EQ(parser.finish(), HTTPResponseParser::Result::Complete);

	EXPECT_EQ(parser.body(), "Unbounded body");
	EXPECT_FALSE(parser.response().keepConnectionAlive());
}

TEST(NetworkHTTPResponseParser, bodilessResponses)
{
	/* 204 No Content: complete at the header boundary, whatever follows. */
	{
		HTTPResponseParser parser;

		const std::string payload{"HTTP/1.1 204 No Content\r\n\r\n"};

		EXPECT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Complete);
		EXPECT_TRUE(parser.body().empty());
	}

	/* HEAD: Content-Length announces a body that will never come. */
	{
		HTTPResponseParser parser;
		parser.expectBodilessResponse();

		const std::string payload{"HTTP/1.1 200 OK\r\nContent-Length: 1234\r\n\r\n"};

		EXPECT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Complete);
		EXPECT_TRUE(parser.body().empty());
	}
}

TEST(NetworkHTTPResponseParser, interimContinueIsSkipped)
{
	const std::string payload{
		"HTTP/1.1 100 Continue\r\n"
		"\r\n"
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"\r\n"
		"OK"
	};

	HTTPResponseParser parser;

	ASSERT_EQ(feedBySlices(parser, payload, 1), HTTPResponseParser::Result::Complete);

	EXPECT_EQ(parser.response().codeResponse(), 200);
	EXPECT_EQ(parser.body(), "OK");
}

TEST(NetworkHTTPResponseParser, emptyReasonPhraseIsLegal)
{
	HTTPResponseParser parser;

	/* RFC 9112 §4: the reason phrase is optional. */
	const std::string payload{"HTTP/1.1 404\r\nContent-Length: 0\r\n\r\n"};

	ASSERT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Complete);

	EXPECT_EQ(parser.response().codeResponse(), 404);
	EXPECT_TRUE(parser.response().textResponse().empty());
}

TEST(NetworkHTTPResponseParser, headerFieldNamesAreCaseInsensitive)
{
	HTTPResponseParser parser;

	/* RFC 9110 §5.1: field names are case-insensitive — framing must work
	 * whatever case the server picked. */
	const std::string payload{"HTTP/1.1 200 OK\r\ncontent-length: 5\r\ncontent-type: a/b\r\n\r\nHello"};

	ASSERT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Complete);

	EXPECT_EQ(parser.body(), "Hello");
	EXPECT_EQ(parser.response().value("Content-Type"), "a/b");
	EXPECT_EQ(parser.response().value("CONTENT-TYPE"), "a/b");
}

TEST(NetworkHTTPResponseParser, rejectsOversizedHeaderSection)
{
	HTTPResponseParserLimits limits;
	limits.maxHeaderSectionSize = 128;

	HTTPResponseParser parser{limits};

	std::string payload{"HTTP/1.1 200 OK\r\nX-Padding: "};
	payload.append(512, 'a');

	EXPECT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Failure);
}

TEST(NetworkHTTPResponseParser, rejectsMalformedStatusLines)
{
	for ( const auto * hostile : {
		"GARBAGE\r\n\r\n",
		"HTTP/1.1 999 Out of range\r\n\r\n",
		"HTTP/1.1 abc NaN\r\n\r\n",
		"HTTP/1.1  200 double space\r\n\r\n"
	} )
	{
		HTTPResponseParser parser;

		const std::string payload{hostile};

		EXPECT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Failure) << hostile;
	}
}

TEST(NetworkHTTPResponseParser, rejectsUpgradeResponse)
{
	HTTPResponseParser parser;

	const std::string payload{"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n\r\n"};

	EXPECT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Failure);
}

TEST(NetworkHTTPResponseParser, rejectsMalformedContentLength)
{
	for ( const auto * hostile : {
		"HTTP/1.1 200 OK\r\nContent-Length: 12x\r\n\r\n",
		"HTTP/1.1 200 OK\r\nContent-Length: -5\r\n\r\n",
		"HTTP/1.1 200 OK\r\nContent-Length: 99999999999999999999999\r\n\r\n",
		"HTTP/1.1 200 OK\r\nContent-Length: 5, 5\r\n\r\n"
	} )
	{
		HTTPResponseParser parser;

		const std::string payload{hostile};

		EXPECT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Failure) << hostile;
	}
}

TEST(NetworkHTTPResponseParser, rejectsDuplicateContentLength)
{
	HTTPResponseParser parser;

	/* Request-smuggling vector: two lengths, the header map would silently
	 * keep one — the parser must refuse the whole response. */
	const std::string payload{"HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 10\r\n\r\nHello"};

	EXPECT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Failure);
}

TEST(NetworkHTTPResponseParser, transferEncodingOverridesContentLength)
{
	HTTPResponseParser parser;

	/* RFC 9112 §6.3: when Transfer-Encoding is present, Content-Length MUST
	 * be ignored (second smuggling vector). */
	const std::string payload{
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 9999\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		"5\r\nHello\r\n0\r\n\r\n"
	};

	ASSERT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Complete);

	EXPECT_EQ(parser.body(), "Hello");
}

TEST(NetworkHTTPResponseParser, rejectsMalformedChunkSizes)
{
	for ( const auto * hostile : {
		"zz\r\n",                    /* not hexadecimal */
		"FFFFFFFFFFFFFFFFFF\r\n",    /* uint64 overflow */
		"\r\n",                      /* empty size */
		"5 5\r\n"                    /* trailing junk */
	} )
	{
		HTTPResponseParser parser;

		const std::string payload = std::string{"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"} + hostile;

		EXPECT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Failure) << hostile;
	}
}

TEST(NetworkHTTPResponseParser, rejectsMalformedChunkTerminator)
{
	HTTPResponseParser parser;

	const std::string payload{"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHelloXX"};

	EXPECT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Failure);
}

TEST(NetworkHTTPResponseParser, enforcesBodySizeCap)
{
	HTTPResponseParserLimits limits;
	limits.maxBodySize = 8;

	/* Announced up-front by Content-Length: refused at the header stage. */
	{
		HTTPResponseParser parser{limits};

		const std::string payload{"HTTP/1.1 200 OK\r\nContent-Length: 16\r\n\r\n"};

		EXPECT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Failure);
	}

	/* Sneaked in by chunks: refused when the decoded total crosses the cap. */
	{
		HTTPResponseParser parser{limits};

		const std::string payload{"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n10\r\n0123456789abcdef\r\n"};

		EXPECT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Failure);
	}
}

TEST(NetworkHTTPResponseParser, rejectsTruncatedResponses)
{
	/* Truncated fixed body. */
	{
		HTTPResponseParser parser;

		const std::string payload{"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nHalf"};

		ASSERT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::NeedMoreData);
		EXPECT_EQ(parser.finish(), HTTPResponseParser::Result::Failure);
	}

	/* Truncated chunked body. */
	{
		HTTPResponseParser parser;

		const std::string payload{"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHe"};

		ASSERT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::NeedMoreData);
		EXPECT_EQ(parser.finish(), HTTPResponseParser::Result::Failure);
	}

	/* Truncated header section. */
	{
		HTTPResponseParser parser;

		const std::string payload{"HTTP/1.1 200 OK\r\nContent-"};

		ASSERT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::NeedMoreData);
		EXPECT_EQ(parser.finish(), HTTPResponseParser::Result::Failure);
	}
}

TEST(NetworkHTTPResponseParser, streamingConsumerCanDrainBody)
{
	HTTPResponseParserLimits limits;
	limits.maxBodySize = std::numeric_limits< uint64_t >::max();

	HTTPResponseParser parser{limits};

	const std::string headers{"HTTP/1.1 200 OK\r\nContent-Length: 8\r\n\r\n"};

	ASSERT_EQ(parser.feed(headers.data(), headers.size()), HTTPResponseParser::Result::NeedMoreData);

	/* Drain between feeds, as the download-to-file path will do. */
	ASSERT_EQ(parser.feed("ABCD", 4), HTTPResponseParser::Result::NeedMoreData);
	EXPECT_EQ(parser.body(), "ABCD");
	parser.body().clear();

	ASSERT_EQ(parser.feed("EFGH", 4), HTTPResponseParser::Result::Complete);
	EXPECT_EQ(parser.body(), "EFGH");
	EXPECT_EQ(parser.bodyBytesDecoded(), 8U);
}

TEST(NetworkHTTPResponseParser, keepAliveSemantics)
{
	/* HTTP/1.1 + Connection: close. */
	{
		HTTPResponseParser parser;

		const std::string payload{"HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"};

		ASSERT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Complete);
		EXPECT_FALSE(parser.response().keepConnectionAlive());
	}

	/* HTTP/1.0 + Connection: keep-alive. */
	{
		HTTPResponseParser parser;

		const std::string payload{"HTTP/1.0 200 OK\r\nConnection: Keep-Alive\r\nContent-Length: 0\r\n\r\n"};

		ASSERT_EQ(parser.feed(payload.data(), payload.size()), HTTPResponseParser::Result::Complete);
		EXPECT_TRUE(parser.response().keepConnectionAlive());
	}
}