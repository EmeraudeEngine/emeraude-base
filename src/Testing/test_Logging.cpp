/*
 * src/Testing/test_Logging.cpp
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
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "Logging/Severity.hpp"

namespace EmEn::Base
{
	TEST(Logging, customSinkReceivesMessages)
	{
		Severity capturedSeverity{Severity::Debug};
		std::string capturedTag;
		std::string capturedMessage;
		int callCount{0};

		Logging::setSink([&] (Severity severity, const char * tag, std::string_view message) {
			capturedSeverity = severity;
			capturedTag = tag != nullptr ? tag : "";
			capturedMessage = std::string{message};
			++callCount;
		});

		Logging::log(Severity::Error, "Net", "boom");
		EXPECT_EQ(callCount, 1);
		EXPECT_EQ(capturedSeverity, Severity::Error);
		EXPECT_EQ(capturedTag, "Net");
		EXPECT_EQ(capturedMessage, "boom");

		/* Convenience wrapper routes the right severity. */
		Logging::warning("Disk", "almost full");
		EXPECT_EQ(callCount, 2);
		EXPECT_EQ(capturedSeverity, Severity::Warning);
		EXPECT_EQ(capturedMessage, "almost full");

		/* MUST reset: the captured lambda references locals that die at scope end. */
		Logging::setSink(nullptr);
	}

	TEST(Logging, defaultSinkWritesToCerr)
	{
		Logging::setSink(nullptr);

		std::stringstream buffer;
		auto * previous = std::cerr.rdbuf(buffer.rdbuf());
		Logging::log(Severity::Info, "Boot", "ready");
		std::cerr.rdbuf(previous);

		const auto output = buffer.str();
		EXPECT_NE(output.find("Boot"), std::string::npos);
		EXPECT_NE(output.find("ready"), std::string::npos);
		EXPECT_NE(output.find("Info"), std::string::npos);
	}
}