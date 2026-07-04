/*
 * src/Testing/test_NetworkHTTPSClientLive.cpp
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

/* LIVE network tests — the real-world "does it actually download from a secure,
 * public server?" check. They are OPT-IN: they hit the public internet against the
 * system trust store, so they are skipped unless EMERAUDE_RUN_LIVE_NETWORK_TESTS is
 * set. This keeps the default suite hermetic (offline, deterministic, CI-safe) while
 * giving a human a one-command end-to-end validation. Run with:
 *
 *   EMERAUDE_RUN_LIVE_NETWORK_TESTS=1 \
 *     ./.claude-build-release/Release/EmeraudeBaseUnitTests --gtest_filter='NetworkHTTPSClientLive.*'
 */

#include <gtest/gtest.h>

/* STL inclusions. */
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

/* Local inclusions. */
#include "Network/HTTPSClient.hpp"
#include "Network/TrustStore.hpp"

using namespace EmEn::Base;

namespace
{
	/* This resource is EXTERNAL and may stop responding (host down, file moved or
	 * removed) — a failure here does not necessarily mean the client is broken; it
	 * is why this whole suite is opt-in and never part of the default (hermetic) run.
	 * A stable, secure, public resource: the full-resolution "Mr Bean" photo on
	 * Wikimedia's CDN (image/jpeg, ~732 KB, HTTP/1.1 over TLS). NOTE: this is the
	 * direct upload URL — the fr.wikipedia.org article link with a '#/media/...'
	 * fragment is the HTML page, not the file. */
	constexpr auto MrBeanImageURL{"https://upload.wikimedia.org/wikipedia/commons/6/6f/Rowan_Atkinson_and_Manneken_Pis.jpg"};

	/** @brief Skips the calling test unless live network tests are explicitly enabled. */
	bool
	liveNetworkEnabled () noexcept
	{
		return std::getenv("EMERAUDE_RUN_LIVE_NETWORK_TESTS") != nullptr;
	}

	/** @brief Builds an HTTPS client trusting the operating-system CA store. */
	asio::ssl::context
	makeSystemTrustContext () noexcept
	{
		asio::ssl::context context{asio::ssl::context::tls_client};

		(void)Network::TrustStore::applySystemTrustStore(context);

		return context;
	}
}

TEST(NetworkHTTPSClientLive, downloadsMrBeanImageToFile)
{
	if ( !liveNetworkEnabled() )
	{
		GTEST_SKIP() << "set EMERAUDE_RUN_LIVE_NETWORK_TESTS=1 to run live network tests";
	}

	auto tlsContext = makeSystemTrustContext();

	Network::HTTPSClient client{tlsContext};

	const auto filepath = std::filesystem::temp_directory_path() / "emeraude-mr-bean.jpg";

	ASSERT_TRUE(client.download(Network::URI{MrBeanImageURL}, filepath)) << "download failed (network / trust store / TLS ?)";

	/* Verify we actually got the binary: JPEG magic (FF D8 FF) + a sane size. */
	std::ifstream file{filepath, std::ios::binary};
	ASSERT_TRUE(file.is_open());

	std::array< unsigned char, 3 > magic{};
	file.read(reinterpret_cast< char * >(magic.data()), magic.size());

	EXPECT_EQ(magic[0], 0xFF);
	EXPECT_EQ(magic[1], 0xD8);
	EXPECT_EQ(magic[2], 0xFF);

	const auto fileSize = std::filesystem::file_size(filepath);
	EXPECT_GT(fileSize, 100000U) << "file suspiciously small: " << fileSize << " bytes";

	std::filesystem::remove(filepath);
}

TEST(NetworkHTTPSClientLive, getReturnsImageContentType)
{
	if ( !liveNetworkEnabled() )
	{
		GTEST_SKIP() << "set EMERAUDE_RUN_LIVE_NETWORK_TESTS=1 to run live network tests";
	}

	auto tlsContext = makeSystemTrustContext();

	Network::HTTPSClient client{tlsContext};

	const auto result = client.get(Network::URI{MrBeanImageURL});

	ASSERT_TRUE(result.has_value()) << "GET failed";
	EXPECT_EQ(result->response.codeResponse(), 200);
	EXPECT_NE(result->response.value(Network::HTTPResponse::ContentType).find("image/jpeg"), std::string::npos);
	EXPECT_FALSE(result->body.empty());

	/* The body IS the image bytes: same JPEG magic. */
	ASSERT_GE(result->body.size(), 3U);
	EXPECT_EQ(static_cast< unsigned char >(result->body[0]), 0xFF);
	EXPECT_EQ(static_cast< unsigned char >(result->body[1]), 0xD8);
}

TEST(NetworkHTTPSClientLive, rejectsUntrustedRealServer)
{
	if ( !liveNetworkEnabled() )
	{
		GTEST_SKIP() << "set EMERAUDE_RUN_LIVE_NETWORK_TESTS=1 to run live network tests";
	}

	/* Empty trust store (no system CAs loaded): the real, valid Wikimedia
	 * certificate must be rejected — proves verification is truly enforced. */
	asio::ssl::context emptyTrust{asio::ssl::context::tls_client};

	Network::HTTPSClient client{emptyTrust};

	EXPECT_FALSE(client.get(Network::URI{MrBeanImageURL}).has_value());
}
