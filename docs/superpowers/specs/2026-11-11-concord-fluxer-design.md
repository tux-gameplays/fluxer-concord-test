# Concord as a Fluxer C library

## Intent

Adapt Concord so C applications written against its public Discord-named API can connect to and operate on Fluxer, especially Fluxer's original public instance. The existing names (for example `struct discord` and `discord_from_token`) may remain where preserving source compatibility is useful; the runtime target is Fluxer, not a Discord/Fluxer dual-mode client.

The first acceptance example is the existing C bot listening for `!ping` and replying `pong` on Fluxer. This is only a smoke test; the end goal is coverage of the resources and operations Concord currently exposes wherever Fluxer has an equivalent.

## Goals

- Make the existing Concord C API use Fluxer's public HTTP API and Gateway.
- Keep existing public C names and call patterns where practical, avoiding a needless source-level rename.
- Discover service URLs from Fluxer's `GET /.well-known/fluxer` document and use `endpoints.api_public` and `endpoints.gateway` rather than constructing or hard-coding service URLs.
- Support the official Fluxer instance and self-hosted Fluxer instances through a configurable instance origin.
- Adapt HTTP routes, authorization, request/response codecs, Gateway handshake, events, reconnection and rate-limit behavior to Fluxer's documented protocol.
- Maintain an explicit capability/compatibility inventory mapping every currently exposed Concord feature to a Fluxer equivalent, an adaptation, or an unsupported status.
- Clearly report unsupported operations to callers and document them. In particular, Discord slash/application commands and interactions are unavailable when Fluxer has no equivalent; do not silently emulate them as prefix commands.
- Expose unsupported operations through a dedicated `CCORD_UNSUPPORTED` error code, propagated through the existing synchronous/asynchronous completion contracts as applicable, so callers can distinguish protocol capability gaps from HTTP/network failures.
- Test protocol behavior using fixtures and, where feasible, a bot against a running Fluxer instance.

## Non-goals

- Preserve runtime support for Discord as a second backend.
- Add a Discord-compatible shim to Fluxer itself.
- Copy Fluxer server implementation code into Concord; implement a client using Fluxer's public protocol and documentation.
- Claim parity for features absent from Fluxer or hide unsupported behavior behind successful no-op responses.

## Findings from initial source reconnaissance

- Concord currently centralizes its REST base at `DISCORD_API_BASE_URL` (`include/discord.h`), sends `Authorization: Bot <token>` (`src/discord-rest_request.c`), and requests `/gateway/bot` before building a Discord Gateway URL with Discord's API version and query string (`src/gateway.c`, `src/discord-gateway.c`).
- Fluxer's docs specify an unauthenticated `GET /.well-known/fluxer` discovery document. Third-party clients use `endpoints.api_public` followed by `/v1` and connect to `endpoints.gateway`; the URLs should not be inferred from the instance origin.
- Fluxer's bot Gateway endpoint is `GET /v1/gateway/bot`. The Gateway uses protocol version `v=1`, JSON payloads, raw bot token in Identify, opcodes/events, heartbeat and resumable sessions. Its documented Gateway has no Discord intents; event filtering uses Fluxer-specific mechanisms such as `ignored_events` and lazy requests.
- Fluxer message objects retain familiar fields such as `content`, `author`, `webhook_id`, channel and message IDs, but IDs are decimal strings and message types/structures must be checked against Concord codecs rather than assumed identical.
- Fluxer documents `/.well-known/fluxer` discovery and bot-facing HTTP routes under `/v1`. Its docs identify slash commands as unavailable for this target, so Concord's command/interaction surface needs an explicit unsupported result and documentation.
- The Concord public API spans many generated REST resources and Gateway event types, so the work must be staged by protocol layers and use the inventory as the source of truth instead of claiming full parity after the ping example.

## Proposed implementation shape (builder calls)

1. Add Fluxer instance-origin configuration while preserving a sensible official-instance default. Resolve API, Gateway and other service endpoints from Fluxer discovery; make self-host origin override possible in the existing configuration path and/or an explicit setter without embedding private deployment assumptions. If discovery is unavailable or malformed, fail with a clear initialization error; do not guess endpoint URLs.
2. Separate service discovery and transport details from resource operations so endpoint construction and authentication can be changed centrally.
3. Port REST calls in resource groups. For each route, verify the documented Fluxer path, bot authorization support, payload shape, response shape, permissions and error/rate-limit semantics. Update generated codecs or add focused conversion where the protocols differ.
4. Adapt the Gateway state machine to Fluxer's version, Identify payload, event filtering, heartbeat/sequence/resume rules and close behavior. Preserve callbacks where event semantics align; mark unsupported intents or event semantics explicitly.
5. Publish a compatibility matrix generated or maintained alongside the implementation, listing supported, adapted and unsupported APIs. Slash commands/interactions remain unsupported unless Fluxer later exposes an equivalent protocol.
6. Update the bot example and docs to configure a Fluxer instance and test the `!ping` flow. Do not put tokens in source, images or committed config.

## Verification and acceptance

- Unit tests cover discovery parsing, base URL/path composition, auth header, decimal-string IDs, Gateway Identify/heartbeat/resume/error flows and the compatibility status of unsupported calls.
- Concord's normal build and tests pass.
- A smoke test connects to a Fluxer instance, receives a human-authored `!ping`, ignores bot/webhook messages as the example requires, and sends `pong`.
- The same configuration mechanism accepts a self-hosted Fluxer origin and uses the endpoints advertised by that instance.
- Each Concord public operation has an explicit compatibility entry; unsupported operations fail visibly rather than masquerading as success.

## Settled implementation assumptions

- Self-hosted support targets any Fluxer instance that implements the documented discovery document and protocol, rather than a selected deployment/version.
- An unavailable or malformed discovery document is a visible initialization failure; there is no fallback to guessed URLs.
- Operations without a Fluxer equivalent return the dedicated `CCORD_UNSUPPORTED` code through Concord's existing completion/error path.

## Open implementation decisions

- Use Concord's existing `discord_config`/JSON configuration path for the instance origin; constructor initialization performs REST work before returning, so a post-construction setter cannot reliably select a self-hosted instance.
- Exact operation mapping, codec conversions and unsupported list require a full inventory against the current Fluxer docs/OpenAPI and server behavior. These are implementation findings, not assumptions of parity.
- The official instance origin/default should be confirmed from Fluxer's current project configuration/documentation; discovery remains the source of all API and Gateway URLs.
