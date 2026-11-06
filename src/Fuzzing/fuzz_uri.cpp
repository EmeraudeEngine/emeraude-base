/*
 * fuzzing/fuzz_uri.cpp
 * This file is part of Emeraude-Base
 *
 * libFuzzer target for the URI parsers (URI, URIDomain, Query, PercentEncoding, Hostname). A URI comes from data, a
 * console / MCP command or a redirect's Location header, and its host and request target go on the wire: the host
 * percent-decode was a CRLF-injection site during the 2026-08-27 network audit. Each input is parsed, re-emitted,
 * parsed again, and resolved as a reference against itself; the target traps on any CR, LF, space or NUL in a request
 * target or a host (a request-line or header injection), on top of ASan / UBSan with -fno-exceptions.
 * The parsers trace every refusal (an illegal host, an invalid option): almost every input is one, so the target keeps
 * only Fatal diagnostics — the flood of stderr writes cost throughput and buried libFuzzer's own lines (macOS peer,
 * 2026-11-06).
 */

/* STL inclusions. */
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

/* Local inclusions. */
#include "Logging/Logging.hpp"
#include "Logging/Severity.hpp"
#include "Network/URI.hpp"

namespace
{
	using EmEn::Base::Network::URI;

	/**
	 * @brief Traps when a string carries a byte that would break an HTTP request line or header.
	 * @param string The string going on the wire.
	 */
	void
	requireWireSafe (std::string_view string) noexcept
	{
		for ( const auto character : string )
		{
			if ( character == '\r' || character == '\n' || character == '\0' )
			{
				__builtin_trap();
			}
		}
	}

	/**
	 * @brief Checks every wire-bound view of a URI.
	 * @param uri The URI.
	 */
	void
	check (const URI & uri) noexcept
	{
		const auto resource = uri.resource();

		requireWireSafe(resource);

		/* A request target never holds a space either (it separates the method, the target and the version). */
		if ( resource.find(' ') != std::string::npos )
		{
			__builtin_trap();
		}

		requireWireSafe(uri.uriDomain().host());
		requireWireSafe(uri.redacted());

		/* Touch the decoded views so the optimizer cannot elide them. */
		(void)uri.path().empty();
		(void)uri.query().variables().size();
	}
}

extern "C" int
LLVMFuzzerInitialize (int * /*argc*/, char *** /*argv*/)
{
	using namespace EmEn::Base;

	/* Every refusal of a hostile input is an expected Error / Warning trace: only a Fatal one is news. */
	Logging::setSink([] (Severity severity, const char * tag, std::string_view message) {
		if ( severity == Severity::Fatal )
		{
			std::cerr << '[' << tag << "] (Fatal) " << message << '\n';
		}
	});

	return 0;
}

extern "C" int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
	/* NOTE: the bytes copied one by one into chars, through a span: no reinterpret_cast, no pointer arithmetic. */
	const std::span< const uint8_t > bytes{data, size};
	const std::string input(bytes.begin(), bytes.end());

	const URI uri{input};
	check(uri);

	/* Re-emit then parse again: what we print must parse into something just as safe. */
	const URI reparsed{to_string(uri)};
	check(reparsed);

	/* The input as a reference resolved against itself (redirect handling). */
	check(URI::resolve(uri, input));

	(void)URI::isURL(input);

	return 0;
}
