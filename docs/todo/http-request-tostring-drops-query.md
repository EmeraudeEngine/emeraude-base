---
id: http-request-tostring-drops-query
title: HTTPRequest::toString() writes the decoded path and drops the query
status: open
priority: high
scope: Network/HTTPRequest
opened: 2026-11-06
tags: [network, defect]
---

# HTTPRequest::toString() writes the decoded path and drops the query

## Why

`HTTPRequest::toString()` (`src/Network/HTTPRequest.cpp`) builds its request line from
`IO::toGenericU8String(m_resource.path())`: the DECODED path (a `%20` becomes a raw space, a `%2F` a `/`, a
non-ASCII byte goes out raw) and nothing of the query. A request line written that way is wrong for any resource with a
query, and invalid HTTP (or a request-line injection) for a path that decodes to a space, CR or LF.

Found 2026-11-06 while the URI layer was made to keep its encoded bytes (`docs/plans/network-tls/README.md`
§ The URI keeps its encoded bytes). `HTTPSClient` is NOT affected: it builds its request target from `URI::resource()`
(`HTTPSClient.cpp`), which now re-emits the parsed bytes. No caller in the cascade uses `HTTPRequest::toString()`
outside its tests (`test_NetworkHTTPRequest.cpp`, the `requestLine*` tests), so the defect is latent.

## What remains

- Build the request target from `m_resource.resource()` (encoded path, `?`, encoded query), keeping the "one leading
  slash" rule of the 2026-10-12 fix.
- A failing-then-passing test: a resource with a query and a `%20` / `%2F` path reaches the request line byte for byte;
  a path with a raw space is escaped.

## References

- `src/Network/HTTPRequest.cpp` (`toString()`), `src/Network/URI.cpp` (`resource()`).
