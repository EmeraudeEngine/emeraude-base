# Network::HTTPServer, and the client's cleartext path to private addresses (2026-10-04)

## `Network::HTTPServer` (`src/Network/HTTPServer.hpp`)

A small HTTP/1.1 server on its own network thread (asio). It was the engine MCP server's own code; the owner chose
(2026-10-04) to extract it here when engine resource sharing needed the same server: one audited parser instead of
two. Users: the engine's `Console::MCP::Server` and `Resources::SharingServer`.

- **Life**: `HTTPServer{options}`, `start(onRequest, onStreamShutdown, onClosed)` binds and starts the thread
  (`port = 0` → an ephemeral port, read back with `port()`); `stop()` gives each stream its last words
  (`onStreamShutdown` → `writeNowAndClose()`), closes every socket ON the network thread, joins; bounded to 3 s.
- **Threading**: every socket operation and every handler on the network thread. Work done elsewhere answers
  through `post()` (thread-safe; the caller keeps the server alive while it may call it). Hold a connection as a
  `std::weak_ptr` to answer later.
- **One answer per request**: `respond()` (a body; HEAD gets the head), `respondEmpty()`, `respondFile()` (streamed
  in 256 KiB chunks, never loaded whole; a single `Range` → 206 / 416, `If-Range` → the whole file, since no
  validator is emitted; a stalled reader is cut after `requestTimeoutSeconds` per chunk), or `startStream()` (no
  length, a keep-alive payload every N seconds — the MCP SSE streams).
- **Checks before the handler runs**: a non-loopback binding refuses to START without a bearer token; a loopback
  binding accepts only its own names in `Host` (DNS rebinding); a present `Origin` must be this server's; the bearer
  is compared in constant time. **Framing**: duplicated `Content-Length` / `Host` / `Authorization` / `Origin` /
  `Transfer-Encoding` / `Range` → 400 (request smuggling, RFC 9112 § 6.3), `Transfer-Encoding` → 501, a POST
  without a length → 411, a length over `maxBodyBytes` → 413, a head over `maxHeaderBytes` → 431, too many
  connections → 503 at accept. Idle and request timeouts (slowloris).
- `parseByteRange()` is public and tested on its own (RFC 9110 § 14.1.2: several ranges, another unit or a
  malformed value are IGNORED — the whole representation is served).

## Cleartext HTTP in the client (`HTTPSClientOptions::allowPrivateCleartext`)

`HTTPSClient` refuses `http://`. For an engine peer on the LAN, where no trusted certificate exists, the owner chose
(2026-10-04) a cleartext path limited to private addresses:

- Off by default. With it, an `http://` URI is spoken without TLS through `TLSConnection::connectCleartextPrivate()`,
  which refuses unless EVERY address the host resolves to is private (`isPrivateNetworkAddress()`: 127/8, 10/8,
  172.16/12, 192.168/16, 169.254/16, ::1, fe80::/10, fc00::/7, an IPv4-mapped address by its IPv4 part). Every
  address, not one: `async_connect` tries them in order, and a mixed DNS answer would carry the request to a
  public one. Never through a proxy.
- `https://` is unaffected; a redirect from `https://` to `http://` stays refused; the caller's headers are dropped
  on a redirect to another origin, and the scheme is now part of the origin (`sameOrigin()`).
- Nothing is encrypted: a bearer token sent this way is readable on the network.

## Two client additions on the way

- **`Host` carries a non-default port** (RFC 9110 § 7.2). It was the bare host: a loopback server checking its own
  name (this one) refused `127.0.0.1` for `127.0.0.1:<port>`. Proof: `NetworkHTTPServer.CleartextDownloadFromAPrivatePeer`.
- **`HTTPRequestOptions::cancel`** — an `std::atomic< bool >` another thread raises: the exchange stops at the next
  transport read, `DownloadOutcome::Cancelled`, a partial file removed. And a `download()` overload with
  `HTTPRequestOptions` (headers, cancel; a body is refused: `BadRequest`).

## Tests (`src/Testing/test_NetworkHTTPServer.cpp`, 14)

Ranges, loopback answer, foreign Host / Origin, bearer, non-loopback without token, smuggling and framing, oversized
head, pipelined keep-alive, files with ranges / HEAD / 404, an answer posted from another thread, stream last words
at shutdown, the connection bound, the private-address classifier (every range edge), a cleartext download from a
private peer (refused without the opt-in, 401 without the token, byte-exact with it, a public address refused).
