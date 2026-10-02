/*
 * src/Testing/AsanDefaultOptions.cpp
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

/* ASan's compiled-in defaults (its documented hook), linked into the unit tests on macOS with the sanitizers only
 * (CMakeLists.txt). The system libc++ is not instrumented: its container annotations, half seen, report false
 * container-overflows (gtest's test registry at static initialisation). Every other ASan / UBSan check stays on.
 * Visible and kept: the macOS ASan runtime is a dylib that looks the symbol up at run time. */
extern "C" __attribute__((visibility("default"), used)) const char * __asan_default_options ();

extern "C"
__attribute__((visibility("default"), used))
const char *
__asan_default_options ()
{
	return "detect_container_overflow=0";
}
