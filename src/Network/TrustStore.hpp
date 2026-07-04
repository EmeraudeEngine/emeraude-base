/*
 * src/Network/TrustStore.hpp
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
#include <filesystem>

/* Third-party inclusions.
 * NOTE: the no-exceptions hook MUST be included before any asio header. */
#include "Network/asio_throw_exception.hpp"
#include "asio/ssl.hpp"

namespace EmEn::Base::Network::TrustStore
{
	/**
	 * @brief Configures a TLS context with the platform system trust store.
	 * @note CA trust strategy (owner-ruled 2026-07-04, see docs/plans/network-tls/README.md):
	 * hybrid — native system store per platform, plus the applyCABundleFile() override.
	 * The TLS provider is a custom-built LibreSSL, so its compiled-in default certificate
	 * paths are unusable: the trust store must be bootstrapped explicitly on every platform.
	 *  - Linux: probes the well-known distribution CA bundle locations (the curl/Go
	 *    practice), first match wins; falls back to the hashed /etc/ssl/certs directory.
	 *  - macOS: loads /etc/ssl/cert.pem (Apple-maintained bundle extracted from the
	 *    system Keychain). User/enterprise Keychain additions are NOT imported — use
	 *    applyCABundleFile() for those.
	 *  - Windows: imports the ROOT and CA system certificate stores through CryptoAPI
	 *    (LibreSSL has no equivalent of OpenSSL >= 3.2 'winstore' loader).
	 * @param context A reference to the TLS context to configure.
	 * @return bool True when at least one trusted certificate source was loaded.
	 */
	[[nodiscard]]
	bool applySystemTrustStore (asio::ssl::context & context) noexcept;

	/**
	 * @brief Loads a custom CA bundle file (PEM, one or more certificates) into a TLS context.
	 * @note This is the override side of the hybrid CA strategy: hermetic tests (local
	 * self-signed authority) and corporate/user-provided authorities. It can be used
	 * alone or on top of applySystemTrustStore().
	 * @param context A reference to the TLS context to configure.
	 * @param bundleFilepath The path to the PEM bundle file.
	 * @return bool True when the file was read and its certificates were added.
	 */
	[[nodiscard]]
	bool applyCABundleFile (asio::ssl::context & context, const std::filesystem::path & bundleFilepath) noexcept;

	/**
	 * @brief Returns the number of certificates currently held by a TLS context trust store.
	 * @note Diagnostic helper (also used by the unit tests to prove certificates were
	 * actually loaded, not merely that a call succeeded).
	 * @param context A reference to the TLS context to inspect.
	 * @return size_t The number of X509 certificates in the context verification store.
	 */
	[[nodiscard]]
	size_t certificateCount (asio::ssl::context & context) noexcept;
}