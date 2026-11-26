/*
 * src/VertexFactory/FileFormatOBJ.hpp
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

/* Project configuration. */
#include "emeraude_base_config.hpp"
#include "emeraude_platform.hpp"

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

/* Local inclusions for inheritances. */
#include "FileFormatInterface.hpp"

/* Local inclusions for usages. */
#include "BaseUtility.hpp"
#include "IO/ByteStream.hpp"
#include "IO/LineReader.hpp"
#include "IO/StreamLineReader.hpp"
#include "Logging/Logging.hpp"
#include "VertexFactory/ShapeLoadResult.hpp"

/* Local inclusions for usages. */
#include "Math/Vector.hpp"
#include "Shape.hpp"
#include "ShapeTriangle.hpp"
#include "ShapeVertex.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief An OBJ vertex structure.
	 * @tparam index_data_t The precision type of index data.
	 * @note OBJ format use index starting at 1.
	 */
	template< typename index_data_t >
	requires (std::is_unsigned_v< index_data_t > )
	class OBJVertex final
	{
		public:

			explicit
			OBJVertex (index_data_t vIndex) noexcept
				: m_vIndex{vIndex}
			{

			}

			OBJVertex (index_data_t vIndex, index_data_t vtIndex) noexcept
				: m_vIndex{vIndex},
				m_vtIndex{vtIndex}
			{

			}

			OBJVertex (index_data_t vIndex, index_data_t vtIndex, index_data_t vnIndex) noexcept
				: m_vIndex{vIndex},
				m_vtIndex{vtIndex},
				m_vnIndex{vnIndex}
			{

			}

			[[nodiscard]]
			index_data_t
			vIndex () const noexcept
			{
				return m_vIndex;
			}

			[[nodiscard]]
			index_data_t
			vtIndex () const noexcept
			{
				return m_vtIndex;
			}

			[[nodiscard]]
			index_data_t
			vnIndex () const noexcept
			{
				return m_vnIndex;
			}

		private:

			index_data_t m_vIndex{0};
			index_data_t m_vtIndex{0};
			index_data_t m_vnIndex{0};
	};

	/**
	 * @brief An OBJ triangle structure.
	 * @tparam index_data_t The precision type of index data.
	 */
	template< std::unsigned_integral index_data_t >
	using OBJTriangle = std::array< OBJVertex< index_data_t >, 3 >;

	/**
	 * @brief The FileFormatOBJ class.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 * @tparam index_data_t The precision type of index data. Default uint32_t.
	 * @note http://www.fileformat.info/format/wavefrontobj/egff.htm
	 * @extends EmEn::Base::VertexFactory::FileFormatInterface
	 */
	template< typename vertex_data_t = float, typename index_data_t = uint32_t >
	requires (std::is_floating_point_v< vertex_data_t > && std::is_unsigned_v< index_data_t > )
	class FileFormatOBJ final : public FileFormatInterface< vertex_data_t, index_data_t >
	{
		public:

			/**
			 * @brief Constructs an OBJ file format.
			 */
			FileFormatOBJ () noexcept = default;

			/** @copydoc EmEn::Base::VertexFactory::FileFormatInterface::readStream() */
			[[nodiscard]]
			bool
			readStream (IO::ByteStream & stream, ShapeLoadResult< vertex_data_t, index_data_t > & result, const ReadOptions & readOptions) noexcept override
			{
				if ( !stream.isOpen() )
				{
					Logging::error("VertexFactory::FileFormatOBJ", "readStream(), stream is not open !");

					return false;
				}

				m_readOptions = readOptions;

				/* NOTE: A stream that seeks is read through a window, line by line: the file is never held whole (a 312 MB
				 * OBJ held 298 MiB of text for the whole parse, 2026-11-26). One that cannot seek is read whole first, its
				 * lines views into the buffer. See IO::StreamLineReader and IO::LineReader. */
				{
					IO::StreamLineReader input{stream};

					if ( input.isUsable() )
					{
						return this->readLines(input, result);
					}
				}

				const auto dataSize = stream.size();
				std::string buffer(dataSize, '\0');

				if ( !stream.read(buffer.data(), dataSize) )
				{
					Logging::error("VertexFactory::FileFormatOBJ", "readStream(), failed to read stream data !");

					return false;
				}

				IO::LineReader input{buffer};

				return this->readLines(input, result);
			}

			/** @copydoc EmEn::Base::VertexFactory::FileFormatInterface::writeStream() */
			[[nodiscard]]
			bool
			writeStream (IO::ByteStream & stream, const Shape< vertex_data_t, index_data_t > & geometry, const WriteOptions & /*writeOptions*/) const noexcept override
			{
				if ( !geometry.isValid() )
				{
					Logging::error("VertexFactory::FileFormatOBJ", "writeStream(), geometry is invalid !");

					return false;
				}

				std::ostringstream output;

				/* NOTE: Every float written back exactly (max_digits10: 9 for a float), in the classic locale whatever the
				 * global one. The default precision (6 digits) wrote 0.123456789 as 0.123457 and 1.00000012 as 1. */
				output.imbue(std::locale::classic());
				output << std::setprecision(std::numeric_limits< vertex_data_t >::max_digits10);

				/* Header */
				output << "# Exported by Emeraude-Engine" << "\n";
				output << "o Geometry" << "\n";

				/* Write Vertices (v) */
				for ( const auto & vertex : geometry.vertices() )
				{
					const auto & pos = vertex.position();

					/* OBJ does not enforce index, just order. */
					output << "v " << pos.x() << " " << pos.y() << " " << pos.z() << "\n";
				}

				/* Write Texture Coordinates (vt) */
				/* We write them even if zero/invalid if we want to rely on the same index,
				   OR we write them and keep a mapping.
				   The simplest approach for indexed geometry in Shape is to assume every vertex has unique attributes combination.
				   So we just dump all attributes in order and reference them by same index.
				*/
				if ( geometry.isTextureCoordinatesAvailable() )
				{
					for ( const auto & vertex : geometry.vertices() )
					{
						const auto & uv = vertex.textureCoordinates();

						output << "vt " << uv.x() << " " << uv.y() << "\n";
					}
				}

				/* Write Normals (vn) - only if available */
				const bool hasNormals = geometry.isNormalsAvailable();

				if ( hasNormals )
				{
					for ( const auto & vertex : geometry.vertices() )
					{
						const auto & n = vertex.normal();

						output << "vn " << n.x() << " " << n.y() << " " << n.z() << "\n";
					}
				}

				/* Write Faces (f) */
				/* OBJ indices are 1-based */
				const bool hasUV = geometry.isTextureCoordinatesAvailable();

				output << "s off" << "\n";

				for ( const auto & triangle : geometry.triangles() )
				{
					output << "f";

					for ( index_data_t index = 0; index < 3; ++index )
					{
						const index_data_t idx = triangle.vertexIndex(index) + 1;

						if ( hasUV && hasNormals )
						{
							/* f v/vt/vn */
							output << " " << idx << "/" << idx << "/" << idx;
						}
						else if ( hasUV )
						{
							/* f v/vt */
							output << " " << idx << "/" << idx;
						}
						else if ( hasNormals )
						{
							/* f v//vn */
							output << " " << idx << "//" << idx;
						}
						else
						{
							/* f v */
							output << " " << idx;
						}
					}

					output << "\n";
				}

				const std::string content = output.str();

				return stream.write(content.data(), content.size());
			}

		private:

			/**
			 * @brief Reads the file twice through a line reader: once to size the tables, once to assemble the faces.
			 * @tparam line_reader_t IO::LineReader (a buffer read whole) or IO::StreamLineReader (a stream read through a
			 * window).
			 * @param input A reference to the line reader over the file.
			 * @param result A reference to the load result.
			 * @return bool
			 */
			template< typename line_reader_t >
			[[nodiscard]]
			bool
			readLines (line_reader_t & input, ShapeLoadResult< vertex_data_t, index_data_t > & result) noexcept
			{
				auto & geometry = result.shape;

				/* Read a first time to calculate the space required for the structure. */
				if ( !this->analyseFileContent(input) || !linesIntact(input) )
				{
					Logging::error("VertexFactory::FileFormatOBJ", "readStream(), step 1 'Reserving space' has failed !");

					return false;
				}

				if ( !rewindLines(input) )
				{
					Logging::error("VertexFactory::FileFormatOBJ", "readStream(), the stream did not go back to its start for the second pass !");

					return false;
				}

				/* Read a second time to assemble faces. */
				const bool buildSuccess = geometry.build([this, &geometry, &input] (std::vector< std::pair< index_data_t, index_data_t > > & groups, std::vector< ShapeVertex< vertex_data_t > > & vertices, std::vector< ShapeTriangle< vertex_data_t, index_data_t > > & triangles) {
					if ( m_faceMode == FaceMode::Undetermined )
					{
						return false;
					}

					const bool assembled = m_faceMode == FaceMode::V ?
						this->parseFaceAssemblyV(input, groups, vertices, triangles) :
						this->parseFaceAssemblyIndexed(input, groups, vertices, triangles);

					return assembled && this->computeRequestedVectors(geometry);
				}, !m_vt.empty(), false);

				if ( !buildSuccess )
				{
					return false;
				}

				/* NOTE: A failed read of the stream ends the lines early: the shape would be the start of the file. */
				if ( !linesIntact(input) )
				{
					Logging::error("VertexFactory::FileFormatOBJ", "readStream(), a read of the stream failed before its end !");

					geometry.clear();

					return false;
				}

				/* Set the normals flag if normals were read from file. */
				if ( !m_vn.empty() )
				{
					geometry.declareNormalsAvailable();
				}

				return true;
			}

			/**
			 * @brief Returns whether a buffer's lines were all given: always.
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			linesIntact (const IO::LineReader & /*input*/) noexcept
			{
				return true;
			}

			/**
			 * @brief Returns whether a stream's lines were all given: no read of it failed.
			 * @param input A reference to the line reader.
			 * @return bool
			 */
			[[nodiscard]]
			static
			bool
			linesIntact (const IO::StreamLineReader & input) noexcept
			{
				return !input.hasFailed();
			}

			/**
			 * @brief Goes back to the first line of a buffer.
			 * @param input A reference to the line reader.
			 * @return bool Always true.
			 */
			[[nodiscard]]
			static
			bool
			rewindLines (IO::LineReader & input) noexcept
			{
				input.rewind();

				return true;
			}

			/**
			 * @brief Goes back to the first line of a stream.
			 * @param input A reference to the line reader.
			 * @return bool False when the stream refused the seek.
			 */
			[[nodiscard]]
			static
			bool
			rewindLines (IO::StreamLineReader & input) noexcept
			{
				return input.rewind();
			}

			/**
			 * @brief Computes the normals and tangents the read options request and the file does not give, by face mode.
			 * @param geometry A reference to the shape being built.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			computeRequestedVectors (Shape< vertex_data_t, index_data_t > & geometry) const noexcept
			{
				switch ( m_faceMode )
				{
					case FaceMode::V :
						return this->computeForPositions(geometry);

					case FaceMode::V_VN :
						return this->computeForNormals(geometry);

					case FaceMode::V_VT :
						return this->computeForTextureCoordinates(geometry);

					case FaceMode::V_VT_VN :
						return this->computeForEveryAttribute(geometry);

					case FaceMode::Undetermined :
						return false;
				}

				/* NOTE: Every enumerator is listed (decision D14: no default); a cast can still bring another value. */
				return false;
			}

			/**
			 * @brief Returns whether the computed normals are inverted: an odd number of axis reflections reverses the cross
			 * product direction without changing the triangle winding used by the GPU, so the normals must be inverted to
			 * point outward.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			invertsNormals () const noexcept
			{
				return (m_readOptions.flipXAxis != m_readOptions.flipYAxis) != m_readOptions.flipZAxis;
			}

			/**
			 * @brief The requested vectors of a file of positions only (V face mode).
			 * @param geometry A reference to the shape being built.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			computeForPositions (Shape< vertex_data_t, index_data_t > & geometry) const noexcept
			{
				if ( m_readOptions.requestTangentSpace )
				{
					return geometry.computeTriangleNormal(this->invertsNormals()) && geometry.computeTriangleTangentFromEdge() && geometry.computeVertexNormal() && geometry.computeVertexTangent();
				}

				if ( m_readOptions.requestNormal )
				{
					return geometry.computeTriangleNormal(this->invertsNormals()) && geometry.computeVertexNormal();
				}

				return true;
			}

			/**
			 * @brief The requested vectors of a file giving its normals (V_VN face mode).
			 * @note The per-vertex normals from the file are already correct, but computeTriangleTangentFromEdge() needs
			 * per-triangle surface normals to project the edge onto the face plane.
			 * @param geometry A reference to the shape being built.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			computeForNormals (Shape< vertex_data_t, index_data_t > & geometry) const noexcept
			{
				if ( m_readOptions.requestTangentSpace )
				{
					return geometry.computeTriangleNormal(this->invertsNormals()) && geometry.computeTriangleTangentFromEdge() && geometry.computeVertexTangent();
				}

				return true;
			}

			/**
			 * @brief The requested vectors of a file giving its texture coordinates (V_VT face mode).
			 * @param geometry A reference to the shape being built.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			computeForTextureCoordinates (Shape< vertex_data_t, index_data_t > & geometry) const noexcept
			{
				if ( m_readOptions.requestTangentSpace )
				{
					return geometry.computeTriangleTBNSpace() && geometry.computeVertexTBNSpace();
				}

				if ( m_readOptions.requestNormal )
				{
					return geometry.computeTriangleNormal() && geometry.computeVertexNormal();
				}

				return true;
			}

			/**
			 * @brief The requested vectors of a file giving its normals and texture coordinates (V_VT_VN face mode).
			 * @param geometry A reference to the shape being built.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			computeForEveryAttribute (Shape< vertex_data_t, index_data_t > & geometry) const noexcept
			{
				if ( m_readOptions.requestTangentSpace )
				{
					return geometry.computeTriangleTangent() && geometry.computeVertexTangent();
				}

				return true;
			}

			/**
			 * @brief How the face are declared in the file.
			 * @note Values are ordered by "completeness" to allow comparison.
			 */
			enum class FaceMode : uint8_t
			{
				Undetermined,
				/** @brief F Line format : "f v1 v2 v3" */
				V,
				/** @brief F Line format : "f v1//vn1 v2//vn2 v3//vn3" */
				V_VN,
				/** @brief F Line format : "f v1/vt1 v2/vt2 v3/vt3" */
				V_VT,
				/** @brief F Line format : "f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3" */
				V_VT_VN
			};

			/** @brief The index standing for an attribute a face corner does not give, in a vertex key. */
			static constexpr auto NoAttribute{std::numeric_limits< index_data_t >::max()};

			/**
			 * @brief An OBJ vertex is the whole (v, vt, vn) triple, 0-based, plus the SIDE of its face's UV mapping (1 when
			 * mirrored, see uvSide()): the key of one shape vertex.
			 */
			using VertexKey = std::array< index_data_t, 4 >;

			/** @brief Hashes a vertex key (multiplicative mixing of the four entries). */
			struct VertexKeyHash final
			{
				[[nodiscard]]
				size_t
				operator() (const VertexKey & key) const noexcept
				{
					constexpr std::uint64_t Multiplier{0x9E3779B97F4A7C15ULL};

					/* NOTE: The first round is (0 · Multiplier) ^ key[0] = key[0]. Not named 'hash': cppcheck 2.22
					 * confuses it with std::hash in an instantiating TU and reports its stores unread. */
					std::uint64_t seed{0};

					for ( const auto index : key )
					{
						seed = (seed * Multiplier) ^ static_cast< std::uint64_t >(index);
					}

					return static_cast< size_t >(seed ^ (seed >> 32U));
				}
			};

			/** @brief The shape vertex of every (v, vt, vn, side) key already met. */
			using VertexIndexMap = std::unordered_map< VertexKey, index_data_t, VertexKeyHash >;

			/**
			 * @brief Returns the side of a face triangle's UV mapping: 1 when it is MIRRORED (its UV determinant
			 * r = Δu1·Δv2 − Δu2·Δv1 is positive, Shape::triangleTangentFrame()), 0 otherwise or without texture coordinates.
			 * @note A position shared by both sides with the same (vt, vn) sits on a mirror seam; its tangent frames are
			 * opposite, so it is two shape vertices (MikkTSpace splits them the same way).
			 * @param faceIndices The face corners.
			 * @param triangleOffset The fan offset of the triangle (corners 0, 1 + offset, 2 + offset).
			 * @return index_data_t
			 */
			[[nodiscard]]
			index_data_t
			uvSide (const std::vector< OBJVertex< index_data_t > > & faceIndices, index_data_t triangleOffset) const noexcept
			{
				const auto cornerTextureCoordinates = [this, &faceIndices] (size_t corner) noexcept -> std::optional< Math::Vector< 3, vertex_data_t > > {
					if ( corner >= faceIndices.size() )
					{
						return std::nullopt;
					}

					const auto vtIndex = faceIndices[corner].vtIndex();

					if ( vtIndex == 0 || vtIndex > m_vt.size() )
					{
						return std::nullopt;
					}

					return m_vt[vtIndex - 1];
				};

				const auto uvA = cornerTextureCoordinates(0);
				const auto uvB = cornerTextureCoordinates(1 + static_cast< size_t >(triangleOffset));
				const auto uvC = cornerTextureCoordinates(2 + static_cast< size_t >(triangleOffset));

				if ( !uvA.has_value() || !uvB.has_value() || !uvC.has_value() )
				{
					return 0;
				}

				const auto determinant = (((*uvB)[Math::X] - (*uvA)[Math::X]) * ((*uvC)[Math::Y] - (*uvA)[Math::Y])) - (((*uvC)[Math::X] - (*uvA)[Math::X]) * ((*uvB)[Math::Y] - (*uvA)[Math::Y]));

				return determinant > 0 ? 1 : 0;
			}

			/**
			 * @brief Returns the shape vertex of a face corner, appending it the first time its (v, vt, vn) triple is met.
			 * @note ⚠️ The whole triple is the key (2026-10-05): the loader used to key a vertex by ONE index (the most
			 * numerous attribute) and to append a vertex only when the POSITION differed, so a face sharing a position
			 * with another normal or texture coordinates (a hard edge, a UV seam) inherited the first face's — on
			 * basic-scenery's temple, 381 triangles a ZERO normal (black) and 4880 corners a foreign one.
			 * @note The side is the fourth entry (2026-10-07): a corner of a mirror seam is one vertex per side.
			 * @pre The indices are in range of m_v, m_vt, m_vn, or NoAttribute for vt and vn.
			 * @param key The 0-based (v, vt, vn) triple and the side.
			 * @param vertexIndexes A reference to the triples already met.
			 * @param vertices A reference to the shape vertices.
			 * @return std::optional< index_data_t > The shape vertex index, or nothing past the index type's capacity.
			 */
			[[nodiscard]]
			std::optional< index_data_t >
			vertexIndexOf (const VertexKey & key, VertexIndexMap & vertexIndexes, std::vector< ShapeVertex< vertex_data_t > > & vertices) const noexcept
			{
				if ( const auto known = vertexIndexes.find(key); known != vertexIndexes.end() )
				{
					return known->second;
				}

				/* NOTE: The maximum is NoAttribute, never a vertex index. */
				if ( vertices.size() >= static_cast< size_t >(NoAttribute) )
				{
					Logging::error("VertexFactory::FileFormatOBJ", "too many vertices for the index type !");

					return std::nullopt;
				}

				const auto vertexIndex = static_cast< index_data_t >(vertices.size());
				const auto normal = key[2] != NoAttribute ? m_vn[key[2]] : Math::Vector< 3, vertex_data_t >{};
				const auto textureCoordinates = key[1] != NoAttribute ? m_vt[key[1]] : Math::Vector< 3, vertex_data_t >{};

				vertices.emplace_back(m_v[key[0]], normal, textureCoordinates);
				vertexIndexes.emplace(key, vertexIndex);

				return vertexIndex;
			}

			/**
			 * @brief The blanks between the words of a line.
			 * @note '\r' is one: a CRLF line keeps it at its end, as std::getline() does.
			 */
			static constexpr std::string_view Blanks{" \t\r"};

			/**
			 * @brief What a line of the file declares, by its first word.
			 */
			enum class LineKind : uint8_t
			{
				Other,
				Position,
				TextureCoordinates,
				Normal,
				Face,
				Group
			};

			/**
			 * @brief The attributes a pass has read so far: a negative index of a face is relative to them.
			 */
			struct AttributeCounts
			{
				index_data_t positions{0};
				index_data_t textureCoordinates{0};
				index_data_t normals{0};
			};

			/**
			 * @brief Returns what a line declares, by its first word: "v" and "v\t" alike (2026-11-26: a tab after the
			 * keyword made the line nothing).
			 * @param line The line.
			 * @return LineKind
			 */
			[[nodiscard]]
			static
			LineKind
			lineKind (std::string_view line) noexcept
			{
				const auto keyword = line.substr(0, line.find_first_of(Blanks));

				if ( keyword == "v" )
				{
					return LineKind::Position;
				}

				if ( keyword == "vt" )
				{
					return LineKind::TextureCoordinates;
				}

				if ( keyword == "vn" )
				{
					return LineKind::Normal;
				}

				if ( keyword == "f" )
				{
					return LineKind::Face;
				}

				if ( keyword == "g" )
				{
					return LineKind::Group;
				}

				return LineKind::Other;
			}

			/**
			 * @brief Returns the words of a line after its keyword.
			 * @param line The line.
			 * @return std::string_view Empty when the keyword stands alone.
			 */
			[[nodiscard]]
			static
			std::string_view
			lineData (std::string_view line) noexcept
			{
				const auto keywordEnd = line.find_first_of(Blanks);

				if ( keywordEnd == std::string_view::npos )
				{
					return {};
				}

				const auto dataStart = line.find_first_not_of(Blanks, keywordEnd);

				return dataStart == std::string_view::npos ? std::string_view{} : line.substr(dataStart);
			}

			/**
			 * @brief Counts the words of a text, separated by blanks.
			 * @param data The text.
			 * @return size_t
			 */
			[[nodiscard]]
			static
			size_t
			countWords (std::string_view data) noexcept
			{
				size_t count = 0;
				auto position = data.find_first_not_of(Blanks);

				while ( position != std::string_view::npos )
				{
					++count;

					position = data.find_first_of(Blanks, position);

					if ( position != std::string_view::npos )
					{
						position = data.find_first_not_of(Blanks, position);
					}
				}

				return count;
			}

			/**
			 * @brief Determines how the OBJ format describe a face.
			 * @param data The words of the face line, after its keyword.
			 * @return FaceMode
			 */
			[[nodiscard]]
			static
			FaceMode
			determineFaceDeclarationMode (std::string_view data) noexcept
			{
				if ( data.find("//") != std::string_view::npos )
				{
					return FaceMode::V_VN;
				}

				const auto dashCount = static_cast< size_t >(std::ranges::count(data, '/'));

				if ( dashCount == 0 )
				{
					return FaceMode::V;
				}

				/* NOTE: Two slashes a corner ("v/vt/vn") outnumber the corners; one ("v/vt") does not. */
				if ( dashCount > countWords(data) )
				{
					return FaceMode::V_VT_VN;
				}

				return FaceMode::V_VT;
			}

			/**
			 * @brief Reads a first time to calculate the space required for the structure.
			 * @note Only the tables the face mode reads are allocated: none in the V mode, where a "v" line writes its
			 * shape vertex directly (2026-11-26: the position table cost 12 bytes a vertex, 45 MiB on a 3.9 M vertex file).
			 * @tparam line_reader_t The line reader type: IO::LineReader or IO::StreamLineReader.
			 * @param file A reference to the line reader over the file.
			 * @return bool
			 */
			template< typename line_reader_t >
			bool
			analyseFileContent (line_reader_t & file) noexcept
			{
				std::string_view line{};
				AttributeCounts counts{};

				while ( file.next(line) )
				{
					const auto kind = lineKind(line);

					if ( kind == LineKind::Position )
					{
						++counts.positions;
					}
					else if ( kind == LineKind::TextureCoordinates )
					{
						++counts.textureCoordinates;
					}
					else if ( kind == LineKind::Normal )
					{
						++counts.normals;
					}
					else if ( kind == LineKind::Face )
					{
						const auto data = lineData(line);

						/* NOTE: Detect the most complete face mode present in the file.
						 * Files may mix formats (e.g., V_VN and V_VT_VN), so we track the "highest" mode seen.
						 */
						const auto lineFaceMode = determineFaceDeclarationMode(data);

						if ( static_cast< uint8_t >(lineFaceMode) > static_cast< uint8_t >(m_faceMode) )
						{
							m_faceMode = lineFaceMode;
						}

						const auto vertexCount = countWords(data);

						if ( vertexCount < 3 )
						{
							Logging::error("VertexFactory::FileFormatOBJ", std::string{"analyseFileContent(), this OBJ loader requires at least 3 vertices per face (got "} + std::to_string(vertexCount) + ") !");

							return false;
						}

						/* NOTE: Polygons with N vertices are triangulated into (N-2) triangles using fan triangulation. */
						m_faceCount += static_cast< index_data_t >(vertexCount - 2);
					}
				}

				if constexpr ( VertexFactoryDebugEnabled )
				{
					Logging::debug("VertexFactory::FileFormatOBJ", std::string{"First pass: "} + std::to_string(counts.positions) + " vertices, " + std::to_string(counts.textureCoordinates) + " texture coordinates, " + std::to_string(counts.normals) + " normals, " + std::to_string(m_faceCount) + " faces.");
				}

				if ( counts.positions == 0 )
				{
					Logging::error("VertexFactory::FileFormatOBJ", "analyseFileContent(), there is no vertex definition in the OBJ file. Aborting.");

					return false;
				}

				if ( m_faceCount == 0 )
				{
					Logging::error("VertexFactory::FileFormatOBJ", "analyseFileContent(), there is no face definition in the OBJ file. Aborting.");

					return false;
				}

				m_positionCount = counts.positions;

				/* Resize the tables the face mode reads. */
				const bool withTextureCoordinates = m_faceMode == FaceMode::V_VT || m_faceMode == FaceMode::V_VT_VN;
				const bool withNormals = m_faceMode == FaceMode::V_VN || m_faceMode == FaceMode::V_VT_VN;

				if ( m_faceMode != FaceMode::V )
				{
					m_v.resize(counts.positions);
				}

				if ( withTextureCoordinates )
				{
					m_vt.resize(counts.textureCoordinates);
				}

				if ( withNormals )
				{
					m_vn.resize(counts.normals);
				}

				m_vertexCount = std::max(m_positionCount, static_cast< index_data_t >(std::max(m_vt.size(), m_vn.size())));

				return true;
			}

			/**
			 * @brief Reads the lines of the second pass: the attributes into their tables, the faces into triangles.
			 * @tparam line_reader_t The line reader type: IO::LineReader or IO::StreamLineReader.
			 * @tparam vertex_of_t A callable giving the shape vertex of a face corner and the side of its triangle,
			 * nothing on a refusal.
			 * @param file A reference to the line reader over the OBJ file.
			 * @param vertexOf The face mode's way to turn a corner into a shape vertex.
			 * @param withSides Whether the triangles carry the side of their UV mapping (uvSide()).
			 * @param groups A reference to an index list.
			 * @param vertices A reference to the vertex list.
			 * @param triangles A reference to the triangle list.
			 * @return bool
			 */
			template< typename line_reader_t, typename vertex_of_t >
			bool
			assembleFaces (line_reader_t & file, const vertex_of_t & vertexOf, bool withSides, std::vector< std::pair< index_data_t, index_data_t > > & groups, std::vector< ShapeVertex< vertex_data_t > > & vertices, std::vector< ShapeTriangle< vertex_data_t, index_data_t > > & triangles) noexcept
			{
				/* Keep count on attributes and triangles. */
				AttributeCounts read{};
				index_data_t triangleCount = 0;

				std::string_view objLine{};
				std::vector< OBJVertex< index_data_t > > faceIndices{};

				while ( file.next(objLine) )
				{
					const auto kind = lineKind(objLine);

					/* Declaring a group. */
					if ( kind == LineKind::Group )
					{
						if ( triangleCount > 0 )
						{
							groups.emplace_back(triangleCount, 0);
						}

						continue;
					}

					if ( kind != LineKind::Face )
					{
						if ( !this->readAttributeLine(kind, objLine, read, vertices) )
						{
							return false;
						}

						continue;
					}

					if ( !this->extractFaceIndices(lineData(objLine), read, faceIndices) || !this->addFanTriangles(faceIndices, vertexOf, withSides, groups, triangles, triangleCount) )
					{
						return false;
					}
				}

				return true;
			}

			/**
			 * @brief Adds the triangles of a face: an f line can express a polygon, split in a fan ("f 0 1 2 3" gives
			 * "0 1 2" and "0 2 3").
			 * @tparam vertex_of_t A callable giving the shape vertex of a face corner and the side of its triangle,
			 * nothing on a refusal.
			 * @param faceIndices The corners of the face, at least 3 (extractFaceIndices()).
			 * @param vertexOf The face mode's way to turn a corner into a shape vertex.
			 * @param withSides Whether the triangles carry the side of their UV mapping (uvSide()).
			 * @param groups A reference to an index list.
			 * @param triangles A reference to the triangle list.
			 * @param triangleCount A reference to the triangle count.
			 * @return bool
			 */
			template< typename vertex_of_t >
			[[nodiscard]]
			bool
			addFanTriangles (const std::vector< OBJVertex< index_data_t > > & faceIndices, const vertex_of_t & vertexOf, bool withSides, std::vector< std::pair< index_data_t, index_data_t > > & groups, std::vector< ShapeTriangle< vertex_data_t, index_data_t > > & triangles, index_data_t & triangleCount) const noexcept
			{
				/* NOTE: extractFaceIndices() guarantees at least 3 indices, so realFaceVertexIndex = triangleOffset + 2 at
				 * most stays below faceIndices.size(). */
				for ( index_data_t triangleOffset = 0; triangleOffset < faceIndices.size() - 2; ++triangleOffset )
				{
					ShapeTriangle< vertex_data_t, index_data_t > triangle;

					/* The side of this triangle's UV mapping: a mirror seam corner is one vertex per side. */
					const auto side = withSides ? this->uvSide(faceIndices, triangleOffset) : index_data_t{0};

					for ( index_data_t faceVertexIndex = 0; faceVertexIndex < 3; ++faceVertexIndex )
					{
						const auto realFaceVertexIndex = faceVertexIndex == 0 ? faceVertexIndex : faceVertexIndex + triangleOffset;
						const auto vertexIndex = vertexOf(faceIndices[realFaceVertexIndex], side);

						if ( !vertexIndex.has_value() )
						{
							return false;
						}

						/* Declare the vertex index to one of the three vertices of the triangle. */
						triangle.setVertexIndex(faceVertexIndex, vertexIndex.value());
					}

					triangles.emplace_back(triangle);

					++triangleCount;

					if ( !groups.empty() )
					{
						++groups.back().second;
					}
				}

				return true;
			}

			/**
			 * @brief Reads a file whose faces give positions only: "f v1 v2 v3".
			 * @note A shape vertex IS a position here: each "v" line writes its vertex directly, a corner names it.
			 * @tparam line_reader_t The line reader type: IO::LineReader or IO::StreamLineReader.
			 * @param file A reference to the line reader over the OBJ file.
			 * @param groups A reference to an index list.
			 * @param vertices A reference to the vertex list.
			 * @param triangles A reference to the triangle list.
			 * @return bool
			 */
			template< typename line_reader_t >
			bool
			parseFaceAssemblyV (line_reader_t & file, std::vector< std::pair< index_data_t, index_data_t > > & groups, std::vector< ShapeVertex< vertex_data_t > > & vertices, std::vector< ShapeTriangle< vertex_data_t, index_data_t > > & triangles) noexcept
			{
				vertices.resize(m_positionCount);
				triangles.reserve(m_faceCount);

				const auto vertexOf = [&vertices] (const OBJVertex< index_data_t > & corner, index_data_t /*side*/) noexcept -> std::optional< index_data_t > {
					/* NOTE: Convert OBJ index (1-based) to vector index (0-based); 0 wraps past every vertex. */
					const auto vIndex = corner.vIndex() - 1;

					if ( vIndex >= vertices.size() )
					{
						Logging::error("VertexFactory::FileFormatOBJ", "face references an out-of-range index !");

						return std::nullopt;
					}

					return vIndex;
				};

				return this->assembleFaces(file, vertexOf, false, groups, vertices, triangles);
			}

			/**
			 * @brief Reads a file whose faces give normals, texture coordinates or both: "f v1//vn1 ...", "f v1/vt1 ...",
			 * "f v1/vt1/vn1 ...".
			 * @note One shape vertex per (v, vt, vn) triple met (vertexIndexOf()); the face mode says which of vt and vn
			 * belong to it, as the three former functions of one form each did.
			 * @tparam line_reader_t The line reader type: IO::LineReader or IO::StreamLineReader.
			 * @param file A reference to the line reader over the OBJ file.
			 * @param groups A reference to an index list.
			 * @param vertices A reference to the vertex list.
			 * @param triangles A reference to the triangle list.
			 * @return bool
			 */
			template< typename line_reader_t >
			bool
			parseFaceAssemblyIndexed (line_reader_t & file, std::vector< std::pair< index_data_t, index_data_t > > & groups, std::vector< ShapeVertex< vertex_data_t > > & vertices, std::vector< ShapeTriangle< vertex_data_t, index_data_t > > & triangles) noexcept
			{
				const bool withTextureCoordinates = m_faceMode == FaceMode::V_VT || m_faceMode == FaceMode::V_VT_VN;
				const bool withNormals = m_faceMode == FaceMode::V_VN || m_faceMode == FaceMode::V_VT_VN;

				/* NOTE: Reserving memory space to the geometry shape: one vertex per (v, vt, vn) triple met. */
				vertices.reserve(m_vertexCount);
				triangles.reserve(m_faceCount);

				/* Keep track of the generated shape vertices. */
				VertexIndexMap vertexIndexes{};
				vertexIndexes.reserve(m_vertexCount);

				const auto vertexOf = [this, &vertexIndexes, &vertices, withTextureCoordinates, withNormals] (const OBJVertex< index_data_t > & corner, index_data_t side) noexcept -> std::optional< index_data_t > {
					/* NOTE: Convert OBJ index (1-based) to vector index (0-based). Index 0 means "not defined" in our system. */
					const auto vIndex = corner.vIndex() - 1;
					const bool hasTextureCoordinates = withTextureCoordinates && corner.vtIndex() > 0;
					const bool hasNormal = withNormals && corner.vnIndex() > 0;
					const auto vtIndex = hasTextureCoordinates ? corner.vtIndex() - 1 : NoAttribute;
					const auto vnIndex = hasNormal ? corner.vnIndex() - 1 : NoAttribute;

					if ( vIndex >= m_v.size() || ( hasTextureCoordinates && vtIndex >= m_vt.size() ) || ( hasNormal && vnIndex >= m_vn.size() ) )
					{
						Logging::error("VertexFactory::FileFormatOBJ", "face references an out-of-range index !");

						return std::nullopt;
					}

					/* NOTE: An OBJ vertex is the whole (v, vt, vn) triple: a position shared with another normal or texture
					 * coordinates (a hard edge, a UV seam) is another shape vertex. */
					return this->vertexIndexOf({vIndex, vtIndex, vnIndex, side}, vertexIndexes, vertices);
				};

				return this->assembleFaces(file, vertexOf, withTextureCoordinates, groups, vertices, triangles);
			}

			/**
			 * @brief Reads an attribute line ("v", "vt", "vn") of the second pass and counts it.
			 * @note A position goes straight to its shape vertex in the V face mode. An attribute whose table the face mode
			 * does not allocate (a normal of a V_VT file) is counted — a negative index stays right — but not read.
			 * @param kind What the line declares.
			 * @param line The line.
			 * @param read A reference to the attributes read so far.
			 * @param vertices A reference to the shape vertices (the V face mode writes the positions there).
			 * @return bool False when the attribute cannot be read.
			 */
			[[nodiscard]]
			bool
			readAttributeLine (LineKind kind, std::string_view line, AttributeCounts & read, std::vector< ShapeVertex< vertex_data_t > > & vertices) noexcept
			{
				if ( kind == LineKind::Position )
				{
					Math::Vector< 3, vertex_data_t > position{};

					if ( !this->readPosition(lineData(line), position) )
					{
						Logging::error("VertexFactory::FileFormatOBJ", std::string{"a vertex line needs three coordinates, each a number: '"} + std::string{line} + "'");

						return false;
					}

					if ( m_faceMode == FaceMode::V )
					{
						if ( read.positions < vertices.size() )
						{
							vertices[read.positions].setPosition(position);
						}
					}
					else if ( read.positions < m_v.size() )
					{
						m_v[read.positions] = position;
					}

					++read.positions;

					return true;
				}

				if ( kind == LineKind::TextureCoordinates )
				{
					if ( read.textureCoordinates < m_vt.size() && !this->readTextureCoordinates(lineData(line), m_vt[read.textureCoordinates]) )
					{
						Logging::error("VertexFactory::FileFormatOBJ", std::string{"a texture coordinates line needs a first coordinate, each one a number: '"} + std::string{line} + "'");

						return false;
					}

					++read.textureCoordinates;

					return true;
				}

				if ( kind == LineKind::Normal )
				{
					if ( read.normals < m_vn.size() && !this->readNormal(lineData(line), m_vn[read.normals]) )
					{
						Logging::error("VertexFactory::FileFormatOBJ", std::string{"a normal line needs three coordinates, each a number: '"} + std::string{line} + "'");

						return false;
					}

					++read.normals;

					return true;
				}

				return true;
			}

			/**
			 * @brief Parses a floating-point number at the start of a text.
			 * @note A leading '+' is read: the format allows it, std::from_chars does not (2026-11-26: "+1.5" was read as 0).
			 * @note Uses strtof on macOS (libc++ lacks std::from_chars for floats), std::from_chars elsewhere. strtof() needs
			 * a terminated string and the text is a view into a larger buffer: the number is copied first.
			 * @note Must use preprocessor "#if" instead of "if constexpr" because Clang checks availability
			 * attributes at lexical level, before constexpr evaluation.
			 * @param text The text, starting at the number.
			 * @param value A reference to the parsed value.
			 * @return size_t The characters of the number, 0 when the text does not start with one.
			 */
			[[nodiscard]]
			static
			size_t
			parseFloat (std::string_view text, float & value) noexcept
			{
				const size_t sign = text.size() > 1 && text.front() == '+' && text[1] != '+' && text[1] != '-' ? 1 : 0;
				const auto number = text.substr(sign);

#if IS_MACOS
				std::array< char, 64 > terminated{};
				number.copy(terminated.data(), std::min(number.find_first_of(Blanks), terminated.size() - 1));

				char * end = nullptr;
				value = std::strtof(terminated.data(), &end);

				const auto consumed = static_cast< size_t >(end - terminated.data());

				return consumed == 0 ? 0 : sign + consumed;
#else
				const auto * const first = std::to_address(number.begin());
				const auto result = std::from_chars(first, std::to_address(number.end()), value);

				return result.ec == std::errc{} ? sign + static_cast< size_t >(result.ptr - first) : 0;
#endif
			}

			/**
			 * @brief Reads the coordinates of an attribute line, after its keyword.
			 * @note A number ends at a blank or at the end of the line: "1,5" is not the number 1 (2026-11-26: a missing
			 * or unreadable coordinate was read as 0 without a word).
			 * @param data The words after the keyword.
			 * @param required The coordinates the line must give.
			 * @param coordinates The coordinates to read, at most; the words after them (a 'w', vertex colors) are ignored.
			 * @return bool False when a required coordinate is missing, or a word read is not a number.
			 */
			[[nodiscard]]
			static
			bool
			readCoordinates (std::string_view data, size_t required, std::span< float > coordinates) noexcept
			{
				size_t position = 0;
				size_t count = 0;

				for ( auto & coordinate : coordinates )
				{
					position = data.find_first_not_of(Blanks, position);

					if ( position == std::string_view::npos )
					{
						break;
					}

					const auto length = parseFloat(data.substr(position), coordinate);

					if ( length == 0 || ( position + length < data.size() && Blanks.find(data[position + length]) == std::string_view::npos ) )
					{
						return false;
					}

					position += length;
					++count;
				}

				return count >= required;
			}

			/**
			 * @brief Reads a position: three coordinates, flipped and scaled by the read options.
			 * @param data The words after the keyword.
			 * @param position A reference to the position.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			readPosition (std::string_view data, Math::Vector< 3, vertex_data_t > & position) const noexcept
			{
				std::array< float, 3 > coordinates{};

				if ( !readCoordinates(data, 3, coordinates) )
				{
					return false;
				}

				position[Math::X] = static_cast< vertex_data_t >(m_readOptions.flipXAxis ? -coordinates[0] : coordinates[0]);
				position[Math::Y] = static_cast< vertex_data_t >(m_readOptions.flipYAxis ? -coordinates[1] : coordinates[1]);
				position[Math::Z] = static_cast< vertex_data_t >(m_readOptions.flipZAxis ? -coordinates[2] : coordinates[2]);

				if ( Utility::different(m_readOptions.scaleFactor, 1.0F) )
				{
					position.scale(m_readOptions.scaleFactor);
				}

				return true;
			}

			/**
			 * @brief Reads texture coordinates: u, and v when given (0 otherwise).
			 * @param data The words after the keyword.
			 * @param textureCoordinates A reference to the texture coordinates.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			readTextureCoordinates (std::string_view data, Math::Vector< 3, vertex_data_t > & textureCoordinates) const noexcept
			{
				std::array< float, 2 > coordinates{};

				if ( !readCoordinates(data, 1, coordinates) )
				{
					return false;
				}

				/* NOTE: flipU/flipV, not flipXAxis/flipYAxis: a texture coordinate is not a
				 * geometric axis, and reading the geometry flags here made one flag mean two
				 * unrelated things. */
				textureCoordinates[Math::X] = static_cast< vertex_data_t >(m_readOptions.flipU ? -coordinates[0] : coordinates[0]);
				textureCoordinates[Math::Y] = static_cast< vertex_data_t >(m_readOptions.flipV ? -coordinates[1] : coordinates[1]);

				return true;
			}

			/**
			 * @brief Reads a normal: three coordinates, flipped by the read options.
			 * @param data The words after the keyword.
			 * @param normal A reference to the normal.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			readNormal (std::string_view data, Math::Vector< 3, vertex_data_t > & normal) const noexcept
			{
				std::array< float, 3 > coordinates{};

				if ( !readCoordinates(data, 3, coordinates) )
				{
					return false;
				}

				normal[Math::X] = static_cast< vertex_data_t >(m_readOptions.flipXAxis ? -coordinates[0] : coordinates[0]);
				normal[Math::Y] = static_cast< vertex_data_t >(m_readOptions.flipYAxis ? -coordinates[1] : coordinates[1]);
				normal[Math::Z] = static_cast< vertex_data_t >(m_readOptions.flipZAxis ? -coordinates[2] : coordinates[2]);

				return true;
			}

			/**
			 * @brief Parses a single face vertex token (e.g., "1", "1/2", "1//3", "1/2/3").
			 * @param token The token to parse.
			 * @param vIndex Output for position index.
			 * @param vtIndex Output for texture coordinate index (0 if not present).
			 * @param vnIndex Output for normal index (0 if not present).
			 * @return bool True if parsing succeeded.
			 */
			[[nodiscard]]
			static
			bool
			parseFaceToken (std::string_view token, int32_t & vIndex, int32_t & vtIndex, int32_t & vnIndex) noexcept
			{
				vIndex = 0;
				vtIndex = 0;
				vnIndex = 0;

				const auto firstSlash = token.find('/');

				/* Format: "v" (position only). */
				if ( firstSlash == std::string_view::npos )
				{
					const auto result = std::from_chars(token.data(), token.data() + token.size(), vIndex);
					return result.ec == std::errc{};
				}

				/* Parse position index (before first slash). */
				{
					const auto result = std::from_chars(token.data(), token.data() + firstSlash, vIndex);

					if ( result.ec != std::errc{} )
					{
						return false;
					}
				}

				const auto secondSlash = token.find('/', firstSlash + 1);

				/* Format: "v//vn" (position and normal, no texture). */
				if ( firstSlash + 1 < token.size() && token[firstSlash + 1] == '/' )
				{
					const auto vnStart = firstSlash + 2;
					const auto result = std::from_chars(token.data() + vnStart, token.data() + token.size(), vnIndex);
					return result.ec == std::errc{};
				}

				/* Format: "v/vt" (position and texture, no normal). */
				if ( secondSlash == std::string_view::npos )
				{
					const auto vtStart = firstSlash + 1;
					const auto result = std::from_chars(token.data() + vtStart, token.data() + token.size(), vtIndex);
					return result.ec == std::errc{};
				}

				/* Format: "v/vt/vn" (position, texture, and normal). */
				{
					const auto vtStart = firstSlash + 1;
					const auto vtEnd = secondSlash;

					auto result = std::from_chars(token.data() + vtStart, token.data() + vtEnd, vtIndex);

					if ( result.ec != std::errc{} )
					{
						return false;
					}

					const auto vnStart = secondSlash + 1;
					result = std::from_chars(token.data() + vnStart, token.data() + token.size(), vnIndex);

					return result.ec == std::errc{};
				}
			}

			/**
			 * @brief Converts a potentially negative OBJ index to a positive index.
			 * @note OBJ uses 1-based indexing. A negative index counts back from the last attribute read SO FAR (the
			 * format's definition; 2026-11-26: the list's final size was used).
			 * @param index The index to convert.
			 * @param readSoFar The attributes of that kind read before the face.
			 * @return index_data_t The converted positive index (still 1-based).
			 */
			[[nodiscard]]
			static
			index_data_t
			resolveIndex (int32_t index, size_t readSoFar) noexcept
			{
				if ( index < 0 )
				{
					return static_cast< index_data_t >(static_cast< int64_t >(readSoFar) + index + 1);
				}

				return static_cast< index_data_t >(index);
			}

			/**
			 * @brief Returns the corners of a face line.
			 * @param data The words of the face line, after its keyword.
			 * @param read The attributes read so far, the base of a negative index.
			 * @param faceIndices A reference to the corners, cleared first (the vector is reused from face to face).
			 * @return bool
			 */
			[[nodiscard]]
			bool
			extractFaceIndices (std::string_view data, const AttributeCounts & read, std::vector< OBJVertex< index_data_t > > & faceIndices) const noexcept
			{
				faceIndices.clear();

				auto position = data.find_first_not_of(Blanks);

				while ( position != std::string_view::npos )
				{
					const auto end = data.find_first_of(Blanks, position);
					const auto token = data.substr(position, end == std::string_view::npos ? std::string_view::npos : end - position);

					/* Parse the token. */
					int32_t vIndex = 0;
					int32_t vtIndex = 0;
					int32_t vnIndex = 0;

					if ( !parseFaceToken(token, vIndex, vtIndex, vnIndex) )
					{
						Logging::error("VertexFactory::FileFormatOBJ", std::string{"extractFaceIndices(), failed to parse face token '"} + std::string{token} + "' in line: 'f " + std::string{data} + "'");

						return false;
					}

					/* Resolve negative indices and add to result. */
					faceIndices.emplace_back(
						resolveIndex(vIndex, read.positions),
						resolveIndex(vtIndex, read.textureCoordinates),
						resolveIndex(vnIndex, read.normals)
					);

					position = end == std::string_view::npos ? end : data.find_first_not_of(Blanks, end);
				}

				if ( faceIndices.size() < 3 )
				{
					Logging::error("VertexFactory::FileFormatOBJ", std::string{"extractFaceIndices(), face must have at least 3 vertices, got "} + std::to_string(faceIndices.size()) + ": 'f " + std::string{data} + "'");

					return false;
				}

				return true;
			}

			std::vector< Math::Vector< 3, vertex_data_t > > m_v;
			std::vector< Math::Vector< 3, vertex_data_t > > m_vt;
			std::vector< Math::Vector< 3, vertex_data_t > > m_vn;
			index_data_t m_positionCount{0}; /* The "v" lines of the file (the V face mode keeps no table of them). */
			index_data_t m_vertexCount{0}; /* The largest attribute count: a lower bound of the vertex count, for the reserve. */
			index_data_t m_faceCount{0};
			FaceMode m_faceMode{FaceMode::Undetermined};
			ReadOptions m_readOptions{};
	};
}
