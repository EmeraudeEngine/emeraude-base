---
id: http-headers-accept-crlf
title: HTTPHeaders::add() / set() accept CR / LF in a header name or value
status: open
priority: high
scope: Network/HTTPHeaders
opened: 2026-11-07
tags: [network, defect]
---

# HTTPHeaders::add() / set() accept CR / LF in a header name or value

## Why

`HTTPHeaders::add()` and `set()` (`src/Network/HTTPHeaders.hpp`) store any name and value; `HTTPRequest::toString()`
(and `HTTPResponse`'s) then write each one as `name: value\r\n`. A `\r\n` inside either injects a header, and with a
body splits the message (the same class as the host CRLF injection fixed 2026-08-27, `docs/caution-points.md`
§ Anything concatenated into a request line must be validated).

Found 2026-11-07 while fixing `HTTPRequest::toString()` (the request target). Latent: no production caller in the cascade
serializes an `HTTPRequest` (`HTTPSClient` builds its own request and gates the caller's headers with
`isRequestHeaderAcceptable()` before connecting); `HTTPServer` writes responses — check whether it serializes caller
header values through `HTTPHeaders`.

## What remains

- Decide the refusal (owner): refuse the field (nothing inserted, `Logging::error`, a `bool` result), or refuse at
  `toString()`. Reuse `HTTPSClient::isRequestHeaderAcceptable()`'s rule (RFC 9110 § 5.1 token for the name, § 5.5 field
  value: no CR / LF / NUL) rather than a second one.
- Check `HTTPServer` and `HTTPResponse` for the same path.
- A failing-then-passing test: a name or a value holding CR, LF or NUL never reaches `toString()`.

## References

- `src/Network/HTTPHeaders.hpp` (`add()`, `set()`), `src/Network/HTTPRequest.cpp` / `HTTPResponse.cpp` (`toString()`),
  `src/Network/HTTPSClient.cpp` (`isRequestHeaderAcceptable()`).
