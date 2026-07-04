/*
 * fuzzing/fuzz_http_response.cpp
 * This file is part of Emeraude-Base
 *
 * libFuzzer target for the HTTP/1.1 response parser (Ave robustus! — A.3 / Network).
 * HTTPResponseParser is THE untrusted-input boundary of the HTTPS client: it consumes
 * whatever bytes a (possibly hostile) server sends. This target drives it two ways from
 * the same input — one whole feed, and a byte-sliced incremental feed — because the
 * incremental state-machine boundaries (header terminator split across feeds, chunk-size
 * line split, body split) exercise different code paths. Runs under ASan + UBSan with
 * -fno-exceptions, so any hidden throw/UB/OOB surfaces as a crash.
 */

/* STL inclusions. */
#include <cstddef>
#include <cstdint>

/* Local inclusions. */
#include "Network/HTTPResponseParser.hpp"

namespace
{
	using EmEn::Base::Network::HTTPResponseParser;

	/**
	 * @brief Drives one parser to completion over a payload, fed in fixed-size slices.
	 * @param payload A pointer to the bytes.
	 * @param size The number of bytes.
	 * @param sliceSize The feeding granularity (0 = one feed).
	 * @param bodiless Whether to declare the response bodiless (HEAD semantics).
	 */
	void
	drive (const uint8_t * payload, size_t size, size_t sliceSize, bool bodiless) noexcept
	{
		HTTPResponseParser parser;

		if ( bodiless )
		{
			parser.expectBodilessResponse();
		}

		const auto * bytes = reinterpret_cast< const char * >(payload);

		if ( sliceSize == 0 )
		{
			if ( parser.feed(bytes, size) == HTTPResponseParser::Result::NeedMoreData )
			{
				(void)parser.finish();
			}

			/* Touch the outputs so the optimizer cannot elide the work. */
			(void)parser.response().codeResponse();
			(void)parser.bodyBytesDecoded();

			return;
		}

		auto result = HTTPResponseParser::Result::NeedMoreData;

		for ( size_t offset = 0; offset < size && result == HTTPResponseParser::Result::NeedMoreData; offset += sliceSize )
		{
			const auto chunk = size - offset < sliceSize ? size - offset : sliceSize;

			result = parser.feed(bytes + offset, chunk);

			/* A streaming consumer drains the body between feeds — exercise that too. */
			(void)parser.body().size();
		}

		if ( result == HTTPResponseParser::Result::NeedMoreData )
		{
			(void)parser.finish();
		}

		(void)parser.response().keepConnectionAlive();
	}
}

extern "C" int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
	if ( size == 0 )
	{
		return 0;
	}

	/* Consume the first byte as a control knob (feeding strategy + bodiless flag),
	 * so a single corpus entry covers both the whole-feed and the incremental paths. */
	const auto control = data[0];
	const auto * payload = data + 1;
	const auto payloadSize = size - 1;

	const bool bodiless = (control & 0x01U) != 0;

	/* Slice sizes chosen to straddle the state boundaries: whole, 1, 3, 7. */
	static constexpr size_t Slices[] = {0, 1, 3, 7};
	const auto sliceSize = Slices[(control >> 1U) & 0x03U];

	drive(payload, payloadSize, sliceSize, bodiless);

	return 0;
}