/*
 * src/Testing/TemporaryPath.hpp
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
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace EmEn::Base::Testing
{
	/**
	 * @brief Returns a path in the system temporary directory that is UNIQUE to this test process: the leaf gets a
	 * per-process token before its extension ("emeraude-x.bin" → "emeraude-x-<token>.bin").
	 * @note Several copies of the suite may run at once (the macOS peer's 8-copy stress run, two developers, CI): a fixed
	 * name let them truncate or delete each other's files (base item httpserver-tests-share-fixed-temp-files,
	 * 2026-10-08). The token is drawn once per process from std::random_device and the clock (not a seeded draw: no
	 * PortableRandom needed). std::filesystem::temp_directory_path() is called through its error_code overload; when it
	 * fails, the current directory is used (traced).
	 * @param leaf The file or directory name.
	 * @return std::filesystem::path
	 */
	[[nodiscard]]
	inline
	std::filesystem::path
	uniqueTemporaryPath (std::string_view leaf)
	{
		static const std::string processToken = [] {
			std::random_device device;
			const auto clockBits = static_cast< uint64_t >(std::chrono::steady_clock::now().time_since_epoch().count());
			const auto value = ((static_cast< uint64_t >(device()) << 32U) | static_cast< uint64_t >(device())) ^ clockBits;
			constexpr std::string_view Digits{"0123456789abcdef"};
			std::string token(16, '0');

			for ( size_t index = 0; index < token.size(); ++index )
			{
				token[index] = Digits[(value >> (index * 4U)) & 0xFU];
			}

			return token;
		}();

		std::error_code errorCode;
		auto directory = std::filesystem::temp_directory_path(errorCode);

		if ( errorCode )
		{
			std::cerr << "uniqueTemporaryPath(), no temporary directory (" << errorCode.message() << "): the current directory is used." "\n";

			directory = std::filesystem::path{"."};
		}

		const std::filesystem::path leafPath{leaf};

		return directory / (leafPath.stem().string() + "-" + processToken + leafPath.extension().string());
	}
}
