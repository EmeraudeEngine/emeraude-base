/*
 * src/Compression/LZMA/Compressor.cpp
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

#include "Compressor.hpp"

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <iostream>

namespace EmEn::Base::Compression::LZMA
{
	Compressor::Compressor (uint32_t maxThreads, uint32_t preset) noexcept
		: m_maxThreads(maxThreads),
		m_preset(preset),
		/* NOTE: m_stream, m_maxThreads and m_preset are declared (initialized) before m_ready. */
		m_ready(maxThreads > 0 ? this->initMT(maxThreads, preset) : this->init(preset))
	{

	}

	bool
	Compressor::restart () noexcept
	{
		lzma_end(&m_stream);

		const lzma_stream fresh = LZMA_STREAM_INIT;
		m_stream = fresh;

		m_ready = m_maxThreads > 0 ? this->initMT(m_maxThreads, m_preset) : this->init(m_preset);

		return m_ready;
	}

	Compressor::~Compressor ()
	{
		lzma_end(&m_stream);
	}

	bool
	Compressor::compressString (const std::string & input, std::string & output) noexcept
	{
		std::stringstream sourceStream;
		sourceStream << input;

		std::stringstream outputStream;

		if ( !this->compress(sourceStream, outputStream) )
		{
			return false;
		}

		output = outputStream.str();

		return true;
	}

	bool
	Compressor::init (uint32_t preset) noexcept
	{
		const auto ret = lzma_easy_encoder(&m_stream, preset, LZMA_CHECK_CRC64);

		if ( ret != LZMA_OK )
		{
			switch ( ret )
			{
				case LZMA_MEM_ERROR :
					std::cerr << "LZMA encoder init error : Memory allocation failed !" "\n";
					break;

				case LZMA_OPTIONS_ERROR :
					std::cerr << "LZMA encoder init error : Specified filter chain is not supported !" "\n";
					break;

				case LZMA_UNSUPPORTED_CHECK :
					std::cerr << "LZMA encoder init error : Specified integrity check is not supported !" "\n";
					break;

				default :
					std::cerr << "LZMA encoder init error : Unknown error, possibly a bug !" "\n";
					break;
			}

			return false;
		}

		return true;
	}

	bool
	Compressor::initMT (uint32_t maxThreads, uint32_t preset) noexcept
	{
		lzma_mt settings{};
		settings.flags = 0;
		/* NOTE: At least one thread, at most the processor's (lzma_cputhreads() is 0 when unknown). The formula was
		 * max(min(1, cpus), maxThreads): the request, uncapped — 2000 threads took 838 MB (2026-10-18; the Decompressor
		 * was fixed on 2026-10-17, this one was missed). */
		settings.threads = std::clamp(maxThreads, 1U, std::max(1U, lzma_cputhreads()));
		settings.block_size = 8192 * 64;
		settings.timeout = 0;
		settings.preset = preset;
		settings.filters = nullptr;
		settings.check = LZMA_CHECK_CRC64;

		const auto ret = lzma_stream_encoder_mt(&m_stream, &settings);

		if ( ret != LZMA_OK )
		{
			switch ( ret )
			{
				case LZMA_MEM_ERROR :
					std::cerr << "LZMA encoder init (MT) error : Memory allocation failed !" "\n";
					break;

				case LZMA_OPTIONS_ERROR :
					std::cerr << "LZMA encoder init (MT) error : Specified filter chain is not supported !" "\n";
					break;

				case LZMA_UNSUPPORTED_CHECK :
					std::cerr << "LZMA encoder init (MT) error : Specified integrity check is not supported !" "\n";
					break;

				default :
					std::cerr << "LZMA encoder init (MT) error : Unknown error, possibly a bug !" "\n";
					break;
			}

			return false;
		}

		return true;
	}

	bool
	Compressor::compress (std::stringstream & input, std::stringstream & output) noexcept
	{
		/* NOTE: Reusable (owner decision, 2026-10-18): every call after the first starts a fresh stream. */
		if ( m_streamUsed && !this->restart() )
		{
			return false;
		}

		if ( !m_ready )
		{
			return false;
		}

		m_streamUsed = true;

		lzma_action action = LZMA_RUN;

		std::array< uint8_t, 8192 > inbuf{0};
		std::array< uint8_t, 8192 > outbuf{0};

		m_stream.next_in = nullptr;
		m_stream.avail_in = 0;
		m_stream.next_out = outbuf.data();
		m_stream.avail_out = outbuf.size();

		while ( true )
		{
			if ( m_stream.avail_in == 0 && !input.eof() )
			{
				m_stream.next_in = inbuf.data();

				if ( input.read(reinterpret_cast< char * >(inbuf.data()), inbuf.size()).bad() )
				{
					std::cerr << "LZMA encoder error: Read error !" "\n";

					return false;
				}

				/* NOTE: gcount() is in [0, inbuf.size()] after a read that is not bad(). */
				m_stream.avail_in = static_cast< size_t >(input.gcount());

				if ( input.eof() )
				{
					action = LZMA_FINISH;
				}
			}

			const auto ret = lzma_code(&m_stream, action);

			if ( m_stream.avail_out == 0 || ret == LZMA_STREAM_END )
			{
				if ( output.write(reinterpret_cast< char * >(outbuf.data()), static_cast< long >(outbuf.size() - m_stream.avail_out)).bad() )
				{
					std::cerr << "LZMA encoder error: Write error !" "\n";

					return false;
				}

				m_stream.next_out = outbuf.data();
				m_stream.avail_out = outbuf.size();
			}

			if ( ret != LZMA_OK )
			{
				if ( ret == LZMA_STREAM_END )
				{
					return true;
				}

				switch ( ret )
				{
					case LZMA_MEM_ERROR:
						std::cerr << "LZMA encoder error: Memory allocation failed !" "\n";
						break;

					case LZMA_DATA_ERROR:
						std::cerr << "LZMA encoder error: File size limits exceeded !" "\n";
						break;

					default:
						std::cerr << "LZMA encoder error: Unknown error, possibly a bug !" "\n";
						break;
				}

				return false;
			}
		}
	}
}
