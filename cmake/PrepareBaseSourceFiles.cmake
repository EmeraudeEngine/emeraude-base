# Explicit source lists for emeraude-base (no globbing — new files must be added here).
# Mirrors the engine's PrepareEngineSourceFiles.cmake approach.
#
# Per-module split (Ave robustus! gap #1): each module's compiled sources live in their own
# EMERAUDE_BASE_<MODULE>_SOURCES list, built as an OBJECT library (emeraude::base::<module>) and
# aggregated into the emeraude::base umbrella. Header-only modules (math, algorithms, animation,
# platform) have no sources here — they are INTERFACE targets. EMERAUDE_BASE_SOURCES is the
# not-yet-split remainder (currently empty: every compiled file belongs to a module).
#
# pixel became a compiled module in 2026-08: the image codecs must NOT be inlined into a consumer,
# or that consumer defines libpng/libjpeg/libtiff/FreeType symbols in its own binary, where they
# interpose the system copies used by anything the process loads. See
# cmake/HideThirdPartyExports.cmake for the measured defect this closes.

# core — the flat src/ root utilities + the logging hook.
set(EMERAUDE_BASE_CORE_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/FastJSON.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/FileTimestamps.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/INIParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Locale.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Logging/Logging.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ObservableTrait.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ObserverTrait.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/SourceCodeParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/String.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ThreadPool.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/TokenFormatter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Variant.cpp
)

# hash module — no external dependency.
set(EMERAUDE_BASE_HASH_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/Hash/CRC32.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Hash/Hash.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Hash/MD5.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Hash/SHA1.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Hash/SHA256.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Hash/SHA512.cpp
)

# gametools module — no external dependency.
set(EMERAUDE_BASE_GAMETOOLS_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/GameTools/CardDeck.cpp
)

# time module — no external dependency.
set(EMERAUDE_BASE_TIME_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/Time/Statistics/Abstract.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Time/Statistics/CPUTime.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Time/Time.cpp
)

# debug module — no external dependency.
set(EMERAUDE_BASE_DEBUG_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/Debug/Statistics.cpp
)

# compression module — ZLIB, LZMA.
set(EMERAUDE_BASE_COMPRESSION_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/Compression/LZMA/Compressor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Compression/LZMA.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Compression/LZMA/Decompressor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Compression/ZLIB.cpp
)

# io module — libzip.
set(EMERAUDE_BASE_IO_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/IO/IO.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/IO/ZipReader.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/IO/ZipWriter.cpp
)

# network module — ASIO (header-only) + LibreSSL (TLS).
set(EMERAUDE_BASE_NETWORK_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/Network/Hostname.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Network/HTTPHeaders.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Network/HTTPRequest.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Network/HTTPResponse.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Network/HTTPResponseParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Network/HTTPSClient.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Network/PercentEncoding.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Network/Query.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Network/TLSConnection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Network/TrustStore.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Network/URI.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Network/URIDomain.cpp
)

# pixel module — PixelFactory (mostly header-only; the third-party image codecs are compiled here
# so libpng/libjpeg/libtiff/FreeType never reach a consumer's own binary).
set(EMERAUDE_BASE_PIXEL_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/PixelFactory/FileFormatJpeg.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/PixelFactory/FileFormatPNG.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/PixelFactory/FileFormatTIFF.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/PixelFactory/Font.cpp
)

# vertex module — VertexFactory (mostly header-only; one compiled generator).
set(EMERAUDE_BASE_VERTEX_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/VertexFactory/TreeGenerator.cpp
)

# wave module — sndfile, samplerate, TinySoundFont.
set(EMERAUDE_BASE_WAVE_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/WaveFactory/FileFormatSNDFile.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/WaveFactory/Processor.cpp
)

# Not-yet-split remainder (every compiled file now belongs to a module above).
set(EMERAUDE_BASE_SOURCES)

set(EMERAUDE_BASE_TEST_SOURCES
	# TinySoundFont implementation, compiled into the test binary only (see the file header).
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/TinySoundFontImpl.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_AnimationCubicSpline.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_Compression.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_Debug.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_FastJSON.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_Hash.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_INIParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_IO.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_LineFormula.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_Logging.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_MD5AnimParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_MathBasics.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_MathCartesianFrame.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_MathMatrix.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_MathOrientedCuboid.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_MathQuaternion.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_MathSpace2D.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_MathSpace3D.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_MathTransformConversions.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_MathVector.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_NetworkHTTPResponseParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_NetworkHTTPSClient.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_NetworkHTTPSClientLive.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_NetworkTLSConnection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_NetworkTrustStore.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_NetworkURI.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_NodeTrait.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_ObserverPattern.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_Platform.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_PixelFactoryColor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_PixelFactoryFileFormats.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_PixelFactoryPixmap.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_PixelFactoryPixmapFormat.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_PixelFactoryProcessor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_PixelFactoryTextPixmap.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_StaticVector.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_String.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_ThreadPool.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_Time.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_TokenFormatter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_Variant.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_Version.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_VertexFactoryFileFormats.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_VertexFactoryShapeBuilder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_VertexFactoryShapeGenerator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_WaveFactoryFileFormats.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Testing/test_ZipArchive.cpp
)

# Ave robustus! A.5 — Google Benchmark sources (EmeraudeBaseBenchmarks target).
set(EMERAUDE_BASE_BENCHMARK_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/Benchmarking/bench_PixelFactoryResize.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Benchmarking/bench_PixelFactoryMirror.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Benchmarking/bench_PixelFactoryConvert.cpp
)