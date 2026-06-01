/*
 * src/Testing/TinySoundFontImpl.cpp
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

/* Compiles the header-only TinySoundFont implementation into the unit-test executable.
 *
 * The emeraude_base library deliberately does NOT compile this implementation: when base is
 * embedded in a host application (e.g. emeraude-engine), that host owns the single TSF
 * implementation instance (engine: Audio/SoundfontResource.cpp). Defining it in the library too
 * would yield duplicate tsf_* symbols at the host's final link.
 *
 * The WaveFactory MIDI tests instantiate FileFormatMIDI, whose renderToWave() odr-uses the SF2
 * renderWithSoundfont() path, so the test binary must provide the tsf_* symbols itself. It does
 * so here, exactly once. The tinysoundfont directory is a SYSTEM include on emeraude::base, so
 * the implementation's own warnings do not trip -Werror. */
#define TSF_IMPLEMENTATION
#include "tsf.h"