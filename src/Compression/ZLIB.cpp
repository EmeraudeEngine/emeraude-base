/*
 * src/Compression/ZLIB.cpp
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

#include "ZLIB.hpp"

/* STL inclusions. */
#include <cstdint>
#include <iostream>
#include <string>

/* Local inclusions. */
#include "Logging/Logging.hpp"

/* Third-party inclusions. */
#include <zlib.h>

namespace EmEn::Base::Compression::ZLIB
{
	size_t
	compressStream (std::istream & sourceStream, std::ostream & targetStream, size_t chunkSize, int level) noexcept
	{
		/* A zero chunk size would divide by zero when counting the steps below. */
		if ( chunkSize == 0 )
		{
			Logging::error("Compression::ZLIB", "compressStream(), the chunk size must not be zero !");

			return 0;
		}

		auto uLongf_chunkSize = static_cast< uLongf >(chunkSize);
		sourceStream.seekg(0, std::istream::end);
		const std::streamoff finalPosition = sourceStream.tellg();
		sourceStream.seekg(0, std::istream::beg);

		if ( finalPosition <= 0 )
		{
			Logging::error("Compression::ZLIB", "compressStream(), nothing to compress !");

			return 0;
		}

		/* Gets length of stream. */
		auto streamSize = static_cast< uLongf >(finalPosition);

		size_t steppes = 0;

		if ( streamSize < uLongf_chunkSize )
		{
			uLongf_chunkSize = streamSize;

			steppes = 1;
		}
		else
		{
			steppes = streamSize / uLongf_chunkSize;

			if ( streamSize % uLongf_chunkSize > 0 )
			{
				steppes++;
			}
		}

		/* Compression result size. */
		size_t compressedSize = 0;

		/* Uncompressed source stream will go here. */
		std::string source{};
		source.resize(uLongf_chunkSize);

		/* Compressed buffer will go here and read back to target stream. */
		std::string destination{};

		for ( size_t step = 0; step < steppes; step++ )
		{
			/* Read chunk size byte of uncompressed data. */
			const auto readSize = static_cast< std::streamsize >(uLongf_chunkSize * sizeof(char));

			sourceStream.read(source.data(), readSize);

			/* Prepare the size of the destination buffer by estimate it. */
			auto destinationSize = compressBound(uLongf_chunkSize);

			destination.resize(destinationSize);

			/* Compression and retrieve the real size of the destination buffer. */
			auto * dest = reinterpret_cast< Bytef * >(destination.data());
			const auto * src = reinterpret_cast< const Bytef * >(source.c_str());

			const auto error = compress2(dest, &destinationSize, src, uLongf_chunkSize, level);

			/* NOTE: every zlib error code is negative (Z_STREAM_ERROR for an invalid level, Z_MEM_ERROR, Z_BUF_ERROR). */
			if ( error != Z_OK )
			{
				Logging::error("Compression::ZLIB", std::string{"compressStream(), "} + zError(error));

				return 0;
			}

			/* Write the base size and the compressed size, each as a 64-bit field: uLongf is only 32 bits on
			 * Windows (LLP64), so writing sizeof(size_t) bytes straight from it would over-read the variable. */
			const auto baseSizeField = static_cast< uint64_t >(uLongf_chunkSize);
			const auto compressedSizeField = static_cast< uint64_t >(destinationSize);

			targetStream.write(reinterpret_cast< const char * >(&baseSizeField), sizeof(baseSizeField));
			targetStream.write(reinterpret_cast< const char * >(&compressedSizeField), sizeof(compressedSizeField));

			/* Write compressed data. */
			const auto writeSize = static_cast< std::streamsize >(destinationSize * sizeof(char));

			targetStream.write(destination.c_str(), writeSize);

			/* Adding compressed data chunk size to the total. */
			compressedSize += destinationSize;

			/* Compute data left to compress and reduce
			 * the chunk size if we are at the end of the stream. */
			if ( streamSize > uLongf_chunkSize )
			{
				streamSize -= uLongf_chunkSize;
			}

			if ( streamSize < uLongf_chunkSize )
			{
				uLongf_chunkSize = streamSize;
				source.resize(uLongf_chunkSize);
			}
		}

		return compressedSize;
	}

	bool
	decompressStream (std::istream & sourceStream, std::ostream & targetStream) noexcept
	{
		/* Compressed source stream will go here. */
		std::string source;
		/* Uncompressed buffer will go here
		 * and read back to target stream. */
		std::string destination;

		while ( sourceStream.good() )
		{
			/* Read the uncompressed size: a 64-bit field (see compressStream()), never straight into an uLongf,
			 * which is only 32 bits on Windows (LLP64). */
			uint64_t baseSizeField = 0;

			sourceStream.read(reinterpret_cast< char * >(&baseSizeField), sizeof(baseSizeField));

			/* If the chunk header says nothing,
			 * we are at the end of the stream. */
			if ( baseSizeField == 0 )
			{
				return true;
			}

			/* Read the compressed size (64-bit field). */
			uint64_t compressedSizeField = 0;

			sourceStream.read(reinterpret_cast< char * >(&compressedSizeField), sizeof(compressedSizeField));

			/* Validate the untrusted chunk sizes BEFORE allocating: an unchecked baseSize lets a few
			 * input bytes request a multi-GB destination buffer (fuzz_compression OOM). Require both reads
			 * to have succeeded, bound both sizes to a sane cap, and reject a decompressed size beyond
			 * zlib's maximum expansion of the compressed payload. */
			constexpr uint64_t MaxChunkBytes = 1ULL << 30;   /* 1 GB */
			constexpr uint64_t MaxZlibRatio = 1032;          /* deflate theoretical max expansion */

			if ( !sourceStream || compressedSizeField == 0 || compressedSizeField > MaxChunkBytes || baseSizeField > MaxChunkBytes || baseSizeField > compressedSizeField * MaxZlibRatio )
			{
				Logging::error("Compression::ZLIB", "decompressStream(), implausible or unreadable chunk sizes !");

				return false;
			}

			/* NOTE: both sizes are now at most MaxChunkBytes (1 GB), which fits any uLongf (32 bits or more). */
			auto baseSize = static_cast< uLongf >(baseSizeField);
			const auto compressedSize = static_cast< uLongf >(compressedSizeField);

			/* Resize both of buffer before writing into. */
			source.resize(compressedSize);
			destination.resize(baseSize);

			/* Read some bytes of compressed data. */
			const auto readSize = static_cast< std::streamsize >(compressedSize * sizeof(char));

			if ( sourceStream.read(source.data(), readSize) )
			{
				auto * dest = reinterpret_cast< Bytef * >(destination.data());
				const auto * src = reinterpret_cast< const Bytef * >(source.c_str());

				if ( uncompress(dest, &baseSize, src, compressedSize) != Z_OK )
				{
					Logging::error("Compression::ZLIB", "decompressStream(), unable to uncompress stream.");

					return false;
				}

				/* Write uncompressed data to destination string. */
				const auto writeSize = static_cast< std::streamsize >(baseSize * sizeof(char));

				targetStream.write(destination.c_str(), writeSize);
			}
			else
			{
				Logging::error("Compression::ZLIB", "decompressStream(), unable to read compressed stream.");

				return false;
			}
		}

		Logging::error("Compression::ZLIB", "decompressStream(), stream seems broken.");

		return false;
	}

	/**
	 * @brief Compresses a string with ZLIB algorithm.
	 * @param input A reference to a string.
	 * @param output A writable reference to a string.
	 * @param level The compression level from 0 to 9. Default 9 (high).
	 * @return bool
	 */
	bool
	compressString (const std::string & input, std::string & output, int level) noexcept
	{
		std::stringstream sourceStream;
		sourceStream << input;

		std::stringstream outputStream;

		/* NOTE: the chunk size is the third parameter: passing the level there compressed in chunks of `level`
		 * bytes (9 by default) at the default level, and divided by zero for level 0. */
		if ( compressStream(sourceStream, outputStream, DefaultChunkSize, level) == 0 )
		{
			return false;
		}

		output = outputStream.str();

		return true;
	}

	/**
	 * @brief Decompresses a string with ZLIB algorithm.
	 * @param input A reference to a string.
	 * @param output A writable reference to a string.
	 * @return bool
	 */
	bool
	decompressString (const std::string & input, std::string & output) noexcept
	{
		std::stringstream sourceStream;
		sourceStream << input;

		std::stringstream outputStream;

		if ( !decompressStream(sourceStream, outputStream) )
		{
			return false;
		}

		output = outputStream.str();

		return true;
	}
}
