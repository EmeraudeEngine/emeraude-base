/*
 * src/Testing/main.cpp
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

/* STL inclusions. */
#include <string_view>

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "Logging/Severity.hpp"

int
main (int argc, char * * argv)
{
	::testing::InitGoogleTest(&argc, argv);

	/* NOTE: The guard (owner, 2026-11-09: no direct std::cout / std::cerr, and no diagnostic left on the terminal). A
	 * message that reaches the Logging sink outside a Testing::DiagnosticsCapture fails the running test: a test that
	 * provokes a refusal captures it and asserts its tag and severity (docs/error-handling.md § 7). The capture puts
	 * this guard back when it ends. A Debug message is swallowed (owner, 2026-11-09): it only exists in a DEBUG build
	 * (the sanitizer suites), so the tests stay identical in Release and Debug. */
	static_cast< void >(EmEn::Base::Logging::setSink([] (EmEn::Base::Severity severity, const char * tag, std::string_view message) {
		if ( severity == EmEn::Base::Severity::Debug )
		{
			return;
		}

		ADD_FAILURE() << "Uncaptured diagnostic [" << (tag != nullptr ? tag : "") << "] (" << EmEn::Base::to_cstring(severity) << ") " << message <<
			"\nWrap the call in EmEn::Base::Testing::captureDiagnostics() and assert it.";
	}));

	return RUN_ALL_TESTS();
}
