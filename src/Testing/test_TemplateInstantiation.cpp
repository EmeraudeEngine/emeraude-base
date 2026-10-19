/*
 * src/Testing/test_TemplateInstantiation.cpp
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

/* NOTE: The compile-time guard of the library (owner decision, 2026-10-19). A member of a class template is compiled
 * only when something calls it: 19 members that had NEVER compiled (the LSCM UV unwrap, Vector's span constructor,
 * Processor::blit(), ShapeAssembler::mergeList()...) and 11 headers no translation unit included were found this way,
 * none visible to the compiler, clang-tidy or the tests. This translation unit
 *   1. includes EVERY header of the library, so each one compiles under the PARANOID warnings of GCC, clang and MSVC;
 *   2. explicitly instantiates every class template with the arguments the cascade uses, which compiles EVERY member
 *      not excluded by its own 'requires' clause.
 * A new header goes in the list, a new class template gets its instantiation (with each supported argument). A member
 * valid only for some arguments takes a 'requires' clause, never a removal from this list.
 * ⚠️ A specialization a .cpp instantiates (FileFormatJpeg / PNG / TIFF / WebP< uint8_t, uint32_t >, Font< uint8_t >:
 * their header declares it 'extern template') is NOT repeated here — an explicit instantiation definition may appear
 * once in a program (GCC and clang accepted the duplicate silently, MSVC refused it: C4661). That .cpp is its guard.
 * See docs/subsystems/source-tree/02-base-specific-rules.md. */

/* STL inclusions. */
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

/* Local inclusions: every header of the library. */
// IWYU pragma: begin_keep
#include "Algorithms/DelaunayTriangulation.hpp"
#include "Algorithms/DiamondSquare.hpp"
#include "Algorithms/Mandelbrot.hpp"
#include "Algorithms/PerlinNoise.hpp"
#include "Algorithms/VoronoiNoise.hpp"
#include "Algorithms/WorleyNoise.hpp"
#include "Animation/AnimationChannel.hpp"
#include "Animation/AnimationClip.hpp"
#include "Animation/Joint.hpp"
#include "Animation/MD5AnimParser.hpp"
#include "Animation/Skeleton.hpp"
#include "Animation/Skin.hpp"
#include "Any.hpp"
#include "AnyValue.hpp"
#include "BaseUtility.hpp"
#include "BlobTrait.hpp"
#include "Compression/LZMA.hpp"
#include "Compression/LZMA/Compressor.hpp"
#include "Compression/LZMA/Decompressor.hpp"
#include "Compression/ZLIB.hpp"
#include "ConditionVariable.hpp"
#include "Debug/ConcurrencyDetector.hpp"
#include "Debug/Dummy.hpp"
#include "Debug/Statistics.hpp"
#include "FastJSON.hpp"
#include "FileTimestamps.hpp"
#include "FlagArrayTrait.hpp"
#include "FlagTrait.hpp"
#include "FlatHashMap.hpp"
#include "GameTools/CardDeck.hpp"
#include "GameTools/CardHand.hpp"
#include "GameTools/Dice.hpp"
#include "Hash/CRC32.hpp"
#include "Hash/FNV1a.hpp"
#include "Hash/Hash.hpp"
#include "Hash/MD5.hpp"
#include "Hash/SHA1.hpp"
#include "Hash/SHA256.hpp"
#include "Hash/SHA512.hpp"
#include "Hash/Types.hpp"
#include "INIParser.hpp"
#include "IO/ByteStream.hpp"
#include "IO/FileStream.hpp"
#include "IO/IO.hpp"
#include "IO/MemoryStream.hpp"
#include "IO/ZipReader.hpp"
#include "IO/ZipWriter.hpp"
#include "Locale.hpp"
#include "Logging/Logging.hpp"
#include "Logging/Severity.hpp"
#include "Math/BSpline.hpp"
#include "Math/Base.hpp"
#include "Math/BezierCurve.hpp"
#include "Math/CartesianFrame.hpp"
#include "Math/CurveShape.hpp"
#include "Math/CurveTessellation.hpp"
#include "Math/DeterminantAverage.hpp"
#include "Math/LineFormula.hpp"
#include "Math/Matrix.hpp"
#include "Math/OctahedralMapping.hpp"
#include "Math/OrientedCuboid.hpp"
#include "Math/PiecewiseLinear.hpp"
#include "Math/Plane.hpp"
#include "Math/Quaternion.hpp"
#include "Math/Range.hpp"
#include "Math/RigidBody.hpp"
#include "Math/Space2D/AARectangle.hpp"
#include "Math/Space2D/Circle.hpp"
#include "Math/Space2D/Collisions/CircleRectangle.hpp"
#include "Math/Space2D/Collisions/PointCircle.hpp"
#include "Math/Space2D/Collisions/PointRectangle.hpp"
#include "Math/Space2D/Collisions/PointSegment.hpp"
#include "Math/Space2D/Collisions/PointTriangle.hpp"
#include "Math/Space2D/Collisions/SamePrimitive.hpp"
#include "Math/Space2D/Collisions/TriangleCircle.hpp"
#include "Math/Space2D/Collisions/TriangleRectangle.hpp"
#include "Math/Space2D/Intersections/LineCircle.hpp"
#include "Math/Space2D/Intersections/LineLine.hpp"
#include "Math/Space2D/Intersections/LineRectangle.hpp"
#include "Math/Space2D/Intersections/LineTriangle.hpp"
#include "Math/Space2D/Intersections/SegmentCircle.hpp"
#include "Math/Space2D/Intersections/SegmentRectangle.hpp"
#include "Math/Space2D/Intersections/SegmentSegment.hpp"
#include "Math/Space2D/Intersections/SegmentTriangle.hpp"
#include "Math/Space2D/Line.hpp"
#include "Math/Space2D/Point.hpp"
#include "Math/Space2D/SAT.hpp"
#include "Math/Space2D/Segment.hpp"
#include "Math/Space2D/Triangle.hpp"
#include "Math/Space3D/AACuboid.hpp"
#include "Math/Space3D/Capsule.hpp"
#include "Math/Space3D/Casts/ConvexDistance.hpp"
#include "Math/Space3D/Casts/ShapeCast.hpp"
#include "Math/Space3D/Collisions/CapsuleCuboid.hpp"
#include "Math/Space3D/Collisions/CapsulePoint.hpp"
#include "Math/Space3D/Collisions/CapsuleSphere.hpp"
#include "Math/Space3D/Collisions/CapsuleTriangle.hpp"
#include "Math/Space3D/Collisions/PointCuboid.hpp"
#include "Math/Space3D/Collisions/PointSphere.hpp"
#include "Math/Space3D/Collisions/PointTriangle.hpp"
#include "Math/Space3D/Collisions/SamePrimitive.hpp"
#include "Math/Space3D/Collisions/SphereCuboid.hpp"
#include "Math/Space3D/Collisions/TriangleCuboid.hpp"
#include "Math/Space3D/Collisions/TriangleSphere.hpp"
#include "Math/Space3D/Contacts/BoxBox.hpp"
#include "Math/Space3D/Contacts/BoxTriangle.hpp"
#include "Math/Space3D/Contacts/CapsuleBox.hpp"
#include "Math/Space3D/Contacts/CapsuleTriangle.hpp"
#include "Math/Space3D/Contacts/ContactManifold.hpp"
#include "Math/Space3D/Contacts/RoundShapes.hpp"
#include "Math/Space3D/Contacts/SphereBox.hpp"
#include "Math/Space3D/Contacts/SphereTriangle.hpp"
#include "Math/Space3D/Intersections/LineCapsule.hpp"
#include "Math/Space3D/Intersections/LineCuboid.hpp"
#include "Math/Space3D/Intersections/LineLine.hpp"
#include "Math/Space3D/Intersections/LineSphere.hpp"
#include "Math/Space3D/Intersections/LineTriangle.hpp"
#include "Math/Space3D/Intersections/SegmentCapsule.hpp"
#include "Math/Space3D/Intersections/SegmentCuboid.hpp"
#include "Math/Space3D/Intersections/SegmentSegment.hpp"
#include "Math/Space3D/Intersections/SegmentSphere.hpp"
#include "Math/Space3D/Intersections/SegmentTriangle.hpp"
#include "Math/Space3D/Line.hpp"
#include "Math/Space3D/OrientedBox.hpp"
#include "Math/Space3D/Point.hpp"
#include "Math/Space3D/SAT.hpp"
#include "Math/Space3D/Segment.hpp"
#include "Math/Space3D/Sphere.hpp"
#include "Math/Space3D/Triangle.hpp"
#include "Math/Space3D/TriangleMesh.hpp"
#include "Math/TransformUtils.hpp"
#include "Math/Vector.hpp"
#include "Mutex.hpp"
#include "NameableTrait.hpp"
#include "Network/GracefulCloser.hpp"
#include "Network/HTTPHeaders.hpp"
#include "Network/HTTPRequest.hpp"
#include "Network/HTTPResponse.hpp"
#include "Network/HTTPResponseParser.hpp"
#include "Network/HTTPSClient.hpp"
#include "Network/HTTPServer.hpp"
#include "Network/HappyEyeballs.hpp"
#include "Network/Hostname.hpp"
#include "Network/PercentEncoding.hpp"
#include "Network/Query.hpp"
#include "Network/TLSConnection.hpp"
#include "Network/TrustStore.hpp"
#include "Network/URI.hpp"
#include "Network/URIDomain.hpp"
#include "Network/asio_throw_exception.hpp"
#include "NodeTrait.hpp"
#include "ObservableTrait.hpp"
#include "ObserverTrait.hpp"
#include "PixelFactory/Color.hpp"
#include "PixelFactory/DefaultFont.hpp"
#include "PixelFactory/FileFormatHDR.hpp"
#include "PixelFactory/FileFormatInterface.hpp"
#include "PixelFactory/FileFormatJpeg.hpp"
#include "PixelFactory/FileFormatPNG.hpp"
#include "PixelFactory/FileFormatTIFF.hpp"
#include "PixelFactory/FileFormatTarga.hpp"
#include "PixelFactory/FileFormatWebP.hpp"
#include "PixelFactory/FileIO.hpp"
#include "PixelFactory/Font.hpp"
#include "PixelFactory/Gradient.hpp"
#include "PixelFactory/IOCommon.hpp"
#include "PixelFactory/Margin.hpp"
#include "PixelFactory/Pixmap.hpp"
#include "PixelFactory/Processor.hpp"
#include "PixelFactory/StreamIO.hpp"
#include "PixelFactory/TextProcessor.hpp"
#include "PixelFactory/Types.hpp"
#include "PortableRandom.hpp"
#include "Randomizer.hpp"
#include "ScopedRegistration.hpp"
#include "SharedMutex.hpp"
#include "SourceCodeParser.hpp"
#include "StaticVector.hpp"
#include "String.hpp"
#include "Thread.hpp"
#include "ThreadPool.hpp"
#include "ThreadSafety.hpp"
#include "Time/Elapsed/Abstract.hpp"
#include "Time/Elapsed/CPUTime.hpp"
#include "Time/Elapsed/PrintScopeCPUTime.hpp"
#include "Time/Elapsed/PrintScopeRealTime.hpp"
#include "Time/Elapsed/RealTime.hpp"
#include "Time/Elapsed/ScopeCPUTime.hpp"
#include "Time/Elapsed/ScopeRealTime.hpp"
#include "Time/EventTrait.hpp"
#include "Time/Statistics/Abstract.hpp"
#include "Time/Statistics/CPUTime.hpp"
#include "Time/Statistics/RealTime.hpp"
#include "Time/Time.hpp"
#include "Time/TimedEvent.hpp"
#include "Time/Types.hpp"
#include "TokenFormatter.hpp"
#include "Variant.hpp"
#include "Version.hpp"
#include "VertexFactory/CapUVMapping.hpp"
#include "VertexFactory/FileFormatInterface.hpp"
#include "VertexFactory/FileFormatMDx.hpp"
#include "VertexFactory/FileFormatNative.hpp"
#include "VertexFactory/FileFormatOBJ.hpp"
#include "VertexFactory/FileFormatSTL.hpp"
#include "VertexFactory/FileIO.hpp"
#include "VertexFactory/Grid.hpp"
#include "VertexFactory/GridQuad.hpp"
#include "VertexFactory/Normal.hpp"
#include "VertexFactory/Shape.hpp"
#include "VertexFactory/ShapeAssembler.hpp"
#include "VertexFactory/ShapeBuilder.hpp"
#include "VertexFactory/ShapeBuilderOptions.hpp"
#include "VertexFactory/ShapeDecimator.hpp"
#include "VertexFactory/ShapeEdge.hpp"
#include "VertexFactory/ShapeGenerator.hpp"
#include "VertexFactory/ShapeLoadResult.hpp"
#include "VertexFactory/ShapeProcessor.hpp"
#include "VertexFactory/ShapeSimplifier.hpp"
#include "VertexFactory/ShapeSplitter.hpp"
#include "VertexFactory/ShapeTriangle.hpp"
#include "VertexFactory/ShapeVertex.hpp"
#include "VertexFactory/Silhouette.hpp"
#include "VertexFactory/StreamIO.hpp"
#include "VertexFactory/TextureCoordinates.hpp"
#include "VertexFactory/TreeColonizationGrower.hpp"
#include "VertexFactory/TreeGenerator.hpp"
#include "VertexFactory/TreeGrowthCurve.hpp"
#include "VertexFactory/TreeLeafAttachment.hpp"
#include "VertexFactory/TreeMesh.hpp"
#include "VertexFactory/TreeParameters.hpp"
#include "VertexFactory/TreeParametricGrower.hpp"
#include "VertexFactory/TreeSegment.hpp"
#include "VertexFactory/TreeSkeleton.hpp"
#include "VertexFactory/TreeSkinner.hpp"
#include "VertexFactory/TreeSkinningOptions.hpp"
#include "VertexFactory/Types.hpp"
#include "VertexFactory/XRayAnalyzer.hpp"
#include "WaveFactory/FileFormatInterface.hpp"
#include "WaveFactory/FileFormatJSON.hpp"
#include "WaveFactory/FileFormatMIDI.hpp"
#include "WaveFactory/FileFormatSNDFile.hpp"
#include "WaveFactory/FileIO.hpp"
#include "WaveFactory/Processor.hpp"
#include "WaveFactory/SFXScript.hpp"
#include "WaveFactory/StreamIO.hpp"
#include "WaveFactory/Synthesizer.hpp"
#include "WaveFactory/Types.hpp"
#include "WaveFactory/Wave.hpp"
#include "emeraude_platform.hpp"

// IWYU pragma: end_keep

/* Every class template, with the arguments the cascade uses. */
template class EmEn::Base::Algorithms::DelaunayTriangulation<>;
template class EmEn::Base::Algorithms::DiamondSquare<>;
template class EmEn::Base::Algorithms::Mandelbrot<>;
template class EmEn::Base::Algorithms::PerlinNoise<>;
template class EmEn::Base::Algorithms::VoronoiNoise<>;
template class EmEn::Base::Algorithms::WorleyNoise<>;
template struct EmEn::Base::Animation::AnimationChannel<>;
template class EmEn::Base::Animation::AnimationClip<>;
template struct EmEn::Base::Animation::Joint<>;
template class EmEn::Base::Animation::MD5AnimParser<>;
template struct EmEn::Base::Animation::QuaternionKeyFrame<>;
template class EmEn::Base::Animation::Skeleton<>;
template class EmEn::Base::Animation::Skin<>;
template struct EmEn::Base::Animation::VectorKeyFrame<>;
template class EmEn::Base::FlagArrayTrait<>;
template class EmEn::Base::FlagTrait<>;
template class EmEn::Base::FlatHashMap< uint32_t, int >;
template class EmEn::Base::Math::BSpline< 3, float >;
template class EmEn::Base::Math::BSplinePoint< 3, float >;
template class EmEn::Base::Math::BezierCurve< 3, float >;
template class EmEn::Base::Math::CartesianFrame<>;
template class EmEn::Base::Math::CartesianFrame< double >;
template class EmEn::Base::Math::CurveShape< double >;
template class EmEn::Base::Math::CurveShape< float >;
template class EmEn::Base::Math::DeterminantAverage<>;
template class EmEn::Base::Math::DeterminantAverage< double >;
template struct EmEn::Base::Math::ImposterCellFrame<>;
template struct EmEn::Base::Math::ImposterCellFrame< double >;
template class EmEn::Base::Math::LineFormula<>;
template class EmEn::Base::Math::LineFormula< double >;
template class EmEn::Base::Math::Matrix< 2, double >;
template class EmEn::Base::Math::Matrix< 2, float >;
template class EmEn::Base::Math::Matrix< 3, double >;
template class EmEn::Base::Math::Matrix< 3, float >;
template class EmEn::Base::Math::Matrix< 4, double >;
template class EmEn::Base::Math::Matrix< 4, float >;
template struct EmEn::Base::Math::OctahedralBlend<>;
template struct EmEn::Base::Math::OctahedralBlend< double >;
template class EmEn::Base::Math::OrientedCuboid<>;
template class EmEn::Base::Math::OrientedCuboid< double >;
template class EmEn::Base::Math::PiecewiseLinear<>;
template class EmEn::Base::Math::PiecewiseLinear< double >;
template class EmEn::Base::Math::Plane<>;
template class EmEn::Base::Math::Plane< double >;
template class EmEn::Base::Math::Quaternion<>;
template class EmEn::Base::Math::Quaternion< double >;
template class EmEn::Base::Math::Range<>;
template class EmEn::Base::Math::Range< double >;
template class EmEn::Base::Math::Space2D::AARectangle< double >;
template class EmEn::Base::Math::Space2D::AARectangle< float >;
template class EmEn::Base::Math::Space2D::AARectangle< int32_t >;
template class EmEn::Base::Math::Space2D::AARectangle< uint32_t >;
template class EmEn::Base::Math::Space2D::Circle<>;
template class EmEn::Base::Math::Space2D::Circle< double >;
template class EmEn::Base::Math::Space2D::Line<>;
template class EmEn::Base::Math::Space2D::Line< double >;
template class EmEn::Base::Math::Space2D::Segment<>;
template class EmEn::Base::Math::Space2D::Segment< double >;
template class EmEn::Base::Math::Space2D::Triangle<>;
template class EmEn::Base::Math::Space2D::Triangle< double >;
template class EmEn::Base::Math::Space3D::AACuboid<>;
template class EmEn::Base::Math::Space3D::AACuboid< double >;
template class EmEn::Base::Math::Space3D::Capsule<>;
template class EmEn::Base::Math::Space3D::Capsule< double >;
template class EmEn::Base::Math::Space3D::CastHit<>;
template class EmEn::Base::Math::Space3D::CastHit< double >;
template struct EmEn::Base::Math::Space3D::ClosestPoints< double >;
template struct EmEn::Base::Math::Space3D::ClosestPoints< float >;
template class EmEn::Base::Math::Space3D::ContactManifold<>;
template class EmEn::Base::Math::Space3D::ContactManifold< double >;
template class EmEn::Base::Math::Space3D::ContactPoint<>;
template class EmEn::Base::Math::Space3D::ContactPoint< double >;
template class EmEn::Base::Math::Space3D::ConvexPolytope< double >;
template class EmEn::Base::Math::Space3D::ConvexPolytope< float >;
template class EmEn::Base::Math::Space3D::Line<>;
template class EmEn::Base::Math::Space3D::Line< double >;
template class EmEn::Base::Math::Space3D::OrientedBox<>;
template class EmEn::Base::Math::Space3D::OrientedBox< double >;
template class EmEn::Base::Math::Space3D::Segment<>;
template class EmEn::Base::Math::Space3D::Segment< double >;
template class EmEn::Base::Math::Space3D::Sphere<>;
template class EmEn::Base::Math::Space3D::Sphere< double >;
template class EmEn::Base::Math::Space3D::Triangle<>;
template class EmEn::Base::Math::Space3D::Triangle< double >;
template class EmEn::Base::Math::Space3D::TriangleMesh<>;
template class EmEn::Base::Math::Space3D::TriangleMesh< double >;
template struct EmEn::Base::Math::TRSDecomposition<>;
template struct EmEn::Base::Math::TRSDecomposition< double >;
template class EmEn::Base::Math::Vector< 2, double >;
template class EmEn::Base::Math::Vector< 2, float >;
template class EmEn::Base::Math::Vector< 2, int32_t >;
template class EmEn::Base::Math::Vector< 3, double >;
template class EmEn::Base::Math::Vector< 3, float >;
template class EmEn::Base::Math::Vector< 4, double >;
template class EmEn::Base::Math::Vector< 4, float >;
template class EmEn::Base::Math::Vector< 4, int32_t >;
template class EmEn::Base::NodeTrait<>;
template class EmEn::Base::PixelFactory::ASCIIGlyphArray<>;
template class EmEn::Base::PixelFactory::Color<>;
template class EmEn::Base::PixelFactory::Color< double >;
template class EmEn::Base::PixelFactory::FileFormatHDR<>;
template class EmEn::Base::PixelFactory::FileFormatInterface<>;
template class EmEn::Base::PixelFactory::FileFormatTarga<>;
template class EmEn::Base::PixelFactory::Gradient<>;
template class EmEn::Base::PixelFactory::Margin<>;
template class EmEn::Base::PixelFactory::Pixmap<>;
template class EmEn::Base::PixelFactory::Processor<>;
template struct EmEn::Base::PixelFactory::RawPixmapData<>;
template class EmEn::Base::PixelFactory::TextProcessor<>;
template struct EmEn::Base::PortableHash< uint32_t >;
template class EmEn::Base::PortableRandom::UniformInteger< int32_t >;
template class EmEn::Base::PortableRandom::UniformInteger< uint32_t >;
template class EmEn::Base::PortableRandom::UniformInteger< uint64_t >;
template class EmEn::Base::PortableRandom::UniformReal< double >;
template class EmEn::Base::PortableRandom::UniformReal< float >;
template class EmEn::Base::Randomizer<>;
template class EmEn::Base::StaticVector< int, 8 >;
template class EmEn::Base::Time::Elapsed::PrintScopeCPUTime<>;
template class EmEn::Base::Time::Elapsed::PrintScopeCPUTimeThreshold<>;
template class EmEn::Base::Time::Elapsed::PrintScopeRealTime<>;
template class EmEn::Base::Time::Elapsed::PrintScopeRealTimeThreshold<>;
template class EmEn::Base::Time::Elapsed::RealTime<>;
template class EmEn::Base::Time::Elapsed::ScopeRealTime<>;
template class EmEn::Base::Time::EventTrait<>;
template class EmEn::Base::Time::Statistics::RealTime<>;
template class EmEn::Base::Time::TimedEvent<>;
template struct EmEn::Base::VertexFactory::BoundaryLoop<>;
template struct EmEn::Base::VertexFactory::DiamondSquareParams<>;
template class EmEn::Base::VertexFactory::FileFormatInterface<>;
template class EmEn::Base::VertexFactory::FileFormatMDx<>;
template class EmEn::Base::VertexFactory::FileFormatNative<>;
template class EmEn::Base::VertexFactory::FileFormatOBJ<>;
template class EmEn::Base::VertexFactory::FileFormatSTL<>;
template class EmEn::Base::VertexFactory::Grid<>;
template class EmEn::Base::VertexFactory::GridQuad<>;
template class EmEn::Base::VertexFactory::OBJVertex< uint32_t >;
template struct EmEn::Base::VertexFactory::PerlinNoiseParams<>;
template class EmEn::Base::VertexFactory::Shape<>;
template class EmEn::Base::VertexFactory::ShapeAssembler<>;
template class EmEn::Base::VertexFactory::ShapeBuilder<>;
template class EmEn::Base::VertexFactory::ShapeBuilderOptions<>;
template class EmEn::Base::VertexFactory::ShapeDecimator<>;
template class EmEn::Base::VertexFactory::ShapeEdge<>;
template struct EmEn::Base::VertexFactory::ShapeLoadResult<>;
template class EmEn::Base::VertexFactory::ShapeProcessor<>;
template class EmEn::Base::VertexFactory::ShapeSplitter<>;
template class EmEn::Base::VertexFactory::ShapeTriangle<>;
template class EmEn::Base::VertexFactory::ShapeVertex<>;
template class EmEn::Base::VertexFactory::Silhouette<>;
template struct EmEn::Base::VertexFactory::SplitResult<>;
template class EmEn::Base::VertexFactory::TreeChildTable<>;
template class EmEn::Base::VertexFactory::TreeColonizationGrower<>;
template class EmEn::Base::VertexFactory::TreeGrowthCurve<>;
template class EmEn::Base::VertexFactory::TreeLeafAttachment<>;
template class EmEn::Base::VertexFactory::TreeLevelParameters<>;
template class EmEn::Base::VertexFactory::TreeMesh<>;
template class EmEn::Base::VertexFactory::TreeParameters<>;
template class EmEn::Base::VertexFactory::TreeParametricGrower<>;
template class EmEn::Base::VertexFactory::TreeSegment<>;
template class EmEn::Base::VertexFactory::TreeSkeleton<>;
template class EmEn::Base::VertexFactory::TreeSkinner<>;
template class EmEn::Base::VertexFactory::TreeSkinningOptions<>;
template class EmEn::Base::VertexFactory::XRayAnalyzer<>;
template class EmEn::Base::WaveFactory::FileFormatInterface<>;
template class EmEn::Base::WaveFactory::FileFormatJSON<>;
template class EmEn::Base::WaveFactory::FileFormatMIDI<>;
template class EmEn::Base::WaveFactory::FileFormatSNDFile< float >;
template class EmEn::Base::WaveFactory::SFXScript<>;
template class EmEn::Base::WaveFactory::Synthesizer<>;
template class EmEn::Base::WaveFactory::Wave<>;

/* NOTE: The constrained alias templates carry a type-constraint, never a requires-clause: cppcheck 2.22 abandons the
 * whole translation unit on 'template< ... > requires (...) using X = ...;' (2026-10-19). The constraint was MOVED, not
 * removed — these assertions prove it still refuses what the former requires-clause refused. */
namespace
{
	template< typename precision_t >
	concept HasPoint2D = requires { typename EmEn::Base::Math::Space2D::Point< precision_t >; };

	template< typename precision_t >
	concept HasPoint3D = requires { typename EmEn::Base::Math::Space3D::Point< precision_t >; };

	template< typename index_data_t >
	concept HasOBJTriangle = requires { typename EmEn::Base::VertexFactory::OBJTriangle< index_data_t >; };
}

static_assert(HasPoint2D< float > && HasPoint2D< int32_t > && HasPoint2D< uint32_t >);
static_assert(!HasPoint2D< const char * > && !HasPoint2D< std::array< float, 2 > >);
static_assert(HasPoint3D< float > && HasPoint3D< double >);
static_assert(!HasPoint3D< int32_t > && !HasPoint3D< uint32_t >);
static_assert(HasOBJTriangle< uint16_t > && HasOBJTriangle< uint32_t >);
static_assert(!HasOBJTriangle< int32_t > && !HasOBJTriangle< float >);
