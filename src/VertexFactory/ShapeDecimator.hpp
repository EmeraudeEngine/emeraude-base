/*
 * src/VertexFactory/ShapeDecimator.hpp
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
#include <algorithm>
#include <array>
#include <atomic>
#include <stop_token>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory_resource>
#include <queue>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/* Local inclusions for usages. */
#include "FlatHashMap.hpp"
#include "PixelFactory/Pixmap.hpp"
#include "ThreadPool.hpp"
#include "Shape.hpp"
#include "ShapeProcessor.hpp"

namespace EmEn::Base::VertexFactory
{
	/**
	 * @brief Mesh decimation using Quadric Error Metrics (Garland & Heckbert).
	 * @note Reduces polygon density while preserving overall shape, UV seams, and boundary edges.
	 * @tparam vertex_data_t The precision type of vertex data. Default float.
	 * @tparam index_data_t The precision type of index data. Default uint32_t.
	 */
	template< typename vertex_data_t = float, typename index_data_t = uint32_t >
	requires (std::is_floating_point_v< vertex_data_t > && std::is_unsigned_v< index_data_t >)
	class ShapeDecimator final
	{
		public:

			/**
			 * @brief Constructs a decimator for the given shape.
			 * @note The source shape is preserved for normal map baking (correct normals).
			 * A position-only dedup copy is created internally for QEM mesh connectivity.
			 * @param source The source high-poly shape (with correct normals/UVs).
			 * @param ratio Decimation ratio: 0.0 = maximum reduction, 1.0 = no reduction.
			 * @param boundaryPenaltyWeight Penalty multiplier for boundary/UV seam edges. Default 1000.
			 * @param normalMapResolution Resolution of the baked normal map (0 = disabled). Default 0.
			 */
			explicit
			ShapeDecimator (const Shape< vertex_data_t, index_data_t > & source, vertex_data_t ratio = static_cast< vertex_data_t >(0.5), vertex_data_t boundaryPenaltyWeight = static_cast< vertex_data_t >(1000), uint32_t normalMapResolution = 0, ThreadPool * threadPool = nullptr) noexcept
				: m_source(source),
				m_bakeSource(source),
				m_ratio(std::clamp(ratio, static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(1))),
				m_boundaryPenaltyWeight(boundaryPenaltyWeight),
				m_normalMapResolution(normalMapResolution),
				m_threadPool(threadPool)
			{

			}

			/** @brief Copy constructor. */
			ShapeDecimator (const ShapeDecimator &) noexcept = delete;

			/** @brief Move constructor. */
			ShapeDecimator (ShapeDecimator &&) noexcept = delete;

			/** @brief Copy assignment. */
			ShapeDecimator & operator= (const ShapeDecimator &) noexcept = delete;

			/** @brief Move assignment. */
			ShapeDecimator & operator= (ShapeDecimator &&) noexcept = delete;

			/** @brief Destructor. */
			~ShapeDecimator () = default;

			/**
			 * @brief Lets the caller interrupt decimate() through a std::stop_token (Ave Robustus II, decision D1): every
			 * stage checks it every CancellationCheckInterval iterations, and a requested stop makes decimate() return an
			 * EMPTY shape. The token comes from the TaskHandle of the job running the decimation.
			 * @param stopToken The token.
			 */
			void
			setStopToken (std::stop_token stopToken) noexcept
			{
				m_stopToken = std::move(stopToken);
			}

			/**
			 * @brief TRANSITIONAL: the engine's former global shutdown flag, still read until its LOD jobs own a TaskHandle
			 * (Ave Robustus II, phase P2, engine item jobs-owned-by-their-starter) — then this setter goes.
			 * @note Non-owning: the flag must outlive decimate().
			 * @param flag A pointer to the flag, nullptr for none (the default).
			 */
			void
			setCancellationFlag (const std::atomic_bool * flag) noexcept
			{
				m_cancellationFlag = flag;
			}

			/**
			 * @brief Returns whether a stop was requested (stop token or transitional flag).
			 * @return bool
			 */
			[[nodiscard]]
			bool
			isCancelled () const noexcept
			{
				return m_stopToken.stop_requested() || (m_cancellationFlag != nullptr && m_cancellationFlag->load(std::memory_order_relaxed));
			}

			/**
			 * @brief Performs the decimation and returns a new reduced shape.
			 * @return Shape< vertex_data_t, index_data_t > The decimated shape, or an EMPTY shape when cancelled
			 * (setCancellationFlag()).
			 */
			[[nodiscard]]
			Shape< vertex_data_t, index_data_t >
			decimate () const noexcept
			{
				if ( m_source.empty() || m_ratio >= static_cast< vertex_data_t >(1) )
				{
					return m_source;
				}

				/* NOTE: An inconsistent source (a triangle past the vertex array) is refused: the work mesh indexes the
				 * vertices through the triangles. */
				if ( !m_source.indicesInRange() )
				{
					std::cerr << "ShapeDecimator::decimate(), a source triangle refers to a vertex that does not exist !" "\n";

					return {};
				}

				/* The QEM works on a POSITION-deduplicated connectivity (OBJ-style meshes with per-face vertices would have
				 * no shared edge). It used to be a full copy of the source, deduplicated in place: on a 2.24 M-triangle mesh
				 * the copy alone took 0.6 s and could not be interrupted (Ave Robustus II D7). The work mesh holds only
				 * what the QEM reads: a representative source vertex per position, and the triangles remapped. */
				WorkMesh work;

				if ( !this->buildWorkMesh(work) )
				{
					return {};
				}

				const auto srcTriCount = work.triangleCount();
				const auto targetTriCount = std::max(size_t{4}, static_cast< size_t >(std::round(static_cast< vertex_data_t >(srcTriCount) * m_ratio)));

				/* Initialize mutable working data.
				 * NOTE: The per-vertex adjacency lists live in ONE arena, declared before the vertices so it outlives them:
				 * their growth never frees, and their release is a few large blocks (they were std::unordered_set: ~18 M
				 * nodes on a 2.24 M-triangle mesh, whose release held a cancelled decimation 0.2 to 1.35 s). */
				std::pmr::monotonic_buffer_resource adjacencyArena;
				std::vector< VertexData > vertices;
				std::vector< TriangleData > triangles;

				/* NOTE: Every stage below returns early on a stop request (Ave Robustus II: a bounded stop latency). */
				if ( !this->initializeData(vertices, triangles, work, adjacencyArena) )
				{
					return {};
				}

				/* Each corner's UV in its own chart (see CornerUVTable): what the output keeps, and what a collapse must
				 * not turn over. */
				const CornerUVTable cornerUVs{m_source, work, *this};

				if ( cornerUVs.cancelled() )
				{
					return {};
				}

				/* Compute initial quadrics. */
				if ( !this->computeInitialQuadrics(vertices, triangles) )
				{
					return {};
				}

				/* Detect and penalize boundaries and UV seams. */
				if ( !this->applyBoundaryAndSeamPenalties(vertices, triangles) )
				{
					return {};
				}

				/* Build initial collapse candidates. */
				CollapseQueue queue;

				if ( !this->buildCollapseQueue(vertices, triangles, queue) )
				{
					return {};
				}

				/* Iterative edge collapse. */
				size_t liveTriCount = triangles.size();
				size_t iteration = 0;

				while ( liveTriCount > targetTriCount && !queue.empty() )
				{
					if ( ++iteration % CancellationCheckInterval == 0 && this->isCancelled() )
					{
						return {};
					}

					auto candidate = queue.top();
					queue.pop();

					/* Lazy validation: skip stale entries. */
					if ( vertices[candidate.v0].removed || vertices[candidate.v1].removed )
					{
						continue;
					}

					if ( candidate.genV0 != vertices[candidate.v0].generation || candidate.genV1 != vertices[candidate.v1].generation )
					{
						continue;
					}

					/* Topology check: link condition. */
					if ( !checkLinkCondition(candidate.v0, candidate.v1, vertices, triangles) )
					{
						continue;
					}

					/* Normal flip check. */
					if ( checkNormalFlip(candidate.v0, candidate.v1, candidate.optimalPos, vertices, triangles) )
					{
						continue;
					}

					/* Sliver triangle check. */
					if ( checkSliverCreation(candidate.v0, candidate.v1, candidate.optimalPos, vertices, triangles) )
					{
						continue;
					}

					/* UV fold-over check: a collapse must not reverse a triangle's UV winding (its texture mirrored, its
					 * tangent frame turned around) — the 3D flip check above cannot see it. */
					if ( checkUVFoldOver(candidate.v0, candidate.v1, vertices, triangles, cornerUVs) )
					{
						continue;
					}

					/* Perform the collapse: v0 survives, v1 is removed. */
					const auto v0 = candidate.v0;
					const auto v1 = candidate.v1;

					vertices[v0].position = candidate.optimalPos;
					vertices[v0].quadric += vertices[v1].quadric;

					/* Update triangles: replace v1 with v0. */
					for ( const auto triIdx : vertices[v1].adjacentTris )
					{
						auto & tri = triangles[triIdx];

						if ( tri.removed )
						{
							continue;
						}

						/* Replace v1 with v0. */
						for ( int i = 0; i < 3; ++i )
						{
							if ( tri.v[i] == v1 )
							{
								tri.v[i] = v0;
							}
						}

						/* Check for degenerate triangle (two identical indices). */
						if ( tri.v[0] == tri.v[1] || tri.v[1] == tri.v[2] || tri.v[0] == tri.v[2] )
						{
							tri.removed = true;
							--liveTriCount;

							continue;
						}

						/* Add this triangle to v0's adjacency if not already there. */
						ShapeDecimator::insertUnique(vertices[v0].adjacentTris, triIdx);
					}

					/* Merge neighbor sets. */
					for ( const auto neighbor : vertices[v1].neighbors )
					{
						if ( neighbor != v0 && !vertices[neighbor].removed )
						{
							ShapeDecimator::insertUnique(vertices[v0].neighbors, neighbor);
							ShapeDecimator::eraseValue(vertices[neighbor].neighbors, v1);
							ShapeDecimator::insertUnique(vertices[neighbor].neighbors, v0);
						}
					}

					/* Remove v0 from its own neighbor list and remove v1. */
					ShapeDecimator::eraseValue(vertices[v0].neighbors, v1);

					/* Clean up v0's adjacentTris: remove dead triangles (in place, the order kept). */
					std::erase_if(vertices[v0].adjacentTris, [&triangles] (size_t triIdx) {
						return triangles[triIdx].removed;
					});

					vertices[v1].removed = true;
					++vertices[v0].generation;

					/* Re-queue edges from v0 to its neighbors. */
					for ( const auto neighbor : vertices[v0].neighbors )
					{
						if ( vertices[neighbor].removed )
						{
							continue;
						}

						const auto combined = vertices[v0].quadric + vertices[neighbor].quadric;
						const auto optPos = computeOptimalPosition(combined, vertices[v0].position, vertices[neighbor].position);
						const auto cost = evaluateQuadric(combined, optPos);

						queue.push({cost, v0, neighbor, optPos, vertices[v0].generation, vertices[neighbor].generation});
					}
				}

				if ( this->isCancelled() )
				{
					return {};
				}

				auto output = buildOutputShape(vertices, triangles, cornerUVs);

				if ( this->isCancelled() )
				{
					return {};
				}

				/* Normal map baking: generate lightmap UVs, then bake high-poly normals. */
				if ( m_normalMapResolution > 0 && !this->isCancelled() )
				{
					ShapeProcessor< vertex_data_t, index_data_t > processor{output};
					processor.generateLightmapUV();

					m_normalMap = bakeNormalMap(output, m_normalMapResolution);
				}

				/* NOTE: A stop during the bake leaves no normal map: the whole decimation reports the cancellation. */
				if ( this->isCancelled() )
				{
					return {};
				}

				return output;
			}

			/**
			 * @brief Returns the baked normal map (valid only after decimate() with normalMapResolution > 0).
			 * @return const PixelFactory::Pixmap< uint8_t > &
			 */
			[[nodiscard]]
			const PixelFactory::Pixmap< uint8_t > &
			normalMap () const noexcept
			{
				return m_normalMap;
			}

		private:

			/** @brief How many iterations of any stage loop between two reads of the stop request (cheap, but not per
			 * iteration). */
			static constexpr size_t CancellationCheckInterval{4096};

			/**
			 * @brief Counts one iteration of a stage loop and, every CancellationCheckInterval of them, reads the stop
			 * request.
			 * @param counter The stage's own iteration counter.
			 * @return bool True when the stage must stop now.
			 */
			[[nodiscard]]
			bool
			checkpoint (size_t & counter) const noexcept
			{
				return (++counter % CancellationCheckInterval) == 0 && this->isCancelled();
			}

			/** @brief Two UVs of one vertex closer than this (squared) are the same UV (a seam is far wider). */
			static constexpr vertex_data_t UVMatchToleranceSquared{static_cast< vertex_data_t >(1e-10)};

			/* ---- Internal types ---- */

			/**
			 * @brief Symmetric 4x4 quadric error matrix stored as 10 floats.
			 */
			struct Quadric
			{
				std::array< vertex_data_t, 10 > q{};

				Quadric & operator+= (const Quadric & other) noexcept
				{
					for ( size_t i = 0; i < 10; ++i )
					{
						q[i] += other.q[i];
					}

					return *this;
				}

				[[nodiscard]]
				Quadric operator+ (const Quadric & other) const noexcept
				{
					Quadric result = *this;
					result += other;

					return result;
				}
			};

			/**
			 * @brief A QEM vertex. Its adjacency lists are kept in INSERTION order, without duplicates, in the decimation's
			 * arena: the collapse order follows them, so the decimation is the same on every platform (a
			 * std::unordered_set's order differs per standard library: the three OS gave three different decimations
			 * until 2026-10-09).
			 */
			struct VertexData
			{
				/**
				 * @brief Constructs a vertex whose adjacency lists allocate from an arena.
				 * @param arena The arena (outlives the vertex).
				 */
				explicit
				VertexData (std::pmr::memory_resource * arena) noexcept
					: adjacentTris{arena},
					neighbors{arena}
				{

				}

				Quadric quadric;
				Math::Vector< 3, vertex_data_t > position;
				std::pmr::vector< size_t > adjacentTris;
				std::pmr::vector< index_data_t > neighbors;
				index_data_t srcIndex{0};
				uint32_t generation{0};
				bool removed{false};
			};

			/**
			 * @brief Appends a value to an adjacency list unless it is already there (a set in insertion order).
			 * @param list The list.
			 * @param value The value.
			 */
			template< typename value_t >
			static
			void
			insertUnique (std::pmr::vector< value_t > & list, value_t value) noexcept
			{
				if ( std::ranges::find(list, value) == list.end() )
				{
					list.push_back(value);
				}
			}

			/**
			 * @brief Removes a value from an adjacency list, keeping the order of the others.
			 * @param list The list.
			 * @param value The value.
			 */
			template< typename value_t >
			static
			void
			eraseValue (std::pmr::vector< value_t > & list, value_t value) noexcept
			{
				if ( const auto found = std::ranges::find(list, value); found != list.end() )
				{
					list.erase(found);
				}
			}

			struct TriangleData
			{
				index_data_t v[3]{0, 0, 0};
				size_t srcTriIndex{0};
				uint32_t groupIndex{0};
				bool removed{false};
			};

			/**
			 * @brief The position-deduplicated connectivity the QEM works on, built from the source WITHOUT copying it:
			 * the vertices sharing a quantized position become one work vertex, represented by the first of them (the
			 * same result as ShapeProcessor::deduplicateVertices(false, false) on a copy: same order, same
			 * representatives, so the same decimation).
			 */
			struct WorkMesh final
			{
				/** @brief For each work vertex, the source vertex standing for it (the first one at its position). */
				std::vector< index_data_t > representatives;
				/** @brief Three per source triangle (same order): the work vertex at each corner. */
				std::vector< index_data_t > corners;

				/**
				 * @brief Returns the number of triangles.
				 * @return size_t
				 */
				[[nodiscard]]
				size_t
				triangleCount () const noexcept
				{
					return corners.size() / 3;
				}

				/**
				 * @brief Returns the work vertex at a triangle corner.
				 * @pre triangle < triangleCount(), slot < 3.
				 * @param triangle The triangle index.
				 * @param slot The corner slot, 0 to 2.
				 * @return index_data_t
				 */
				[[nodiscard]]
				index_data_t
				corner (size_t triangle, size_t slot) const noexcept
				{
					return corners[(triangle * 3) + slot];
				}
			};

			/**
			 * @brief The UV of every triangle corner in its own chart. The work shape is deduplicated by POSITION, so a
			 * vertex of a UV seam carries several UVs: the source triangle's corner tells which one (the work triangles
			 * keep the source order and corner slots), and a corner whose vertex was collapsed into another takes, among
			 * that vertex's UVs, the one nearest the corner's own.
			 */
			class CornerUVTable final
			{
				public:

					/**
					 * @brief Builds the table.
					 * @param source The source shape (one vertex per (position, attributes)).
					 * @param work Its position-deduplicated connectivity (same triangles, same order).
					 * @param owner The decimator, whose stop request interrupts the build (cancelled() then reports it).
					 */
					CornerUVTable (const Shape< vertex_data_t, index_data_t > & source, const WorkMesh & work, const ShapeDecimator & owner) noexcept
						: m_source{&source},
						m_work{&work},
						m_valid{source.triangles().size() == work.triangleCount()}
					{
						if ( !m_valid )
						{
							return;
						}

						m_vertexUVs.resize(work.representatives.size());

						const auto workTriangleCount = work.triangleCount();
						size_t iteration = 0;

						for ( size_t t = 0; t < workTriangleCount; ++t )
						{
							if ( owner.checkpoint(iteration) )
							{
								m_valid = false;
								m_cancelled = true;

								return;
							}

							for ( index_data_t corner = 0; corner < 3; ++corner )
							{
								const auto & uv = this->sourceCornerUV(t, corner);
								auto & known = m_vertexUVs[work.corner(t, corner)];

								if ( std::ranges::none_of(known, [&uv] (const auto & other) { return (other - uv).lengthSquared() <= UVMatchToleranceSquared; }) )
								{
									known.push_back(uv);
								}
							}
						}
					}

					/**
					 * @brief Returns the UV of a triangle corner held by a given work vertex, and its index among that
					 * vertex's UVs.
					 * @param triangleIndex The source (and work) triangle index.
					 * @param corner The corner slot, 0 to 2.
					 * @param vertex The work vertex now at that corner.
					 * @return std::pair< size_t, Math::Vector< 3, vertex_data_t > >
					 */
					[[nodiscard]]
					std::pair< size_t, Math::Vector< 3, vertex_data_t > >
					resolve (size_t triangleIndex, index_data_t corner, index_data_t vertex) const noexcept
					{
						if ( !m_valid || vertex >= m_vertexUVs.size() || m_vertexUVs[vertex].empty() )
						{
							return {0, m_source->vertices()[m_work->representatives[vertex]].textureCoordinates()};
						}

						const auto & wanted = this->sourceCornerUV(triangleIndex, corner);
						const auto & candidates = m_vertexUVs[vertex];
						size_t best = 0;
						auto bestDistance = (candidates[0] - wanted).lengthSquared();

						for ( size_t index = 1; index < candidates.size(); ++index )
						{
							const auto distance = (candidates[index] - wanted).lengthSquared();

							if ( distance < bestDistance )
							{
								best = index;
								bestDistance = distance;
							}
						}

						return {best, candidates[best]};
					}

					/**
					 * @brief Returns whether the build was interrupted by a stop request.
					 * @return bool
					 */
					[[nodiscard]]
					bool
					cancelled () const noexcept
					{
						return m_cancelled;
					}

				private:

					[[nodiscard]]
					const Math::Vector< 3, vertex_data_t > &
					sourceCornerUV (size_t triangleIndex, index_data_t corner) const noexcept
					{
						return m_source->vertices()[m_source->triangles()[triangleIndex].vertexIndex(corner)].textureCoordinates();
					}

					/* Non-owning: the table lives inside decimate(), with both shapes. */
					const Shape< vertex_data_t, index_data_t > * m_source{nullptr};
					const WorkMesh * m_work{nullptr};
					std::vector< std::vector< Math::Vector< 3, vertex_data_t > > > m_vertexUVs;
					bool m_valid{false};
					bool m_cancelled{false};
			};

			struct CollapseCandidate
			{
				vertex_data_t cost{0};
				index_data_t v0{0};
				index_data_t v1{0};
				Math::Vector< 3, vertex_data_t > optimalPos;
				uint32_t genV0{0};
				uint32_t genV1{0};

				bool operator> (const CollapseCandidate & other) const noexcept
				{
					return cost > other.cost;
				}
			};

			using CollapseQueue = std::priority_queue< CollapseCandidate, std::vector< CollapseCandidate >, std::greater< CollapseCandidate > >;

			/* ---- Work mesh ---- */

			/** @brief The position quantum of the work mesh: ShapeProcessor's default tolerance (same quantization). */
			static constexpr vertex_data_t WorkPositionTolerance{static_cast< vertex_data_t >(1e-4)};

			/**
			 * @brief Builds the position-deduplicated connectivity (see WorkMesh) with ONE flat hash table, interruptible.
			 * @note Same quantization, same first-occurrence order and same representatives as
			 * ShapeProcessor::deduplicateVertices(false, false) on a copy of the source, which it replaces: the decimation
			 * is the same, bit for bit. The table keys are source vertex INDICES, hashed and compared through their quantized
			 * positions, so a slot is 12 bytes (a 1.1 M-vertex mesh: one 50 MB allocation, one release).
			 * @param work The work mesh to fill.
			 * @return bool False when a stop was requested (work is then partial and must not be used).
			 */
			[[nodiscard]]
			bool
			buildWorkMesh (WorkMesh & work) const noexcept
			{
				const auto & srcVerts = m_source.vertices();
				const auto & srcTris = m_source.triangles();
				const auto scale = static_cast< vertex_data_t >(1) / WorkPositionTolerance;

				/* The quantized position of each source vertex, computed once (the table hashes and compares them). */
				std::vector< std::array< int64_t, 3 > > quantized(srcVerts.size());
				size_t iteration = 0;

				for ( size_t index = 0; index < srcVerts.size(); ++index )
				{
					if ( this->checkpoint(iteration) )
					{
						return false;
					}

					const auto & position = srcVerts[index].position();

					quantized[index] = {
						static_cast< int64_t >(std::round(position[Math::X] * scale)),
						static_cast< int64_t >(std::round(position[Math::Y] * scale)),
						static_cast< int64_t >(std::round(position[Math::Z] * scale))
					};
				}

				const auto hashVertex = [&quantized] (index_data_t vertex) noexcept -> uint64_t {
					const auto & key = quantized[vertex];

					return mixHash(static_cast< uint64_t >(key[0]) ^ mixHash(static_cast< uint64_t >(key[1]) ^ mixHash(static_cast< uint64_t >(key[2]))));
				};
				const auto samePosition = [&quantized] (index_data_t first, index_data_t second) noexcept {
					return quantized[first] == quantized[second];
				};

				FlatHashMap< index_data_t, index_data_t, decltype(hashVertex), decltype(samePosition) > firstAtPosition{srcVerts.size(), hashVertex, samePosition};
				std::vector< index_data_t > workIndexOf(srcVerts.size());

				work.representatives.clear();
				work.representatives.reserve(srcVerts.size());

				for ( size_t index = 0; index < srcVerts.size(); ++index )
				{
					if ( this->checkpoint(iteration) )
					{
						return false;
					}

					const auto vertex = static_cast< index_data_t >(index);
					const auto [workIndex, inserted] = firstAtPosition.tryEmplace(vertex, static_cast< index_data_t >(work.representatives.size()));

					if ( inserted )
					{
						work.representatives.push_back(vertex);
					}

					workIndexOf[index] = *workIndex;
				}

				work.corners.resize(srcTris.size() * 3);

				for ( size_t triangle = 0; triangle < srcTris.size(); ++triangle )
				{
					if ( this->checkpoint(iteration) )
					{
						return false;
					}

					for ( index_data_t slot = 0; slot < 3; ++slot )
					{
						work.corners[(triangle * 3) + slot] = workIndexOf[srcTris[triangle].vertexIndex(slot)];
					}
				}

				return true;
			}

			/* ---- Initialization ---- */

			[[nodiscard]]
			bool
			initializeData (std::vector< VertexData > & vertices, std::vector< TriangleData > & triangles, const WorkMesh & work, std::pmr::memory_resource & adjacencyArena) const noexcept
			{
				size_t iteration = 0;

				const auto & srcVerts = m_source.vertices();
				const auto & srcGroups = m_source.groups();
				const auto triangleCount = work.triangleCount();

				/* One QEM vertex per work vertex (position-deduplicated: see buildWorkMesh()). srcIndex is the SOURCE vertex
				 * standing for it, whose attributes the output keeps. */
				vertices.clear();
				vertices.reserve(work.representatives.size());

				for ( size_t i = 0; i < work.representatives.size(); ++i )
				{
					if ( this->checkpoint(iteration) )
					{
						return false;
					}

					auto & vertex = vertices.emplace_back(&adjacencyArena);

					vertex.position = srcVerts[work.representatives[i]].position();
					vertex.srcIndex = work.representatives[i];
				}

				triangles.resize(triangleCount);

				/* Build a lookup: triangle index → group index.
				 * Groups store (offset, length) ranges over the triangle array. */
				std::vector< uint32_t > triGroupMap(triangleCount, 0);

				for ( uint32_t g = 0; g < static_cast< uint32_t >(srcGroups.size()); ++g )
				{
					const auto groupOffset = srcGroups[g].first;
					const auto groupLength = srcGroups[g].second;

					for ( index_data_t i = 0; i < groupLength; ++i )
					{
						if ( this->checkpoint(iteration) )
						{
							return false;
						}

						const auto triIdx = groupOffset + i;

						if ( triIdx < triangleCount )
						{
							triGroupMap[triIdx] = g;
						}
					}
				}

				for ( size_t t = 0; t < triangleCount; ++t )
				{
					if ( this->checkpoint(iteration) )
					{
						return false;
					}

					triangles[t].v[0] = work.corner(t, 0);
					triangles[t].v[1] = work.corner(t, 1);
					triangles[t].v[2] = work.corner(t, 2);
					triangles[t].srcTriIndex = t;
					triangles[t].groupIndex = triGroupMap[t];

					/* Skip degenerate triangles. */
					if ( triangles[t].v[0] == triangles[t].v[1] || triangles[t].v[1] == triangles[t].v[2] || triangles[t].v[0] == triangles[t].v[2] )
					{
						triangles[t].removed = true;

						continue;
					}

					/* NOTE: The corners are distinct (a degenerate triangle was skipped above): "another corner" is "a
					 * different vertex", in corner order. */
					const std::array< index_data_t, 3 > corners{triangles[t].v[0], triangles[t].v[1], triangles[t].v[2]};

					for ( const auto vIdx : corners )
					{
						ShapeDecimator::insertUnique(vertices[vIdx].adjacentTris, t);

						for ( const auto other : corners )
						{
							if ( other != vIdx )
							{
								ShapeDecimator::insertUnique(vertices[vIdx].neighbors, other);
							}
						}
					}
				}

				return true;
			}

			/* ---- Quadric computation ---- */

			[[nodiscard]]
			static
			Quadric
			computePlaneQuadric (const Math::Vector< 3, vertex_data_t > & normal, vertex_data_t d) noexcept
			{
				const auto a = normal[Math::X];
				const auto b = normal[Math::Y];
				const auto c = normal[Math::Z];

				return Quadric{{a * a, a * b, a * c, a * d, b * b, b * c, b * d, c * c, c * d, d * d}};
			}

			[[nodiscard]]
			bool
			computeInitialQuadrics (std::vector< VertexData > & vertices, const std::vector< TriangleData > & triangles) const noexcept
			{
				size_t iteration = 0;

				for ( size_t t = 0; t < triangles.size(); ++t )
				{
					if ( this->checkpoint(iteration) )
					{
						return false;
					}

					const auto & tri = triangles[t];
					const auto & p0 = vertices[tri.v[0]].position;
					const auto & p1 = vertices[tri.v[1]].position;
					const auto & p2 = vertices[tri.v[2]].position;

					const auto edge1 = p1 - p0;
					const auto edge2 = p2 - p0;
					const auto cross = Math::Vector< 3, vertex_data_t >::crossProduct(edge1, edge2);
					const auto area = cross.length() * static_cast< vertex_data_t >(0.5);

					if ( area < static_cast< vertex_data_t >(1e-10) )
					{
						continue;
					}

					const auto normal = cross.normalized();
					const auto d = -Math::Vector< 3, vertex_data_t >::dotProduct(normal, p0);

					auto Q = computePlaneQuadric(normal, d);

					/* Weight by triangle area. */
					for ( auto & val : Q.q )
					{
						val *= area;
					}

					vertices[tri.v[0]].quadric += Q;
					vertices[tri.v[1]].quadric += Q;
					vertices[tri.v[2]].quadric += Q;
				}

				return true;
			}

			/* ---- Boundary and UV seam detection ---- */

			[[nodiscard]]
			static
			uint64_t
			packEdgeKey (index_data_t a, index_data_t b) noexcept
			{
				const auto lo = std::min(a, b);
				const auto hi = std::max(a, b);

				return (static_cast< uint64_t >(lo) << 32) | static_cast< uint64_t >(hi);
			}

			[[nodiscard]]
			bool
			applyBoundaryAndSeamPenalties (std::vector< VertexData > & vertices, const std::vector< TriangleData > & triangles) const noexcept
			{
				size_t iteration = 0;

				/* Count edge occurrences to find boundaries. */
				std::unordered_map< uint64_t, size_t > edgeCounts;
				std::unordered_map< uint64_t, std::array< index_data_t, 2 > > edgeVerts;

				for ( const auto & tri : triangles )
				{
					if ( this->checkpoint(iteration) )
					{
						return false;
					}

					for ( int i = 0; i < 3; ++i )
					{
						const auto a = tri.v[i];
						const auto b = tri.v[(i + 1) % 3];
						const auto key = packEdgeKey(a, b);

						++edgeCounts[key];
						edgeVerts[key] = {a, b};
					}
				}

				/* Apply penalty to boundary edges. */
				for ( const auto & [key, count] : edgeCounts )
				{
					if ( this->checkpoint(iteration) )
					{
						return false;
					}

					if ( count != 1 )
					{
						continue;
					}

					const auto & [a, b] = edgeVerts[key];
					const auto & posA = vertices[a].position;
					const auto & posB = vertices[b].position;

					const auto edgeDir = (posB - posA).normalized();

					/* Find the adjacent triangle to get its normal. */
					Math::Vector< 3, vertex_data_t > triNormal{0, 1, 0};

					for ( const auto triIdx : vertices[a].adjacentTris )
					{
						const auto & tri = triangles[triIdx];
						bool hasA = false;
						bool hasB = false;

						for ( int i = 0; i < 3; ++i )
						{
							if ( tri.v[i] == a )
							{
								hasA = true;
							}

							if ( tri.v[i] == b )
							{
								hasB = true;
							}
						}

						if ( hasA && hasB )
						{
							const auto & p0 = vertices[tri.v[0]].position;
							const auto & p1 = vertices[tri.v[1]].position;
							const auto & p2 = vertices[tri.v[2]].position;

							triNormal = Math::Vector< 3, vertex_data_t >::crossProduct(p1 - p0, p2 - p0).normalized();

							break;
						}
					}

					/* Perpendicular plane to the boundary edge. */
					const auto perpNormal = Math::Vector< 3, vertex_data_t >::crossProduct(edgeDir, triNormal).normalized();
					const auto d = -Math::Vector< 3, vertex_data_t >::dotProduct(perpNormal, posA);

					auto penalty = computePlaneQuadric(perpNormal, d);

					for ( auto & val : penalty.q )
					{
						val *= m_boundaryPenaltyWeight;
					}

					vertices[a].quadric += penalty;
					vertices[b].quadric += penalty;
				}

				/* Detect UV seam vertices and apply penalties. */
				const auto & srcVerts = m_source.vertices();

				for ( size_t v = 0; v < vertices.size(); ++v )
				{
					if ( this->checkpoint(iteration) )
					{
						return false;
					}

					if ( vertices[v].adjacentTris.size() < 2 )
					{
						continue;
					}

					Math::Vector< 3, vertex_data_t > firstTC;
					bool isSeam = false;
					bool hasFirst = false;

					for ( const auto triIdx : vertices[v].adjacentTris )
					{
						const auto & srcTri = m_source.triangles()[triIdx];

						for ( index_data_t i = 0; i < 3; ++i )
						{
							if ( triangles[triIdx].v[i] == static_cast< index_data_t >(v) )
							{
								const auto & tc = srcVerts[srcTri.vertexIndex(i)].textureCoordinates();

								if ( !hasFirst )
								{
									firstTC = tc;
									hasFirst = true;
								}
								else if ( (tc - firstTC).lengthSquared() > static_cast< vertex_data_t >(1e-6) )
								{
									isSeam = true;
								}

								break;
							}
						}

						if ( isSeam )
						{
							break;
						}
					}

					if ( isSeam )
					{
						/* Add a large penalty to prevent collapsing UV seam vertices. */
						Quadric penalty{};

						for ( auto & val : penalty.q )
						{
							val = m_boundaryPenaltyWeight;
						}

						vertices[v].quadric += penalty;
					}
				}

				return true;
			}

			/* ---- Optimal position and quadric evaluation ---- */

			[[nodiscard]]
			static
			vertex_data_t
			evaluateQuadric (const Quadric & Q, const Math::Vector< 3, vertex_data_t > & v) noexcept
			{
				/* Q = [a11 a12 a13 a14]   indices: [0 1 2 3]
				 *	 [a12 a22 a23 a24]			[1 4 5 6]
				 *	 [a13 a23 a33 a34]			[2 5 7 8]
				 *	 [a14 a24 a34 a44]			[3 6 8 9]
				 *
				 * v^T Q v = (extended with w=1) */
				const auto x = v[Math::X];
				const auto y = v[Math::Y];
				const auto z = v[Math::Z];

				return (Q.q[0] * x * x) + (static_cast< vertex_data_t >(2) * Q.q[1] * x * y) + (static_cast< vertex_data_t >(2) * Q.q[2] * x * z) + (static_cast< vertex_data_t >(2) * Q.q[3] * x)
					 + (Q.q[4] * y * y) + (static_cast< vertex_data_t >(2) * Q.q[5] * y * z) + (static_cast< vertex_data_t >(2) * Q.q[6] * y)
					 + (Q.q[7] * z * z) + (static_cast< vertex_data_t >(2) * Q.q[8] * z)
					 + Q.q[9];
			}

			[[nodiscard]]
			static
			Math::Vector< 3, vertex_data_t >
			computeOptimalPosition (const Quadric & Q, const Math::Vector< 3, vertex_data_t > & vA, const Math::Vector< 3, vertex_data_t > & vB) noexcept
			{
				const auto mid = (vA + vB) * static_cast< vertex_data_t >(0.5);
				const auto edgeLengthSq = (vB - vA).lengthSquared();

				/* Try to solve the 3x3 system from the upper-left of Q.
				 * | a11 a12 a13 | | x |   | -a14 |
				 * | a12 a22 a23 | | y | = | -a24 |
				 * | a13 a23 a33 | | z |   | -a34 | */
				const auto a = Q.q[0], b = Q.q[1], c = Q.q[2], d = Q.q[3];
				const auto e = Q.q[4], f = Q.q[5], g = Q.q[6];
				const auto h = Q.q[7], i = Q.q[8];

				const auto det = (a * (e * h - f * f)) - (b * (b * h - f * c)) + (c * (b * f - e * c));

				if ( std::abs(det) > static_cast< vertex_data_t >(1e-6) )
				{
					const auto invDet = static_cast< vertex_data_t >(1) / det;

					const auto rx = -d, ry = -g, rz = -i;

					const auto x = invDet * (rx * (e * h - f * f) + ry * (c * f - b * h) + rz * (b * f - c * e));
					const auto y = invDet * (rx * (c * f - b * h) + ry * (a * h - c * c) + rz * (b * c - a * f));
					const auto z = invDet * (rx * (b * f - c * e) + ry * (b * c - a * f) + rz * (a * e - b * b));

					const Math::Vector< 3, vertex_data_t > solved{x, y, z};

					/* Reject the solved position if it lies too far from the edge midpoint.
					 * A factor of 2× the edge length keeps the result in a reasonable neighborhood. */
					constexpr auto MaxDistanceFactor = static_cast< vertex_data_t >(4);
					const auto distSqFromMid = (solved - mid).lengthSquared();

					if ( distSqFromMid <= MaxDistanceFactor * edgeLengthSq )
					{
						return solved;
					}
				}

				/* Fallback: pick the best among midpoint, vA, vB. */
				const auto costA = evaluateQuadric(Q, vA);
				const auto costB = evaluateQuadric(Q, vB);
				const auto costMid = evaluateQuadric(Q, mid);

				if ( costMid <= costA && costMid <= costB )
				{
					return mid;
				}

				return (costA <= costB) ? vA : vB;
			}

			/* ---- Topology checks ---- */

			[[nodiscard]]
			static
			bool
			checkLinkCondition (index_data_t v0, index_data_t v1, const std::vector< VertexData > & vertices, [[maybe_unused]] const std::vector< TriangleData > & triangles) noexcept
			{
				/* Count shared neighbors (the "link" of the edge). */
				size_t sharedCount = 0;

				for ( const auto neighbor : vertices[v0].neighbors )
				{
					if ( neighbor != v1 && std::ranges::find(vertices[v1].neighbors, neighbor) != vertices[v1].neighbors.end() )
					{
						++sharedCount;
					}
				}

				/* For a manifold interior edge: exactly 2 shared neighbors.
				 * For a boundary edge: exactly 1 shared neighbor.
				 * Allow both cases. */
				return sharedCount <= 2;
			}

			/**
			 * @brief Checks if a collapse would produce sliver triangles (near-degenerate).
			 * Uses the compactness ratio: 4√3 × area / perimeter². A perfect equilateral = 1.0.
			 * @return true if a sliver would be created (collapse should be rejected).
			 */
			[[nodiscard]]
			static
			bool
			checkSliverCreation (index_data_t v0, index_data_t v1, const Math::Vector< 3, vertex_data_t > & newPos, const std::vector< VertexData > & vertices, const std::vector< TriangleData > & triangles) noexcept
			{
				constexpr auto MinCompactness = static_cast< vertex_data_t >(0.02);
				constexpr auto FourSqrt3 = static_cast< vertex_data_t >(6.928203230275509);

				auto checkTriangles = [&] (index_data_t vKeep, index_data_t vRemove) -> bool
				{
					for ( const auto triIdx : vertices[vKeep].adjacentTris )
					{
						const auto & tri = triangles[triIdx];

						if ( tri.removed )
						{
							continue;
						}

						/* Skip triangles shared by both vertices (they will be removed). */
						bool hasRemove = false;

						for ( int i = 0; i < 3; ++i )
						{
							if ( tri.v[i] == vRemove )
							{
								hasRemove = true;

								break;
							}
						}

						if ( hasRemove )
						{
							continue;
						}

						/* Compute the triangle with vKeep moved to newPos. */
						Math::Vector< 3, vertex_data_t > positions[3];

						for ( int i = 0; i < 3; ++i )
						{
							positions[i] = (tri.v[i] == vKeep) ? newPos : vertices[tri.v[i]].position;
						}

						const auto e0 = positions[1] - positions[0];
						const auto e1 = positions[2] - positions[1];
						const auto e2 = positions[0] - positions[2];

						const auto area = Math::Vector< 3, vertex_data_t >::crossProduct(e0, positions[2] - positions[0]).length() * static_cast< vertex_data_t >(0.5);
						const auto perimeter = e0.length() + e1.length() + e2.length();

						if ( perimeter < static_cast< vertex_data_t >(1e-10) )
						{
							return true;
						}

						const auto compactness = FourSqrt3 * area / (perimeter * perimeter);

						if ( compactness < MinCompactness )
						{
							return true;
						}
					}

					return false;
				};

				return checkTriangles(v0, v1) || checkTriangles(v1, v0);
			}

			[[nodiscard]]
			static
			bool
			checkNormalFlip (index_data_t v0, index_data_t v1, const Math::Vector< 3, vertex_data_t > & newPos, const std::vector< VertexData > & vertices, const std::vector< TriangleData > & triangles) noexcept
			{
				/* Check all triangles that will survive the collapse (adjacent to v0 or v1,
				 * not shared between them). Verify that no triangle normal flips. */
				auto checkTriangles = [&] (index_data_t vKeep, index_data_t vRemove) -> bool
				{
					for ( const auto triIdx : vertices[vKeep].adjacentTris )
					{
						const auto & tri = triangles[triIdx];

						if ( tri.removed )
						{
							continue;
						}

						/* Skip triangles shared by both v0 and v1 (they'll be removed). */
						bool hasRemove = false;

						for ( int i = 0; i < 3; ++i )
						{
							if ( tri.v[i] == vRemove )
							{
								hasRemove = true;

								break;
							}
						}

						if ( hasRemove )
						{
							continue;
						}

						/* Compute old and new normals. */
						Math::Vector< 3, vertex_data_t > oldPositions[3];
						Math::Vector< 3, vertex_data_t > newPositions[3];

						for ( int i = 0; i < 3; ++i )
						{
							oldPositions[i] = vertices[tri.v[i]].position;
							newPositions[i] = (tri.v[i] == vKeep) ? newPos : vertices[tri.v[i]].position;
						}

						const auto oldCross = Math::Vector< 3, vertex_data_t >::crossProduct(
							oldPositions[1] - oldPositions[0], oldPositions[2] - oldPositions[0]);

						const auto newCross = Math::Vector< 3, vertex_data_t >::crossProduct(
							newPositions[1] - newPositions[0], newPositions[2] - newPositions[0]);

						/* Skip degenerate triangles (near-zero area). */
						const auto oldLenSq = oldCross.lengthSquared();
						const auto newLenSq = newCross.lengthSquared();

						if ( oldLenSq < static_cast< vertex_data_t >(1e-20) || newLenSq < static_cast< vertex_data_t >(1e-20) )
						{
							return true;
						}

						/* Normalize before comparing: the threshold is a cosine angle, not a magnitude. */
						const auto dot = Math::Vector< 3, vertex_data_t >::dotProduct(oldCross, newCross);
						const auto normalizedDot = dot / std::sqrt(oldLenSq * newLenSq);

						if ( normalizedDot < static_cast< vertex_data_t >(0.2) )
						{
							return true; /* Flip or excessive deformation detected. */
						}
					}

					return false;
				};

				return checkTriangles(v0, v1) || checkTriangles(v1, v0);
			}

			/**
			 * @brief Returns whether collapsing v1 into v0 would reverse the UV winding of a triangle around v1 (v0 keeps
			 * its UV: only the triangles that held v1 and not v0 change their UVs).
			 * @param v0 The surviving vertex.
			 * @param v1 The removed vertex.
			 * @param vertices The working vertices.
			 * @param triangles The working triangles.
			 * @param cornerUVs The corner UVs.
			 * @return bool True when a UV fold-over would be created (the collapse is rejected).
			 */
			[[nodiscard]]
			static
			bool
			checkUVFoldOver (index_data_t v0, index_data_t v1, const std::vector< VertexData > & vertices, const std::vector< TriangleData > & triangles, const CornerUVTable & cornerUVs) noexcept
			{
				const auto determinant = [] (const std::array< Math::Vector< 3, vertex_data_t >, 3 > & uv) noexcept {
					return ((uv[1][Math::X] - uv[0][Math::X]) * (uv[2][Math::Y] - uv[0][Math::Y])) - ((uv[2][Math::X] - uv[0][Math::X]) * (uv[1][Math::Y] - uv[0][Math::Y]));
				};

				for ( const auto triIdx : vertices[v1].adjacentTris )
				{
					const auto & tri = triangles[triIdx];

					if ( tri.removed || tri.v[0] == v0 || tri.v[1] == v0 || tri.v[2] == v0 )
					{
						continue;
					}

					std::array< Math::Vector< 3, vertex_data_t >, 3 > before{};
					std::array< Math::Vector< 3, vertex_data_t >, 3 > after{};
					auto beforeIt = before.begin();
					auto afterIt = after.begin();
					index_data_t corner = 0;

					for ( const auto vertex : tri.v )
					{
						*beforeIt = cornerUVs.resolve(tri.srcTriIndex, corner, vertex).second;
						*afterIt = vertex == v1 ? cornerUVs.resolve(tri.srcTriIndex, corner, v0).second : *beforeIt;

						++beforeIt;
						++afterIt;
						++corner;
					}

					const auto areaBefore = determinant(before);
					const auto areaAfter = determinant(after);

					/* A triangle already degenerate in UV space has no winding to keep. */
					if ( areaBefore != 0 && (areaAfter == 0 || (areaBefore > 0) != (areaAfter > 0)) )
					{
						return true;
					}
				}

				return false;
			}

			/* ---- Priority queue ---- */

			[[nodiscard]]
			bool
			buildCollapseQueue (const std::vector< VertexData > & vertices, const std::vector< TriangleData > & triangles, CollapseQueue & queue) const noexcept
			{
				std::unordered_set< uint64_t > processedEdges;
				size_t iteration = 0;

				for ( size_t t = 0; t < triangles.size(); ++t )
				{
					if ( this->checkpoint(iteration) )
					{
						return false;
					}

					const auto & tri = triangles[t];

					for ( int i = 0; i < 3; ++i )
					{
						const auto a = tri.v[i];
						const auto b = tri.v[(i + 1) % 3];
						const auto key = packEdgeKey(a, b);

						if ( processedEdges.contains(key) )
						{
							continue;
						}

						processedEdges.insert(key);

						const auto combined = vertices[a].quadric + vertices[b].quadric;
						const auto optPos = computeOptimalPosition(combined, vertices[a].position, vertices[b].position);
						const auto cost = evaluateQuadric(combined, optPos);

						queue.push({cost, a, b, optPos, vertices[a].generation, vertices[b].generation});
					}
				}

				return true;
			}

			/* ---- Output shape construction ---- */

			[[nodiscard]]
			Shape< vertex_data_t, index_data_t >
			buildOutputShape (const std::vector< VertexData > & vertices, const std::vector< TriangleData > & triangles, const CornerUVTable & cornerUVs) const noexcept
			{
				Shape< vertex_data_t, index_data_t > output;

				/* ⚠️ UVs PER CORNER, from the triangle's own chart. The work shape is deduplicated by POSITION only, so a
				 * vertex of a UV seam (a sphere's u = 0 / u = 1 column) is ONE work vertex: giving it one UV made every
				 * triangle on the other side of the seam span the whole texture backwards — ~30 folded triangles on a
				 * 32 × 16 sphere at ANY ratio (base item decimator-uv-fold-overs, 2026-10-07). The work triangles keep the
				 * source order and corner slots, so the source triangle's corner gives the right UV; a corner whose vertex
				 * was collapsed into another takes, among that vertex's UVs, the one nearest the corner's own. One output
				 * vertex per (work vertex, UV): the seam is split again. */
				/* Output vertices keyed by (work vertex, UV index). */
				std::map< std::pair< index_data_t, size_t >, index_data_t > vertexMap;
				/* Three per triangle: the output vertex of each corner. */
				std::vector< index_data_t > outputCorners(triangles.size() * 3);
				size_t iteration = 0;

				for ( size_t t = 0; t < triangles.size(); ++t )
				{
					if ( this->checkpoint(iteration) )
					{
						return {};
					}

					if ( triangles[t].removed )
					{
						continue;
					}

					const auto & triangle = triangles[t];
					index_data_t corner = 0;

					for ( const auto srcIdx : triangle.v )
					{
						const auto slot = (t * 3) + corner;
						const auto [uvIndex, uv] = cornerUVs.resolve(triangle.srcTriIndex, corner, srcIdx);
						const auto key = std::make_pair(srcIdx, uvIndex);

						++corner;

						if ( const auto known = vertexMap.find(key); known != vertexMap.end() )
						{
							outputCorners[slot] = known->second;

							continue;
						}

						const auto & srcVertex = m_source.vertices()[vertices[srcIdx].srcIndex];
						const auto & newPos = vertices[srcIdx].position;

						const auto dstIdx = output.saveVertex(newPos, srcVertex.normal(), uv);
						output.vertices()[dstIdx].setTangent(srcVertex.tangent());
						output.vertices()[dstIdx].setTangentHandedness(srcVertex.tangentHandedness());

						vertexMap.emplace(key, dstIdx);
						outputCorners[slot] = dstIdx;
					}
				}

				/* Collect surviving triangle indices, sorted by group for multi-material preservation. */
				std::vector< size_t > survivingTriIndices;
				survivingTriIndices.reserve(triangles.size());

				for ( size_t t = 0; t < triangles.size(); ++t )
				{
					if ( !triangles[t].removed )
					{
						survivingTriIndices.emplace_back(t);
					}
				}

				std::ranges::stable_sort(survivingTriIndices, [&triangles] (size_t a, size_t b) {
					return triangles[a].groupIndex < triangles[b].groupIndex;
				});

				/* Emit surviving triangles in group order and rebuild group ranges. */
				auto & groups = output.groups();
				groups.clear();

				uint32_t currentGroup = std::numeric_limits< uint32_t >::max();
				index_data_t groupStart = 0;

				for ( const auto triIdx : survivingTriIndices )
				{
					if ( this->checkpoint(iteration) )
					{
						return {};
					}

					const auto & tri = triangles[triIdx];

					const auto dv0 = outputCorners[(triIdx * 3) + 0];
					const auto dv1 = outputCorners[(triIdx * 3) + 1];
					const auto dv2 = outputCorners[(triIdx * 3) + 2];

					const auto c0 = output.saveVertexColor({});
					const auto c1 = output.saveVertexColor({});
					const auto c2 = output.saveVertexColor({});

					ShapeTriangle< vertex_data_t, index_data_t > outTri(dv0, dv1, dv2);
					outTri.setVertexColorIndex(0, c0);
					outTri.setVertexColorIndex(1, c1);
					outTri.setVertexColorIndex(2, c2);

					output.triangles().emplace_back(outTri);

					/* Track group transitions. */
					if ( tri.groupIndex != currentGroup )
					{
						if ( currentGroup != std::numeric_limits< uint32_t >::max() )
						{
							const auto triCount = static_cast< index_data_t >(output.triangles().size()) - 1 - groupStart;
							groups.emplace_back(groupStart, triCount);
						}

						currentGroup = tri.groupIndex;
						groupStart = static_cast< index_data_t >(output.triangles().size()) - 1;
					}
				}

				/* Close the last group. */
				if ( currentGroup != std::numeric_limits< uint32_t >::max() )
				{
					const auto triCount = static_cast< index_data_t >(output.triangles().size()) - groupStart;
					groups.emplace_back(groupStart, triCount);
				}

				if ( !output.empty() )
				{
					output.computeTriangleNormal();
					output.computeTriangleTangent();
					output.computeVertexNormal();
					output.computeVertexTangent();

					/* Verify normal orientation against the source.
					 * If the majority of computed normals disagree with source normals
					 * (e.g. due to Y-flip changing winding), flip all normals. */
					size_t agree = 0;
					size_t disagree = 0;
					const auto sampleCount = std::min(size_t{100}, output.vertices().size());

					for ( size_t i = 0; i < sampleCount; ++i )
					{
						const auto sampleIdx = i * output.vertices().size() / sampleCount;
						const auto & computedNormal = output.vertices()[sampleIdx].normal();

						/* Find closest source vertex by position. */
						const auto & pos = output.vertices()[sampleIdx].position();
						vertex_data_t bestDist = std::numeric_limits< vertex_data_t >::max();
						Math::Vector< 3, vertex_data_t > srcNormal;

						for ( const auto & srcVert : m_source.vertices() )
						{
							if ( this->checkpoint(iteration) )
							{
								return {};
							}

							const auto dist = (srcVert.position() - pos).lengthSquared();

							if ( dist < bestDist )
							{
								bestDist = dist;
								srcNormal = srcVert.normal();
							}
						}

						if ( Math::Vector< 3, vertex_data_t >::dotProduct(computedNormal, srcNormal) > 0 )
						{
							++agree;
						}
						else
						{
							++disagree;
						}
					}

					if ( disagree > agree )
					{
						for ( auto & vert : output.vertices() )
						{
							vert.setNormal(vert.normal() * static_cast< vertex_data_t >(-1));
						}

						for ( auto & tri : output.triangles() )
						{
							tri.setSurfaceNormal(tri.surfaceNormal() * static_cast< vertex_data_t >(-1));
						}
					}

					output.updateProperties();
				}

				return output;
			}

			/* ---- Members ---- */

			/* ---- Normal map baking ---- */

			[[nodiscard]]
			PixelFactory::Pixmap< uint8_t >
			bakeNormalMap (const Shape< vertex_data_t, index_data_t > & lowPoly, uint32_t resolution) const noexcept
			{
				using namespace PixelFactory;

				Pixmap< uint8_t > normalMap(resolution, resolution, ChannelMode::RGBA, Color< float >{0.5F, 0.5F, 1.0F, 1.0F});

				const auto & lowTris = lowPoly.triangles();
				const auto & lowVerts = lowPoly.vertices();
				const auto & highTris = m_bakeSource.triangles();
				const auto & highVerts = m_bakeSource.vertices();

				/* Build a simple grid spatial hash over high-poly triangles for fast ray-triangle tests. */
				const auto & bbox = m_bakeSource.boundingBox();
				const auto bboxMin = bbox.minimum();
				const auto bboxMax = bbox.maximum();
				const auto bboxSize = bboxMax - bboxMin;

				constexpr size_t GridRes = 128;
				const auto cellSize = bboxSize * (static_cast< vertex_data_t >(1) / static_cast< vertex_data_t >(GridRes));

				std::vector< std::vector< size_t > > grid(GridRes * GridRes * GridRes);

				auto gridIndex = [&] (const Math::Vector< 3, vertex_data_t > & pos) -> size_t
				{
					auto cx = static_cast< size_t >(std::clamp((pos[Math::X] - bboxMin[Math::X]) / cellSize[Math::X], static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(GridRes - 1)));
					auto cy = static_cast< size_t >(std::clamp((pos[Math::Y] - bboxMin[Math::Y]) / cellSize[Math::Y], static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(GridRes - 1)));
					auto cz = static_cast< size_t >(std::clamp((pos[Math::Z] - bboxMin[Math::Z]) / cellSize[Math::Z], static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(GridRes - 1)));

					return cx + (cy * GridRes) + (cz * GridRes * GridRes);
				};

				size_t iteration = 0;

				for ( size_t t = 0; t < highTris.size(); ++t )
				{
					if ( this->checkpoint(iteration) )
					{
						return {};
					}

					const auto & tri = highTris[t];
					const auto & p0 = highVerts[tri.vertexIndex(0)].position();
					const auto & p1 = highVerts[tri.vertexIndex(1)].position();
					const auto & p2 = highVerts[tri.vertexIndex(2)].position();

					/* Insert into all grid cells that the triangle's AABB overlaps. */
					auto triMin = p0, triMax = p0;

					for ( size_t axis = 0; axis < 3; ++axis )
					{
						triMin[axis] = std::min({p0[axis], p1[axis], p2[axis]});
						triMax[axis] = std::max({p0[axis], p1[axis], p2[axis]});
					}

					auto minCell = gridIndex(triMin);
					auto maxCell = gridIndex(triMax);

					size_t minCx = minCell % GridRes, minCy = (minCell / GridRes) % GridRes, minCz = minCell / (GridRes * GridRes);
					size_t maxCx = maxCell % GridRes, maxCy = (maxCell / GridRes) % GridRes, maxCz = maxCell / (GridRes * GridRes);

					for ( size_t z = minCz; z <= maxCz; ++z )
					{
						for ( size_t y = minCy; y <= maxCy; ++y )
						{
							for ( size_t x = minCx; x <= maxCx; ++x )
							{
								grid[x + (y * GridRes) + (z * GridRes * GridRes)].push_back(t);
							}
						}
					}
				}

				/* Ray-triangle intersection (Möller–Trumbore). */
				auto rayTriangleIntersect = [&highVerts] (const Math::Vector< 3, vertex_data_t > & origin, const Math::Vector< 3, vertex_data_t > & dir, const ShapeTriangle< vertex_data_t, index_data_t > & tri, vertex_data_t & outT, vertex_data_t & outU, vertex_data_t & outV) -> bool
				{
					constexpr auto eps = static_cast< vertex_data_t >(1e-7);

					const auto & v0 = highVerts[tri.vertexIndex(0)].position();
					const auto & v1 = highVerts[tri.vertexIndex(1)].position();
					const auto & v2 = highVerts[tri.vertexIndex(2)].position();

					const auto e1 = v1 - v0;
					const auto e2 = v2 - v0;
					const auto h = Math::Vector< 3, vertex_data_t >::crossProduct(dir, e2);
					const auto a = Math::Vector< 3, vertex_data_t >::dotProduct(e1, h);

					if ( std::abs(a) < eps )
					{
						return false;
					}

					const auto f = static_cast< vertex_data_t >(1) / a;
					const auto s = origin - v0;
					outU = f * Math::Vector< 3, vertex_data_t >::dotProduct(s, h);

					if ( outU < 0 || outU > 1 )
					{
						return false;
					}

					const auto q = Math::Vector< 3, vertex_data_t >::crossProduct(s, e1);
					outV = f * Math::Vector< 3, vertex_data_t >::dotProduct(dir, q);

					if ( outV < 0 || outU + outV > 1 )
					{
						return false;
					}

					outT = f * Math::Vector< 3, vertex_data_t >::dotProduct(e2, q);

					return true;
				};

				/* Build a 2D grid over UV space for fast texel → triangle lookup. */
				constexpr size_t UVGridRes = 64;
				std::vector< std::vector< size_t > > uvGrid(UVGridRes * UVGridRes);

				for ( size_t t = 0; t < lowTris.size(); ++t )
				{
					if ( this->checkpoint(iteration) )
					{
						return {};
					}

					const auto & tri = lowTris[t];
					const auto & tc0 = lowVerts[tri.vertexIndex(0)].textureCoordinates();
					const auto & tc1 = lowVerts[tri.vertexIndex(1)].textureCoordinates();
					const auto & tc2 = lowVerts[tri.vertexIndex(2)].textureCoordinates();

					auto uvMinU = std::min({tc0[Math::X], tc1[Math::X], tc2[Math::X]});
					auto uvMaxU = std::max({tc0[Math::X], tc1[Math::X], tc2[Math::X]});
					auto uvMinV = std::min({tc0[Math::Y], tc1[Math::Y], tc2[Math::Y]});
					auto uvMaxV = std::max({tc0[Math::Y], tc1[Math::Y], tc2[Math::Y]});

					auto cellMinU = static_cast< size_t >(std::clamp(uvMinU * static_cast< vertex_data_t >(UVGridRes), static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(UVGridRes - 1)));
					auto cellMaxU = static_cast< size_t >(std::clamp(uvMaxU * static_cast< vertex_data_t >(UVGridRes), static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(UVGridRes - 1)));
					auto cellMinV = static_cast< size_t >(std::clamp(uvMinV * static_cast< vertex_data_t >(UVGridRes), static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(UVGridRes - 1)));
					auto cellMaxV = static_cast< size_t >(std::clamp(uvMaxV * static_cast< vertex_data_t >(UVGridRes), static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(UVGridRes - 1)));

					for ( size_t cv = cellMinV; cv <= cellMaxV; ++cv )
					{
						for ( size_t cu = cellMinU; cu <= cellMaxU; ++cu )
						{
							uvGrid[(cv * UVGridRes) + cu].push_back(t);
						}
					}
				}

				/* For each texel, find the corresponding low-poly triangle, ray-cast to high-poly.
				 * Parallelized by row — each row writes to independent pixels. */
				const auto invRes = static_cast< vertex_data_t >(1) / static_cast< vertex_data_t >(resolution);

				auto processRow = [&] (uint32_t py)
				{
					/* NOTE: One read per row (rows may run in parallel: the stop request is thread-safe). */
					if ( this->isCancelled() )
					{
						return;
					}

					for ( uint32_t px = 0; px < resolution; ++px )
					{
						const auto u = (static_cast< vertex_data_t >(px) + static_cast< vertex_data_t >(0.5)) * invRes;
						const auto v = (static_cast< vertex_data_t >(py) + static_cast< vertex_data_t >(0.5)) * invRes;

						/* Find which low-poly triangle contains this UV using the grid. */
						bool found = false;
						Math::Vector< 3, vertex_data_t > worldPos;
						Math::Vector< 3, vertex_data_t > lowNormal;
						Math::Vector< 3, vertex_data_t > lowTangent;
						Math::Vector< 3, vertex_data_t > lowBitangent;

						const auto cellU = static_cast< size_t >(std::clamp(u * static_cast< vertex_data_t >(UVGridRes), static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(UVGridRes - 1)));
						const auto cellV = static_cast< size_t >(std::clamp(v * static_cast< vertex_data_t >(UVGridRes), static_cast< vertex_data_t >(0), static_cast< vertex_data_t >(UVGridRes - 1)));

						for ( const auto t : uvGrid[(cellV * UVGridRes) + cellU] )
						{
							if ( found )
							{
								break;
							}
							const auto & tri = lowTris[t];
							const auto & tc0 = lowVerts[tri.vertexIndex(0)].textureCoordinates();
							const auto & tc1 = lowVerts[tri.vertexIndex(1)].textureCoordinates();
							const auto & tc2 = lowVerts[tri.vertexIndex(2)].textureCoordinates();

							/* Barycentric coordinates in UV space. */
							const auto d00 = ((tc1[Math::X] - tc0[Math::X]) * (tc1[Math::X] - tc0[Math::X])) + ((tc1[Math::Y] - tc0[Math::Y]) * (tc1[Math::Y] - tc0[Math::Y]));
							const auto d01 = ((tc1[Math::X] - tc0[Math::X]) * (tc2[Math::X] - tc0[Math::X])) + ((tc1[Math::Y] - tc0[Math::Y]) * (tc2[Math::Y] - tc0[Math::Y]));
							const auto d11 = ((tc2[Math::X] - tc0[Math::X]) * (tc2[Math::X] - tc0[Math::X])) + ((tc2[Math::Y] - tc0[Math::Y]) * (tc2[Math::Y] - tc0[Math::Y]));
							const auto d20 = ((u - tc0[Math::X]) * (tc1[Math::X] - tc0[Math::X])) + ((v - tc0[Math::Y]) * (tc1[Math::Y] - tc0[Math::Y]));
							const auto d21 = ((u - tc0[Math::X]) * (tc2[Math::X] - tc0[Math::X])) + ((v - tc0[Math::Y]) * (tc2[Math::Y] - tc0[Math::Y]));

							const auto denom = (d00 * d11) - (d01 * d01);

							if ( std::abs(denom) < static_cast< vertex_data_t >(1e-10) )
							{
								continue;
							}

							const auto baryV = (d11 * d20 - d01 * d21) / denom;
							const auto baryW = (d00 * d21 - d01 * d20) / denom;
							const auto baryU = static_cast< vertex_data_t >(1) - baryV - baryW;

							if ( baryU < 0 || baryV < 0 || baryW < 0 )
							{
								continue;
							}

							/* Interpolate 3D position and TBN from the low-poly. */
							const auto & p0 = lowVerts[tri.vertexIndex(0)].position();
							const auto & p1 = lowVerts[tri.vertexIndex(1)].position();
							const auto & p2 = lowVerts[tri.vertexIndex(2)].position();

							worldPos = p0 * baryU + p1 * baryV + p2 * baryW;

							const auto & n0 = lowVerts[tri.vertexIndex(0)].normal();
							const auto & n1 = lowVerts[tri.vertexIndex(1)].normal();
							const auto & n2 = lowVerts[tri.vertexIndex(2)].normal();

							lowNormal = ((n0 * baryU) + (n1 * baryV) + (n2 * baryW)).normalized();

							const auto & t0 = lowVerts[tri.vertexIndex(0)].tangent();
							const auto & t1 = lowVerts[tri.vertexIndex(1)].tangent();
							const auto & t2 = lowVerts[tri.vertexIndex(2)].tangent();

							lowTangent = ((t0 * baryU) + (t1 * baryV) + (t2 * baryW)).normalized();
							lowBitangent = Math::Vector< 3, vertex_data_t >::crossProduct(lowNormal, lowTangent).normalized();

							found = true;
						}

						if ( !found )
						{
							continue;
						}

						/* Ray-cast along the low-poly normal (both directions) to find the high-poly surface. */
						vertex_data_t bestDist = std::numeric_limits< vertex_data_t >::max();
						Math::Vector< 3, vertex_data_t > highNormal = lowNormal;
						bool hit = false;

						/* Check cells around the world position. */
						const auto cellIdx = gridIndex(worldPos);
						const size_t cx = cellIdx % GridRes;
						const size_t cy = (cellIdx / GridRes) % GridRes;
						const size_t cz = cellIdx / (GridRes * GridRes);

						const size_t searchRadius = 1;
						const size_t sMinX = (cx > searchRadius) ? cx - searchRadius : 0;
						const size_t sMinY = (cy > searchRadius) ? cy - searchRadius : 0;
						const size_t sMinZ = (cz > searchRadius) ? cz - searchRadius : 0;
						const size_t sMaxX = std::min(cx + searchRadius, GridRes - 1);
						const size_t sMaxY = std::min(cy + searchRadius, GridRes - 1);
						const size_t sMaxZ = std::min(cz + searchRadius, GridRes - 1);

						for ( size_t sz = sMinZ; sz <= sMaxZ; ++sz )
						{
							for ( size_t sy = sMinY; sy <= sMaxY; ++sy )
							{
								for ( size_t sx = sMinX; sx <= sMaxX; ++sx )
								{
									for ( const auto highTriIdx : grid[sx + (sy * GridRes) + (sz * GridRes * GridRes)] )
									{
										const auto & highTri = highTris[highTriIdx];
										vertex_data_t tHit, uHit, vHit;

										/* Try both directions along the normal. */
										if ( rayTriangleIntersect(worldPos, lowNormal, highTri, tHit, uHit, vHit) && std::abs(tHit) < bestDist )
										{
											bestDist = std::abs(tHit);

											const auto & hn0 = highVerts[highTri.vertexIndex(0)].normal();
											const auto & hn1 = highVerts[highTri.vertexIndex(1)].normal();
											const auto & hn2 = highVerts[highTri.vertexIndex(2)].normal();

											highNormal = ((hn0 * (static_cast< vertex_data_t >(1) - uHit - vHit)) + (hn1 * uHit) + (hn2 * vHit)).normalized();
											hit = true;
										}

										const auto negDir = lowNormal * static_cast< vertex_data_t >(-1);

										if ( rayTriangleIntersect(worldPos, negDir, highTri, tHit, uHit, vHit) && std::abs(tHit) < bestDist )
										{
											bestDist = std::abs(tHit);

											const auto & hn0 = highVerts[highTri.vertexIndex(0)].normal();
											const auto & hn1 = highVerts[highTri.vertexIndex(1)].normal();
											const auto & hn2 = highVerts[highTri.vertexIndex(2)].normal();

											highNormal = ((hn0 * (static_cast< vertex_data_t >(1) - uHit - vHit)) + (hn1 * uHit) + (hn2 * vHit)).normalized();
											hit = true;
										}
									}
								}
							}
						}

						/* Convert high-poly normal to tangent space. */
						Math::Vector< 3, vertex_data_t > tsNormal;

						if ( hit )
						{
							tsNormal = {
								Math::Vector< 3, vertex_data_t >::dotProduct(highNormal, lowTangent),
								Math::Vector< 3, vertex_data_t >::dotProduct(highNormal, lowBitangent),
								Math::Vector< 3, vertex_data_t >::dotProduct(highNormal, lowNormal)
							};

							tsNormal = tsNormal.normalized();
						}
						else
						{
							tsNormal = {0, 0, 1}; /* Default: no detail (flat). */
						}

						/* Encode to [0,255]: tangent-space normal [-1,1] → [0,1] → [0,255]. */
						const Color< float > normalColor{
							(tsNormal[Math::X] * 0.5F) + 0.5F,
							(tsNormal[Math::Y] * 0.5F) + 0.5F,
							(tsNormal[Math::Z] * 0.5F) + 0.5F,
							1.0F
						};

						normalMap.setPixel(px, py, normalColor);
					}
				};

				if ( m_threadPool != nullptr )
				{
					m_threadPool->parallelFor(uint32_t{0}, resolution, processRow);
				}
				else
				{
					for ( uint32_t py = 0; py < resolution; ++py )
					{
						processRow(py);
					}
				}

				/* Dilation pass: extend baked pixels into empty space to eliminate UV seams.
				 * Empty pixels have the default normal (0.5, 0.5, 1.0, 1.0) = RGB(128, 128, 255). */
				dilateNormalMap(normalMap, 8);

				if ( this->isCancelled() )
				{
					return {};
				}

				return normalMap;
			}

			/**
			 * @brief Dilates a normal map by extending baked texels into empty space.
			 * @param normalMap The normal map to dilate in-place.
			 * @param iterations Number of dilation passes (each extends by 1 pixel).
			 */
			static
			void
			dilateNormalMap (PixelFactory::Pixmap< uint8_t > & normalMap, uint32_t iterations) noexcept
			{
				const auto width = normalMap.width();
				const auto height = normalMap.height();
				const auto colorCount = normalMap.colorCount();

				/* Detect empty pixels: RGB close to (128, 128, 255) = default flat normal. */
				std::vector< bool > filled(static_cast< size_t >(width) * height, false);
				auto & data = normalMap.data();

				for ( uint32_t y = 0; y < height; ++y )
				{
					for ( uint32_t x = 0; x < width; ++x )
					{
						const auto offset = (static_cast< size_t >(y) * width + x) * colorCount;
						const auto r = data[offset];
						const auto g = data[offset + 1];
						const auto b = data[offset + 2];

						/* A pixel is "filled" if it differs from the default (128, 128, 255). */
						filled[(static_cast< size_t >(y) * width) + x] = !(r == 128 && g == 128 && b == 255);
					}
				}

				/* Offsets for 8-connected neighbors. */
				constexpr int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
				constexpr int dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

				for ( uint32_t iter = 0; iter < iterations; ++iter )
				{
					std::vector< bool > newFilled = filled;
					auto dataCopy = data;

					for ( uint32_t y = 0; y < height; ++y )
					{
						for ( uint32_t x = 0; x < width; ++x )
						{
							const auto idx = (static_cast< size_t >(y) * width) + x;

							if ( filled[idx] )
							{
								continue;
							}

							/* Find the first filled neighbor. */
							for ( int n = 0; n < 8; ++n )
							{
								const auto nx = static_cast< int >(x) + dx[n];
								const auto ny = static_cast< int >(y) + dy[n];

								if ( nx < 0 || nx >= static_cast< int >(width) || ny < 0 || ny >= static_cast< int >(height) )
								{
									continue;
								}

								const auto nIdx = (static_cast< size_t >(ny) * width) + static_cast< size_t >(nx);

								if ( filled[nIdx] )
								{
									const auto srcOffset = nIdx * colorCount;
									const auto dstOffset = idx * colorCount;

									for ( uint32_t c = 0; c < colorCount; ++c )
									{
										dataCopy[dstOffset + c] = data[srcOffset + c];
									}

									newFilled[idx] = true;

									break;
								}
							}
						}
					}

					data = std::move(dataCopy);
					filled = std::move(newFilled);
				}
			}

			const Shape< vertex_data_t, index_data_t > & m_source;
			const Shape< vertex_data_t, index_data_t > & m_bakeSource;
			vertex_data_t m_ratio;
			vertex_data_t m_boundaryPenaltyWeight;
			uint32_t m_normalMapResolution{0};
			ThreadPool * m_threadPool{nullptr};
			std::stop_token m_stopToken;
			const std::atomic_bool * m_cancellationFlag{nullptr};
			mutable PixelFactory::Pixmap< uint8_t > m_normalMap;
	};
}
