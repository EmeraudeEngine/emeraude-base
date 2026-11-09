/*
 * src/Compression/LZMA/Decompressor.cpp
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

#include "Decompressor.hpp"

/* STL inclusions. */
#include <algorithm>
#include <array>

/* Local inclusions. */
#include "Compression/LZMA.hpp"
#include "Logging/Logging.hpp"

namespace EmEn::Base::Compression::LZMA
{
	Decompressor::Decompressor (uint32_t maxThreads) noexcept
		: m_maxThreads(maxThreads),
		/* NOTE: m_stream and m_maxThreads are declared (initialized) before m_ready. */
		m_ready(maxThreads > 0 ? this->initMT(maxThreads) : this->init())
	{

	}

	bool
	Decompressor::restart () noexcept
	{
		lzma_end(&m_stream);

		const lzma_stream fresh = LZMA_STREAM_INIT;
		m_stream = fresh;

		m_ready = m_maxThreads > 0 ? this->initMT(m_maxThreads) : this->init();

		return m_ready;
	}

	Decompressor::~Decompressor ()
	{
		lzma_end(&m_stream);
	}

	bool
	Decompressor::decompressString (const std::string & input, std::string & output) noexcept
	{
		std::stringstream sourceStream;
		sourceStream << input;

		std::stringstream outputStream;

		/* NOTE: A string is bounded: a few bytes of xz can expand without limit. */
		const size_t maxOutput = input.size() > std::numeric_limits< size_t >::max() / MaxDecompressionRatio ? std::numeric_limits< size_t >::max() : input.size() * MaxDecompressionRatio;

		if ( !this->decompress(sourceStream, outputStream, maxOutput) )
		{
			return false;
		}

		output = outputStream.str();

		return true;
	}

	bool
	Decompressor::init () noexcept
	{
		const auto ret = lzma_stream_decoder(&m_stream, DecoderMemoryLimit, LZMA_CONCATENATED);

		if ( ret != LZMA_OK )
		{
			switch ( ret )
			{
				case LZMA_MEM_ERROR :
					Logging::error("Compression::LZMA::Decompressor", "LZMA decoder init error : Memory allocation failed !");
					break;

				case LZMA_OPTIONS_ERROR :
					Logging::error("Compression::LZMA::Decompressor", "LZMA decoder init error : Specified filter chain is not supported !");
					break;

				default :
					Logging::error("Compression::LZMA::Decompressor", "LZMA decoder init error : Unknown error, possibly a bug !");
					break;
			}

			return false;
		}

		return true;
	}

	bool
	Decompressor::initMT (uint32_t maxThreads) noexcept
	{
		lzma_mt settings{};
		settings.flags = LZMA_CONCATENATED;
		/* NOTE: At least one thread, at most the processor's (lzma_cputhreads() is 0 when unknown). The formula was
		 * max(min(1, cpus), maxThreads): the request, uncapped. */
		settings.threads = std::clamp(maxThreads, 1U, std::max(1U, lzma_cputhreads()));
		settings.timeout = 0;
		/* NOTE: One hard limit for the whole decoder: liblzma falls back to one thread when several do not fit. */
		settings.memlimit_threading = DecoderMemoryLimit;
		settings.memlimit_stop = DecoderMemoryLimit;

		const auto ret = lzma_stream_decoder_mt(&m_stream, &settings);

		if ( ret != LZMA_OK )
		{
			switch ( ret )
			{
				case LZMA_MEM_ERROR :
					Logging::error("Compression::LZMA::Decompressor", "LZMA decoder init (MT) error : Memory allocation failed !");
					break;

				case LZMA_OPTIONS_ERROR :
					Logging::error("Compression::LZMA::Decompressor", "LZMA decoder init (MT) error : Specified filter chain is not supported !");
					break;

				case LZMA_UNSUPPORTED_CHECK :
					Logging::error("Compression::LZMA::Decompressor", "LZMA decoder init (MT) error : Specified integrity check is not supported !");
					break;

				default :
					Logging::error("Compression::LZMA::Decompressor", "LZMA decoder init (MT) error : Unknown error, possibly a bug !");
					break;
			}

			return false;
		}

		return true;
	}

	bool
	Decompressor::decompress (std::stringstream & input, std::stringstream & output, size_t maxOutput) noexcept
	{
		size_t written = 0;

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
					Logging::error("Compression::LZMA::Decompressor", "LZMA decoder error: Read error !");

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
				const auto produced = outbuf.size() - m_stream.avail_out;

				if ( produced > maxOutput - written )
				{
					Logging::error("Compression::LZMA::Decompressor", "LZMA decoder error: The output exceeds its bound, refused !");

					return false;
				}

				written += produced;

				if ( output.write(reinterpret_cast< char * >(outbuf.data()), static_cast< long >(outbuf.size() - m_stream.avail_out)).bad() )
				{
					Logging::error("Compression::LZMA::Decompressor", "LZMA decoder error: Write error !");

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
					case LZMA_MEM_ERROR :
						Logging::error("Compression::LZMA::Decompressor", "LZMA decoder error: Memory allocation failed !");
						break;

					case LZMA_MEMLIMIT_ERROR :
						Logging::error("Compression::LZMA::Decompressor", "LZMA decoder error: The stream needs more decoder memory than the limit, refused !");
						break;

					case LZMA_FORMAT_ERROR :
						Logging::error("Compression::LZMA::Decompressor", "LZMA decoder error: The input is not in the .xz format !");
						break;

					case LZMA_OPTIONS_ERROR :
						Logging::error("Compression::LZMA::Decompressor", "LZMA decoder error: Unsupported compression options !");
						break;

					case LZMA_DATA_ERROR :
						Logging::error("Compression::LZMA::Decompressor", "LZMA decoder error: Compressed file is corrupt !");
						break;

					case LZMA_BUF_ERROR :
						Logging::error("Compression::LZMA::Decompressor", "LZMA decoder error: Compressed file is truncated or otherwise corrupt !");
						break;

					default :
						Logging::error("Compression::LZMA::Decompressor", "LZMA decoder error: Unknown error, possibly a bug !");
						break;
				}

				return false;
			}
		}
	}
}
