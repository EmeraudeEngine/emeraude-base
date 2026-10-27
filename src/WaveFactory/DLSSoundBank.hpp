/*
 * src/WaveFactory/DLSSoundBank.hpp
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
#include <optional>
#include <span>
#include <vector>

namespace EmEn::Base::WaveFactory::DLS
{
	/**
	 * @brief Converts a Downloadable Sounds bank (DLS Level 1, and the Level 2 chunks that share its layout) into an
	 * in-memory SoundFont 2 that TinySoundFont loads (`tsf_load_memory()`), so an RMID file plays with its OWN bank.
	 * @note Owner decision 2026-10-27 (docs/subsystems/wavefactory/04, the DLS converter): a converter, reusing the
	 * SoundFont synthesizer the MIDI renderer already drives, rather than a second synthesizer.
	 * @note What is converted: every instrument (`lins` / `ins `: `insh` bank and program, the drum flag → SoundFont
	 * bank 128), every region (`rgnh` key and velocity ranges, key group → exclusive class, `wsmp` unity note, fine tune,
	 * gain and forward loop — the region's own, else its wave's — `wlnk` wave through the pool table), the volume
	 * envelope and the pan of the articulations (`art1` / `art2` under the instrument or the region; the region's win),
	 * and the PCM waves of the pool (`wvpl`: 8 or 16 bits, MONO). The other articulation destinations (LFO, pitch
	 * envelope, filter, modulators) are not mapped: the region still plays, with SoundFont defaults there. A region whose
	 * wave is missing, stereo or not PCM is skipped.
	 * @note A trust boundary (the bank comes from a file): every offset, size and count is checked before use; a
	 * malformed bank is REFUSED (std::nullopt), never read past its end. Bounded: at most MaxInstruments instruments,
	 * MaxRegionsPerInstrument regions each, MaxWaves waves.
	 * @note References: MIDI Manufacturers Association, "Downloadable Sounds Level 1" (1997) and "Level 2.2" (2006); the
	 * DLS structures as declared by Microsoft's dls1.h / dls2.h (DirectMusic); E-mu Systems / Creative Technology,
	 * "SoundFont 2.04 Technical Specification" (2006). No third-party code: the layouts follow the specifications.
	 * @param dlsBank The bytes of the whole RIFF 'DLS ' chunk ("RIFF", its size, "DLS ", then its sub-chunks).
	 * @return std::optional< std::vector< std::byte > > The SoundFont 2 file, or nothing when the bank is malformed or
	 * holds no playable region.
	 */
	[[nodiscard]]
	std::optional< std::vector< std::byte > > convertToSoundFont (std::span< const std::byte > dlsBank) noexcept;

	/** @brief The most instruments a bank may declare. */
	constexpr size_t MaxInstruments{4096};

	/** @brief The most regions an instrument may declare. */
	constexpr size_t MaxRegionsPerInstrument{1024};

	/** @brief The most waves a pool may hold. */
	constexpr size_t MaxWaves{65536};
}
