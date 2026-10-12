/*
 * src/Testing/test_NetworkHTTPRequest.cpp
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
#include <string>

/* Local inclusions. */
#include "IO/IO.hpp"
#include "Network/HTTPRequest.hpp"
#include "Network/URI.hpp"

using namespace EmEn::Base::Network;

namespace
{
	/** @brief The request line (the text before the first CRLF) of a GET on http://example.com + path. */
	std::string
	requestLine (const std::string & path)
	{
		URI resource{"http://example.com"};
		resource.setPath(EmEn::Base::IO::u8path(path));

		const auto text = HTTPRequest{HTTPRequest::Method::GET, resource}.toString();

		return text.substr(0, text.find("\r\n"));
	}
}

/* 2026-10-12: the request line carries ONE leading slash and the path as is. It wrote " /" before a path that already
 * starts with one ("GET //a/b"), through std's operator<<, which quotes a path ("GET /\"/a/b\""). */
TEST(NetworkHTTPRequest, requestLineAbsolutePath)
{
	EXPECT_EQ(requestLine("/a/b"), "GET /a/b HTTP/1.1");
}

TEST(NetworkHTTPRequest, requestLineRelativePath)
{
	EXPECT_EQ(requestLine("a/b"), "GET /a/b HTTP/1.1");
}

TEST(NetworkHTTPRequest, requestLineEmptyPath)
{
	EXPECT_EQ(requestLine(""), "GET / HTTP/1.1");
}
