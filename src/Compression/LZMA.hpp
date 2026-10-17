/*
 * src/Compression/LZMA.hpp
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
#include <cstdint>
#include <string>

namespace EmEn::Base::Compression::LZMA
{
	/** @brief The memory a decoder may use: 65 MiB, what the xz -9 preset needs (measured; 64 MiB refuses it). A header
	 * declaring a larger dictionary is refused (LZMA_MEMLIMIT_ERROR). A HARDENING, not a proven defect (owner decision,
	 * 2026-10-17): an 80-byte stream declaring a 512-768 MiB dictionary reserved it without touching it (no RSS on Linux);
	 * the limit bounds that reservation (address space, Windows commit). The streaming Decompressor had no limit. */
	constexpr uint64_t DecoderMemoryLimit{65ULL << 20U};

	/** @brief The largest decompressed string, as a multiple of its compressed size (deflate's maximum — the rule of
	 * every base reader, owner decision 2026-10-17). Beyond it the decompression is refused: 156 KB of xz decompressed to
	 * 1 GiB of zeros (2.1 GB peak, accepted) with no bound. An xz stream compressed beyond 1032:1 (a run of zeros) is
	 * refused too: the known trade-off. */
	constexpr size_t MaxDecompressionRatio{1032};

	/**
	 * @brief Compresses a string with LZMA algorithm.
	 * @param input A reference to a string.
	 * @param output A writable reference to a string.
	 * @param level The compression level from 0 to 9. Default 9 (high).
	 * @return bool
	 */
	bool compressString (const std::string & input, std::string & output, int level = 9) noexcept;

	/**
	 * @brief Decompresses a string with LZMA algorithm.
	 * @note Refused (false, an empty output): an empty input, a stream needing more than DecoderMemoryLimit, an output
	 * beyond the input size x MaxDecompressionRatio, a corrupt or truncated stream.
	 * @param input A reference to a string.
	 * @param output A writable reference to a string.
	 * @return bool
	 */
	bool decompressString (const std::string & input, std::string & output) noexcept;
}
