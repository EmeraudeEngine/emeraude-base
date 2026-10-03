/*
 * src/VertexFactory/FileFormatNative.hpp
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
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

/* Local inclusions for inheritances. */
#include "FileFormatInterface.hpp"

/* Local inclusions for usages. */
#include "Logging/Logging.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief Emeraude engine native geometry format.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 * @tparam index_data_t The precision type of index data. Default uint32_t.
	 * @extends EmEn::Base::VertexFactory::FileFormatInterface
	 */
	template< typename vertex_data_t = float, typename index_data_t = uint32_t >
	requires (std::is_floating_point_v< vertex_data_t > && std::is_unsigned_v< index_data_t > )
	class FileFormatNative final : public FileFormatInterface< vertex_data_t, index_data_t >
	{
		public:

			static constexpr auto Magic{"EE3D_V1"};

			/** @brief The size of a version-2 vertex record: every ShapeVertex member before the secondary texture
			 * coordinates (version 3 appended them last). */
			static constexpr uint64_t VersionTwoVertexSize{
				(4 * sizeof(Math::Vector< 3, vertex_data_t >)) + sizeof(Math::Vector< 4, int32_t >) + sizeof(Math::Vector< 4, vertex_data_t >) + sizeof(vertex_data_t)
			};

			static_assert(sizeof(ShapeVertex< vertex_data_t >) == VersionTwoVertexSize + sizeof(Math::Vector< 2, vertex_data_t >), "ShapeVertex's layout no longer matches the native format: a version-2 record must be its exact prefix (no padding, the secondary set last).");

			FileFormatNative () noexcept = default;

			/** @copydoc EmEn::Base::VertexFactory::FileFormatInterface::readStream() */
			[[nodiscard]]
			bool
			readStream (IO::ByteStream & stream, ShapeLoadResult< vertex_data_t, index_data_t > & result, const ReadOptions & /*readOptions*/) noexcept override
			{
				auto & geometry = result.shape;
				geometry.clear();

				if ( !stream.isOpen() )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), stream is not open !");

					return false;
				}

				/* 1. Read Header (32 bytes) */
				char header[32] = {0};

				if ( !stream.read(header, 32) )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), unable to read header !");

					return false;
				}

				/* Check Magic "EE3D_V1" (8 bytes including null) */
				if ( std::string(header, 7) != Magic )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), invalid magic !");

					return false;
				}

				/* Check Version (2 bytes) at offset 8 */
				uint16_t version = 0;
				std::memcpy(&version, &header[8], sizeof(uint16_t));

				/* ⚠️⚠️ VERSION 3 (2026-10-03). Vertices are written as a RAW BLOB of
				 * sizeof(ShapeVertex<vertex_data_t>), so ANY change to that structure changes the
				 * on-disk layout. Version 1 vertices were 80 bytes, version 2 (the tangent handedness)
				 * 84, version 3 (the secondary texture coordinates, appended LAST) 92. A version-2
				 * record is the exact first 84 bytes of a version-3 one, so version 2 is still read
				 * (the secondary set at (0, 0)). There is deliberately NO version-1 read path: reading
				 * a v1 blob with a later stride would misparse it SILENTLY — the count validation
				 * below can pass on a wrong stride. Refusing it loudly is the only safe behaviour.
				 * ⚠️ If ShapeVertex or ShapeTriangle ever changes again, BUMP THIS. A size change
				 * with an unchanged version number is silent data corruption. */
				if ( version != 2 && version != 3 )
				{
					Logging::error("VertexFactory::FileFormatNative", std::string{"readStream(), unsupported version "} + std::to_string(version) + " — this build reads versions 2 and 3; a version-1 file predates the tangent handedness and must be re-exported from its source asset !");

					return false;
				}

				/* Check Precision (2 bytes) at offset 10 & 11 */
				const auto vPrecision = static_cast< uint8_t >(header[10]);
				const auto iPrecision = static_cast< uint8_t >(header[11]);

				if ( vPrecision != sizeof(vertex_data_t) || iPrecision != sizeof(index_data_t) )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), precision mismatch !");

					return false;
				}

				/* 2. Read Metadata (Counts) - 3 * 8 bytes = 24 bytes */
				uint64_t counts[3] = {0, 0, 0};

				if ( !stream.read(counts, 3 * sizeof(uint64_t)) )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), unable to read metadata !");

					return false;
				}

				const uint64_t vertexCount = counts[0];
				const uint64_t triangleCount = counts[1];
				const uint64_t colorCount = counts[2];

				/* 3. Validate counts against the remaining stream bytes BEFORE any resize().
				 * On hostile/corrupt input an unvalidated 64-bit count would request a
				 * multi-exabyte allocation (std::length_error -> std::terminate under
				 * -fno-exceptions). The 56-byte header+metadata were read successfully, so the
				 * stream holds at least that many bytes. Division-based bounds avoid integer
				 * overflow in count * elementSize. */
				constexpr uint64_t headerBytes = 32 + (3 * sizeof(uint64_t));
				const uint64_t streamSize = stream.size();

				if ( streamSize < headerBytes )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), stream smaller than its own header !");

					return false;
				}

				uint64_t remaining = streamSize - headerBytes;

				const uint64_t vertexSize = version == 3 ? sizeof(ShapeVertex< vertex_data_t >) : VersionTwoVertexSize;
				constexpr uint64_t triangleSize = sizeof(ShapeTriangle< vertex_data_t, index_data_t >);
				constexpr uint64_t colorSize = sizeof(Math::Vector< 4, vertex_data_t >);

				if ( vertexCount > remaining / vertexSize )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), vertex count exceeds the stream size !");

					return false;
				}

				remaining -= vertexCount * vertexSize;

				if ( triangleCount > remaining / triangleSize )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), triangle count exceeds the stream size !");

					return false;
				}

				remaining -= triangleCount * triangleSize;

				if ( colorCount > remaining / colorSize )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), color count exceeds the stream size !");

					return false;
				}

				/* 4. Resize Geometry (counts now proven to fit). */
				auto & vertices = geometry.vertices();
				auto & triangles = geometry.triangles();
				auto & colors = geometry.vertexColors();

				vertices.resize(vertexCount);
				triangles.resize(triangleCount);
				colors.resize(colorCount);

				/* 5. Read Data Blobs */
				if ( vertexCount > 0 && version == 2 )
				{
					/* A version-2 record is the first VersionTwoVertexSize bytes of a ShapeVertex: each is copied
					 * over a default vertex, whose secondary texture coordinates stay (0, 0). */
					std::vector< char > records(vertexCount * VersionTwoVertexSize);

					if ( !stream.read(records.data(), records.size()) )
					{
						Logging::error("VertexFactory::FileFormatNative", "readStream(), failed to read vertices !");

						return false;
					}

					for ( uint64_t index = 0; index < vertexCount; ++index )
					{
						std::memcpy(static_cast< void * >(&vertices[index]), records.data() + (index * VersionTwoVertexSize), VersionTwoVertexSize);
					}
				}
				else if ( vertexCount > 0 )
				{
					if ( !stream.read(vertices.data(), vertexCount * sizeof(ShapeVertex< vertex_data_t >)) )
					{
						Logging::error("VertexFactory::FileFormatNative", "readStream(), failed to read vertices !");

						return false;
					}
				}

				if ( triangleCount > 0 )
				{
					if ( !stream.read(triangles.data(), triangleCount * sizeof(ShapeTriangle< vertex_data_t, index_data_t >)) )
					{
						Logging::error("VertexFactory::FileFormatNative", "readStream(), failed to read triangles !");

						return false;
					}
				}

				if ( colorCount > 0 )
				{
					if ( !stream.read(colors.data(), colorCount * sizeof(Math::Vector< 4, vertex_data_t >)) )
					{
						Logging::error("VertexFactory::FileFormatNative", "readStream(), failed to read vertex colors !");

						return false;
					}
				}

				return true;
			}

			/** @copydoc EmEn::Base::VertexFactory::FileFormatInterface::writeStream() */
			[[nodiscard]]
			bool
			writeStream (IO::ByteStream & stream, const Shape< vertex_data_t, index_data_t > & geometry, const WriteOptions & /*writeOptions*/) const noexcept override
			{
				if ( !geometry.isValid() )
				{
					Logging::error("VertexFactory::FileFormatNative", "writeStream(), geometry is invalid !");

					return false;
				}

				if ( !stream.isOpen() )
				{
					Logging::error("VertexFactory::FileFormatNative", "writeStream(), stream is not open !");

					return false;
				}

				/* 1. Header (32 bytes) */
				char header[32] = {0};

				std::memcpy(header, Magic, 7);

				/* ⚠️ Must match the accepted version in readStream(), and must be bumped whenever
				 * ShapeVertex or ShapeTriangle changes size — the payload is a raw blob. */
				uint16_t version = 3;
				std::memcpy(&header[8], &version, sizeof(uint16_t));

				header[10] = static_cast< char >(sizeof(vertex_data_t));
				header[11] = static_cast< char >(sizeof(index_data_t));

				if ( !stream.write(header, 32) )
				{
					return false;
				}

				/* 2. Metadata (Counts) */
				uint64_t counts[3];
				counts[0] = static_cast< uint64_t >(geometry.vertices().size());
				counts[1] = static_cast< uint64_t >(geometry.triangles().size());
				counts[2] = static_cast< uint64_t >(geometry.vertexColors().size());

				if ( !stream.write(counts, 3 * sizeof(uint64_t)) )
				{
					return false;
				}

				/* 3. Data Blobs */
				if ( counts[0] > 0 )
				{
					if ( !stream.write(geometry.vertices().data(), counts[0] * sizeof(ShapeVertex< vertex_data_t >)) )
					{
						return false;
					}
				}

				if ( counts[1] > 0 )
				{
					if ( !stream.write(geometry.triangles().data(), counts[1] * sizeof(ShapeTriangle< vertex_data_t, index_data_t >)) )
					{
						return false;
					}
				}

				if ( counts[2] > 0 )
				{
					if ( !stream.write(geometry.vertexColors().data(), counts[2] * sizeof(Math::Vector< 4, vertex_data_t >)) )
					{
						return false;
					}
				}

				return true;
			}
	};
}
