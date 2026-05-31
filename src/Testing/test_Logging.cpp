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

	TEST(Logging, defaultSinkSplitsStdoutStderr)
	{
		Logging::setSink(nullptr);

		std::stringstream outBuffer;
		std::stringstream errBuffer;
		auto * previousOut = std::cout.rdbuf(outBuffer.rdbuf());
		auto * previousErr = std::cerr.rdbuf(errBuffer.rdbuf());

		Logging::log(Severity::Info, "Boot", "ready");     /* -> stdout */
		Logging::log(Severity::Error, "Disk", "failure");  /* -> stderr */

		std::cout.rdbuf(previousOut);
		std::cerr.rdbuf(previousErr);

		const auto out = outBuffer.str();
		const auto err = errBuffer.str();

		/* Info -> stdout only. */
		EXPECT_NE(out.find("ready"), std::string::npos);
		EXPECT_EQ(err.find("ready"), std::string::npos);
		/* Error -> stderr only. */
		EXPECT_NE(err.find("failure"), std::string::npos);
		EXPECT_EQ(out.find("failure"), std::string::npos);
	}
}