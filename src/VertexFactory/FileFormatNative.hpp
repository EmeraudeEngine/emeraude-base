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
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

/* Local inclusions for inheritances. */
#include "FileFormatInterface.hpp"

/* Local inclusions for usages. */
#include "IO/ByteStream.hpp"
#include "Logging/Logging.hpp"
#include "Math/Vector.hpp"
#include "VertexFactory/Shape.hpp"
#include "VertexFactory/ShapeLoadResult.hpp"
#include "VertexFactory/ShapeTriangle.hpp"
#include "VertexFactory/ShapeVertex.hpp"

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

			/** @brief The version written: 4 since 2026-11-26 (the skinning and the triangles' color and edge indexes left
			 * ShapeVertex / ShapeTriangle for tables of the Shape). */
			static constexpr uint16_t WrittenVersion{4};

			/** @brief The header byte holding the optional tables of a version-4 file. */
			static constexpr size_t FlagsOffset{12};

			/** @brief Flag: the vertices' skinning table follows the colors (influences, then weights). */
			static constexpr uint8_t SkinningFlag{1U << 0U};

			/** @brief Flag: the triangles' color index table follows. */
			static constexpr uint8_t ColorIndexesFlag{1U << 1U};

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
				std::array< char, 32 > header{};

				if ( !stream.read(header.data(), header.size()) )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), unable to read header !");

					return false;
				}

				/* Check Magic "EE3D_V1" (8 bytes including null) */
				if ( std::string_view(header.data(), 7) != Magic )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), invalid magic !");

					return false;
				}

				/* Check Version (2 bytes) at offset 8 */
				uint16_t version = 0;
				std::memcpy(&version, &header[8], sizeof(uint16_t));

				/* ⚠️⚠️ The payload holds RAW records: ANY change to ShapeVertex or ShapeTriangle changes the on-disk
				 * layout and bumps the version. Version 1 vertices were 80 bytes, version 2 (the tangent handedness) 84,
				 * version 3 (the secondary texture coordinates) 92; versions 2 and 3 stored the skinning in the vertex
				 * and the color and edge indexes in the triangle (64 bytes). Version 4 (2026-11-26) stores the compact
				 * records (60 and 40 bytes, float / uint32_t) and the skinning and color indexes as optional tables after
				 * the colors (header byte 12). Versions 2 and 3 are still read, record by record through their former
				 * layout (LegacyVertex, LegacyTriangle). There is deliberately NO version-1 read path: a v1 blob read with
				 * a later stride would misparse SILENTLY. */
				if ( version != 2 && version != 3 && version != WrittenVersion )
				{
					Logging::error("VertexFactory::FileFormatNative", std::string{"readStream(), unsupported version "} + std::to_string(version) + " — this build reads versions 2 to 4; a version-1 file predates the tangent handedness and must be re-exported from its source asset !");

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

				const auto flags = version == WrittenVersion ? static_cast< uint8_t >(header[FlagsOffset]) : uint8_t{0};

				/* A flag this build does not know announces a table it cannot skip: read on, every count would be wrong. */
				if ( (flags & static_cast< uint8_t >(~(SkinningFlag | ColorIndexesFlag))) != 0 )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), unknown optional tables (flags " + std::to_string(flags) + ") !");

					return false;
				}

				/* 2. Read Metadata (Counts) - 3 * 8 bytes = 24 bytes */
				std::array< uint64_t, 3 > counts{};

				if ( !stream.read(counts.data(), sizeof(counts)) )
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

				const uint64_t vertexSize = FileFormatNative::vertexRecordSize(version);
				const uint64_t triangleSize = version == WrittenVersion ? sizeof(ShapeTriangle< vertex_data_t, index_data_t >) : sizeof(LegacyTriangle);
				constexpr uint64_t colorSize = sizeof(Math::Vector< 4, vertex_data_t >);
				constexpr uint64_t skinningSize = sizeof(Math::Vector< 4, int32_t >) + sizeof(Math::Vector< 4, vertex_data_t >);
				constexpr uint64_t colorIndexesSize = 3 * sizeof(index_data_t);

				const auto fits = [&remaining] (uint64_t count, uint64_t elementSize, const char * what) noexcept {
					if ( count > remaining / elementSize )
					{
						Logging::error("VertexFactory::FileFormatNative", std::string{"readStream(), "} + what + " count exceeds the stream size !");

						return false;
					}

					remaining -= count * elementSize;

					return true;
				};

				if ( !fits(vertexCount, vertexSize, "vertex") || !fits(triangleCount, triangleSize, "triangle") || !fits(colorCount, colorSize, "color") ||
					((flags & SkinningFlag) != 0 && !fits(vertexCount, skinningSize, "skinning")) ||
					((flags & ColorIndexesFlag) != 0 && !fits(triangleCount, colorIndexesSize, "color index")) )
				{
					return false;
				}

				/* 4. Resize Geometry (counts now proven to fit). */
				geometry.vertices().resize(vertexCount);
				geometry.triangles().resize(triangleCount);
				geometry.vertexColors().resize(colorCount);

				/* 5. Read Data Blobs */
				const bool recordsRead = version == WrittenVersion ? FileFormatNative::readRecords(stream, geometry) : FileFormatNative::readLegacyRecords(stream, geometry, version);

				if ( !recordsRead )
				{
					return false;
				}

				if ( colorCount > 0 && !stream.read(geometry.vertexColors().data(), colorCount * colorSize) )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), failed to read vertex colors !");

					return false;
				}

				return version != WrittenVersion || FileFormatNative::readTables(stream, geometry, flags);
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

				const bool withColorIndexes = !geometry.vertexColors().empty();
				const auto flags = static_cast< uint8_t >((geometry.isSkinningAvailable() ? SkinningFlag : 0U) | (withColorIndexes ? ColorIndexesFlag : 0U));

				/* 1. Header (32 bytes) */
				std::array< char, 32 > header{};

				std::memcpy(header.data(), Magic, 7);
				std::memcpy(&header[8], &WrittenVersion, sizeof(uint16_t));

				header[10] = static_cast< char >(sizeof(vertex_data_t));
				header[11] = static_cast< char >(sizeof(index_data_t));
				header[FlagsOffset] = static_cast< char >(flags);

				/* 2. Metadata (Counts) */
				const std::array< uint64_t, 3 > counts{
					static_cast< uint64_t >(geometry.vertices().size()),
					static_cast< uint64_t >(geometry.triangles().size()),
					static_cast< uint64_t >(geometry.vertexColors().size())
				};

				if ( !stream.write(header.data(), header.size()) || !stream.write(counts.data(), sizeof(counts)) )
				{
					return false;
				}

				/* 3. Data Blobs, then the optional tables. */
				if ( (counts[0] > 0 && !stream.write(geometry.vertices().data(), counts[0] * sizeof(ShapeVertex< vertex_data_t >))) ||
					(counts[1] > 0 && !stream.write(geometry.triangles().data(), counts[1] * sizeof(ShapeTriangle< vertex_data_t, index_data_t >))) ||
					(counts[2] > 0 && !stream.write(geometry.vertexColors().data(), counts[2] * sizeof(Math::Vector< 4, vertex_data_t >))) )
				{
					return false;
				}

				if ( (flags & SkinningFlag) != 0 )
				{
					for ( index_data_t vertexIndex = 0; vertexIndex < counts[0]; ++vertexIndex )
					{
						if ( !stream.write(geometry.vertexInfluences(vertexIndex).data(), sizeof(Math::Vector< 4, int32_t >)) || !stream.write(geometry.vertexWeights(vertexIndex).data(), sizeof(Math::Vector< 4, vertex_data_t >)) )
						{
							return false;
						}
					}
				}

				if ( withColorIndexes )
				{
					for ( index_data_t triangleIndex = 0; triangleIndex < counts[1]; ++triangleIndex )
					{
						const std::array< index_data_t, 3 > colorIndexes{geometry.triangleVertexColorIndex(triangleIndex, 0), geometry.triangleVertexColorIndex(triangleIndex, 1), geometry.triangleVertexColorIndex(triangleIndex, 2)};

						if ( !stream.write(colorIndexes.data(), sizeof(colorIndexes)) )
						{
							return false;
						}
					}
				}

				return true;
			}

		private:

			/**
			 * @brief The vertex record of versions 2 and 3: ShapeVertex as it was before 2026-11-26, the skinning
			 * inside. Version 2 is its prefix up to the secondary texture coordinates.
			 */
			struct LegacyVertex
			{
				Math::Vector< 3, vertex_data_t > position;
				Math::Vector< 3, vertex_data_t > tangent;
				Math::Vector< 3, vertex_data_t > normal;
				Math::Vector< 3, vertex_data_t > textureCoordinates;
				Math::Vector< 4, int32_t > influences;
				Math::Vector< 4, vertex_data_t > weights;
				vertex_data_t tangentHandedness{1};
				Math::Vector< 2, vertex_data_t > secondaryTextureCoordinates;
			};

			/**
			 * @brief The triangle record of versions 2 and 3: ShapeTriangle as it was before 2026-11-26, the color and
			 * edge indexes inside.
			 */
			struct LegacyTriangle
			{
				Math::Vector< 3, vertex_data_t > surfaceTangent;
				Math::Vector< 3, vertex_data_t > surfaceNormal;
				vertex_data_t surfaceTangentHandedness{1};
				std::array< index_data_t, 3 > vertexIndexes{};
				std::array< index_data_t, 3 > vertexColorIndexes{};
				std::array< index_data_t, 3 > edgeIndexes{};
			};

			/* A version-2 record is the exact prefix of a version-3 one: no padding, the secondary set last. */
			static_assert(sizeof(LegacyVertex) == offsetof(LegacyVertex, secondaryTextureCoordinates) + sizeof(Math::Vector< 2, vertex_data_t >), "the version-3 vertex record has padding: a version-2 record would not be its prefix.");

			/* The version-4 records are written raw: 15 components a vertex (position, tangent, normal, texture coordinates
			 * W included, handedness, secondary set), no padding. A change bumps the version. */
			static_assert(sizeof(ShapeVertex< vertex_data_t >) == 15 * sizeof(vertex_data_t), "ShapeVertex no longer matches the version-4 record: bump FileFormatNative's version.");
			/* 7 components and 3 indexes a triangle, rounded to its alignment (a double shape with 32-bit indexes pads 4
			 * bytes; a float one does not). */
			static_assert(sizeof(ShapeTriangle< vertex_data_t, index_data_t >) == ((((7 * sizeof(vertex_data_t)) + (3 * sizeof(index_data_t)) + alignof(ShapeTriangle< vertex_data_t, index_data_t >) - 1) / alignof(ShapeTriangle< vertex_data_t, index_data_t >)) * alignof(ShapeTriangle< vertex_data_t, index_data_t >)), "ShapeTriangle no longer matches the version-4 record: bump FileFormatNative's version.");

			/**
			 * @brief Returns the size of a vertex record of a version.
			 * @param version The file version (2, 3 or 4).
			 * @return uint64_t
			 */
			[[nodiscard]]
			static
			uint64_t
			vertexRecordSize (uint16_t version) noexcept
			{
				if ( version == WrittenVersion )
				{
					return sizeof(ShapeVertex< vertex_data_t >);
				}

				return version == 3 ? sizeof(LegacyVertex) : offsetof(LegacyVertex, secondaryTextureCoordinates);
			}

			/**
			 * @brief Reads the vertex and triangle records of a version-4 file, raw.
			 * @param stream A reference to the stream.
			 * @param geometry A reference to the shape, its tables sized.
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			readRecords (IO::ByteStream & stream, Shape< vertex_data_t, index_data_t > & geometry) noexcept
			{
				auto & vertices = geometry.vertices();
				auto & triangles = geometry.triangles();

				if ( !vertices.empty() && !stream.read(vertices.data(), vertices.size() * sizeof(ShapeVertex< vertex_data_t >)) )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), failed to read vertices !");

					return false;
				}

				if ( !triangles.empty() && !stream.read(triangles.data(), triangles.size() * sizeof(ShapeTriangle< vertex_data_t, index_data_t >)) )
				{
					Logging::error("VertexFactory::FileFormatNative", "readStream(), failed to read triangles !");

					return false;
				}

				return true;
			}

			/**
			 * @brief Reads the vertex and triangle records of a version-2 or version-3 file through their former layout:
			 * the skinning goes to the shape's table when a vertex has some, the color indexes when the file has colors;
			 * the edge indexes are not kept (the edges are not stored).
			 * @param stream A reference to the stream.
			 * @param geometry A reference to the shape, its vertices and triangles sized.
			 * @param version The file version (2 or 3).
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			readLegacyRecords (IO::ByteStream & stream, Shape< vertex_data_t, index_data_t > & geometry, uint16_t version) noexcept
			{
				const auto vertexSize = FileFormatNative::vertexRecordSize(version);

				for ( index_data_t vertexIndex = 0; vertexIndex < geometry.vertices().size(); ++vertexIndex )
				{
					LegacyVertex record{};

					if ( !stream.read(&record, vertexSize) )
					{
						Logging::error("VertexFactory::FileFormatNative", "readStream(), failed to read vertices !");

						return false;
					}

					auto & vertex = geometry.vertices()[vertexIndex];
					vertex.setPosition(record.position);
					vertex.setTangent(record.tangent);
					vertex.setTangentHandedness(record.tangentHandedness);
					vertex.setNormal(record.normal);
					vertex.setTextureCoordinates(record.textureCoordinates);
					vertex.setSecondaryTextureCoordinates(record.secondaryTextureCoordinates);

					if ( record.influences != Math::Vector< 4, int32_t >{-1, -1, -1, -1} || record.weights != Math::Vector< 4, vertex_data_t >{} )
					{
						geometry.setVertexSkinning(vertexIndex, record.influences, record.weights);
					}
				}

				const bool withColors = !geometry.vertexColors().empty();

				for ( index_data_t triangleIndex = 0; triangleIndex < geometry.triangles().size(); ++triangleIndex )
				{
					LegacyTriangle record{};

					if ( !stream.read(&record, sizeof(LegacyTriangle)) )
					{
						Logging::error("VertexFactory::FileFormatNative", "readStream(), failed to read triangles !");

						return false;
					}

					auto & triangle = geometry.triangles()[triangleIndex];
					triangle = ShapeTriangle< vertex_data_t, index_data_t >{record.vertexIndexes[0], record.vertexIndexes[1], record.vertexIndexes[2]};
					triangle.setSurfaceTangent(record.surfaceTangent);
					triangle.setSurfaceNormal(record.surfaceNormal);
					triangle.setSurfaceTangentHandedness(record.surfaceTangentHandedness);

					if ( withColors )
					{
						geometry.setTriangleVertexColorIndexes(triangleIndex, record.vertexColorIndexes);
					}
				}

				return true;
			}

			/**
			 * @brief Reads the optional tables of a version-4 file into the shape.
			 * @param stream A reference to the stream.
			 * @param geometry A reference to the shape.
			 * @param flags The optional tables the header declares.
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			readTables (IO::ByteStream & stream, Shape< vertex_data_t, index_data_t > & geometry, uint8_t flags) noexcept
			{
				if ( (flags & SkinningFlag) != 0 )
				{
					for ( index_data_t vertexIndex = 0; vertexIndex < geometry.vertices().size(); ++vertexIndex )
					{
						Math::Vector< 4, int32_t > influences{};
						Math::Vector< 4, vertex_data_t > weights{};

						if ( !stream.read(influences.data(), sizeof(influences)) || !stream.read(weights.data(), sizeof(weights)) )
						{
							Logging::error("VertexFactory::FileFormatNative", "readStream(), failed to read the skinning !");

							return false;
						}

						geometry.setVertexSkinning(vertexIndex, influences, weights);
					}
				}

				if ( (flags & ColorIndexesFlag) != 0 )
				{
					for ( index_data_t triangleIndex = 0; triangleIndex < geometry.triangles().size(); ++triangleIndex )
					{
						std::array< index_data_t, 3 > colorIndexes{};

						if ( !stream.read(colorIndexes.data(), sizeof(colorIndexes)) )
						{
							Logging::error("VertexFactory::FileFormatNative", "readStream(), failed to read the color indexes !");

							return false;
						}

						geometry.setTriangleVertexColorIndexes(triangleIndex, colorIndexes);
					}
				}

				return true;
			}
	};
}
