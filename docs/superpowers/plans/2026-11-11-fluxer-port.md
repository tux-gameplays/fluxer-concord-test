# Concord Fluxer Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Concord's existing C client API operate against Fluxer, covering every current public operation that Fluxer supports and returning `CCORD_UNSUPPORTED` for operations without an equivalent.

**Architecture:** Keep Concord's public C names and callback model, but replace Discord-specific runtime service discovery, REST and Gateway behavior with Fluxer's documented discovery-driven HTTP API and v1 Gateway protocol. Add an explicit compatibility catalog linking every public Concord operation to a Fluxer implementation or an unsupported result; implement resource groups only after that mapping is verified.

**Tech Stack:** C99; Concord's existing libcurl/WebSocket/JSON layers, Makefiles and Greatest tests; Fluxer discovery, HTTP API/OpenAPI and Gateway docs.

**Spec:** `docs/superpowers/specs/2026-11-11-concord-fluxer-design.md`

## Global Constraints

- Runtime target is Fluxer, not a Discord/Fluxer dual-mode client.
- Preserve Discord-named public C interfaces where practical.
- Discover `endpoints.api_public` and `endpoints.gateway` from `GET /.well-known/fluxer`; do not construct endpoint URLs by guessing.
- Self-hosted support targets instances implementing the documented Fluxer discovery and protocol.
- Discovery errors fail visibly; never silently fall back to guessed endpoints.
- Unsupported features return the dedicated `CCORD_UNSUPPORTED` code through existing completion/error contracts.
- Slash/application commands and interactions remain unsupported unless Fluxer documents an equivalent protocol; do not emulate them as prefix commands.
- Do not copy Fluxer server implementation code into Concord.
- Run all build and test commands inside the project container: `docker run --rm -v "$PWD:/workspace" -w /workspace coelho-bot ...`; the host toolchain is unusable here.
- `make -C test <target>` builds a test binary but does not run it; execute `/workspace/test/<target>` in the same container command to verify assertions.
- After rebuilding `libdiscord.a`, run `make -C test clean` before rebuilding a focused test target because test binaries do not declare the library as a Make dependency.

## Review Focus

- **Discovery returns missing, malformed, or unsupported-scheme endpoints:** initialization fails clearly; allow HTTP/WS for local self-hosted instances and test invalid schemes in Task 2.
- **Self-hosted discovery advertises non-default API/Gateway hosts and API prefixes:** requests use exactly advertised URLs; test in Task 2 with a local fixture origin.
- **Fluxer snowflakes exceed signed integer range:** IDs round-trip as decimal strings without truncation; test in Task 3 with values above `INT64_MAX`.
- **Gateway sends a close, reconnect, invalid-session or resume sequence:** connection state recovers or fails according to Fluxer behavior; test in Task 4 using scripted WebSocket fixtures.
- **A public Concord operation has no Fluxer counterpart:** caller receives `CCORD_UNSUPPORTED`, not `CCORD_OK` or a generic HTTP failure; test in Tasks 1 and 9, plus each resource task's unsupported operations.

---

### Task 1: Add the unsupported-capability error and complete compatibility catalog

**Files:**
- Modify: `core/concord-error.h`, `core/concord-error.c`
- Create: `docs/FLUXER_COMPATIBILITY.md`
- Create: `test/fluxer_compatibility.c`
- Create: `test/check_fluxer_compatibility.sh`
- Modify: `test/Makefile`
- Test: `test/fluxer_compatibility.c`

**Interfaces:**
- Produces: `#define CCORD_UNSUPPORTED -15` (use the next free core error value), with names/messages returned by both `ccord_code_as_string()` and `ccord_strerror()`.
- Produces: a catalog entry for every public operation declared in public headers, classified `supported`, `adapted`, `unsupported`, or temporarily `pending` with its header/resource. `pending` is allowed only while Tasks 2-9 establish the actual mapping; the final Task 10 check rejects every remaining `pending` entry.

- [x] **Step 1: Write failing error-code tests** asserting the dedicated code has stable name/message strings and is distinct from `CCORD_HTTP_CODE`, `CCORD_BAD_DECODE`, and `CCORD_RESOURCE_UNAVAILABLE`.
- [x] **Step 2: Build and run `fluxer_compatibility`; verify it fails because code `-15` currently maps to the unknown-code text.**
- [x] **Step 3: Add `CCORD_UNSUPPORTED` to `core/concord-error.h` and both string conversion switches in `core/concord-error.c`.**
- [x] **Step 4: Add `test/check_fluxer_compatibility.sh` to extract public function declarations from public headers (excluding `discord-internal.h`, `discord-request.h`, `discord-response.h` and `discord-worker.h`) and verify every operation appears exactly once in `docs/FLUXER_COMPATIBILITY.md`; populate the initial catalog from Concord headers.** Mark application-command and interaction operations unsupported; mark unmapped resource operations pending until their resource task validates them.
- [x] **Step 5: Run `fluxer_compatibility` and `test/check_fluxer_compatibility.sh`; expect PASS, zero missing/duplicate entries and no pending command/interaction operations.**
- [x] **Step 6: Commit as `feat: add Fluxer compatibility catalog and unsupported error`.**

### Task 2: Add Fluxer instance configuration and discovery

**Files:**
- Modify: `.gitignore`, `test/.gitignore`, `include/discord.h`, `include/discord-internal.h`, `src/discord-client.c`, `src/discord-rest_request.c`
- Create: `src/fluxer-discovery.c`, `include/fluxer-discovery.h`
- Modify: `src/Makefile`, `test/Makefile`
- Create: `test/fluxer_discovery.c`, `test/fixtures/fluxer-discovery.json`, `test/fixtures/fluxer-discovery-invalid.json`

**Interfaces:**
- Produces: `struct discord_config.instance_origin`; `discord_from_token()` defaults to `https://api.fluxer.app`, while `discord_from_config()` and `discord_from_json()` accept an explicit origin. The origin is copied into client-owned memory before initialization.
- Produces: discovery parser that resolves and stores the exact `endpoints.api_public`, `endpoints.gateway`, and `endpoints.media` values plus the API `/v1` base.

- [x] **Step 1: Add tests** for parsing a valid discovery document with distinct API, Gateway and media hosts; assert exact advertised URLs are retained.
- [x] **Step 2: Add tests** for missing fields, malformed JSON, invalid schemes and failed discovery HTTP responses; assert initialization fails clearly and does not enqueue a guessed endpoint request.
- [x] **Step 3: Run `make -C test fluxer_discovery`; verify expected missing-interface failures.**
- [x] **Step 4: Add the `instance_origin` field to `struct discord_config`, parse it from JSON, and copy it when constructing the client through `discord_from_config()`.** Keep it client-owned and preserved for clones; reject non-origin values before discovery.
- [x] **Step 5: Implement discovery request `GET {origin}/.well-known/fluxer` with no Authorization header and validate required endpoint fields.** Use `https://api.fluxer.app` as the official default origin confirmed by Fluxer's own production OpenAPI configuration; do not hard-code downstream API or Gateway hosts.
- [x] **Step 6: Run `make -C test fluxer_discovery`; expect PASS, including the Review Focus discovery and self-host endpoint tests.**
- [ ] **Step 7: Commit as `feat: discover Fluxer instance endpoints`.**

### Task 3: Port REST transport and core message/user operations

**Files:**
- Modify: `src/discord-rest_request.c`, `src/discord-rest.c`, `src/discord-rest_ratelimit.c`, `src/channel.c`, `src/user.c`, `src/gateway.c`
- Modify as needed: `gencodecs/api/channel.PRE.h`, `gencodecs/api/user.PRE.h`, `gencodecs/discord_codecs.PRE.h`
- Modify: `test/rest.c`, `test/Makefile`
- Create: `test/fluxer_rest.c`, `test/fixtures/fluxer-message.json`, `test/fixtures/fluxer-user.json`

**Interfaces:**
- Consumes: Task 2's discovered `api_public` base and instance origin.
- Produces: resource calls using the Fluxer `/v1` routes and `Authorization: Bot <token>` unless Fluxer docs require a different scheme; request execution must preserve current callback/sync APIs.

- [ ] **Step 1: Add a local HTTP fixture test** for a Fluxer message create request; assert discovered host, `/v1/channels/{channel_id}/messages` path, Bot authorization, JSON request body, and decoded response.
- [ ] **Step 2: Add decoder tests** for Fluxer message/user fixtures, including a snowflake greater than `INT64_MAX`, webhook author data and fields absent from Discord-shaped objects.
- [ ] **Step 3: Run `make -C test fluxer_rest`; verify failures against Discord hard-coded base/path/decoders.**
- [ ] **Step 4: Replace fixed `DISCORD_API_BASE_URL` concatenation with the discovered Fluxer API base while retaining the existing per-operation callback and error flow.**
- [ ] **Step 5: Port channel message read/create/edit/delete and all operations in `src/user.c` to exact Fluxer routes/payloads; adapt snowflake JSON encode/decode to decimal strings and map error/rate-limit responses.**
- [ ] **Step 6: Update generated codec source templates and regenerate codecs; do not hand-edit generated outputs when a PRE template is the source.**
- [ ] **Step 7: Run `make -C test fluxer_rest` and `make`; expect PASS.** Include Review Focus large snowflake test.
- [ ] **Step 8: Commit as `feat: port Fluxer REST message and user APIs`.**

### Task 4: Port Fluxer Gateway protocol and event loop

**Files:**
- Modify: `src/discord-gateway.c`, `src/discord-gateway_dispatch.c`, `src/discord-events.c`, `src/gateway.c`, `src/discord-cache.c`
- Modify: `include/discord.h`, `include/discord-events.h`, `include/gateway.h`
- Modify: `src/Makefile`, `test/Makefile`
- Create: `test/fluxer_gateway.c`, `test/fixtures/fluxer-gateway-hello.json`, `test/fixtures/fluxer-gateway-ready.json`

**Interfaces:**
- Consumes: Task 2's discovered Gateway URL and Task 3's Fluxer snowflake/codec rules.
- Produces: Gateway client session using Fluxer `?v=1&encoding=json`, Fluxer Identify (`token`, `properties`), heartbeat sequence, resume, and Fluxer event names/payloads.
- Public Discord intent functions remain source-compatible; they must not send Discord intent bits to Fluxer. Document intent compatibility behavior and map event subscriptions only to actual Fluxer filtering facilities.

- [ ] **Step 1: Add scripted WebSocket tests** for Hello, heartbeat ACK, Identify, Ready, Dispatch, close/reconnect, invalid session, resume and sequence handling using Fluxer's documented opcodes and fields.
- [ ] **Step 2: Run `make -C test fluxer_gateway`; verify tests fail on Discord `v=10`, intents and gateway payload assumptions.**
- [ ] **Step 3: Adapt handshake and Identify to Fluxer Gateway v1; connect to the exact discovered URL with required query parameters and Fluxer client properties.**
- [ ] **Step 4: Adapt event dispatch and cache initialization to Fluxer event/payload differences; remove Discord intent transmission and preserve callbacks only where semantics match.**
- [ ] **Step 5: Adapt heartbeat/resume/reconnect and close-code handling to Fluxer's documented session rules; surface unrecoverable states through Concord's existing error callbacks.**
- [ ] **Step 6: Run `make -C test fluxer_gateway` and `make`; expect PASS, including Review Focus reconnect sequence test.**
- [ ] **Step 7: Commit as `feat: connect Concord to Fluxer Gateway`.**

### Task 5: Port guild, membership, role and permission operations

**Files:**
- Modify: `src/guild.c`, relevant declarations in `include/guild.h`
- Modify as needed: generated PRE templates under `gencodecs/api/`
- Modify: `docs/FLUXER_COMPATIBILITY.md`, `test/Makefile`
- Create: `test/fluxer_guild.c`

**Interfaces:**
- Consumes: Task 1's operation catalog and Task 3's REST transport.
- Produces: each public guild/member/role/permission operation uses a verified Fluxer route and codec or returns `CCORD_UNSUPPORTED` before dispatch.

- [ ] **Step 1: Add fixture tests for each supported guild/member/role operation and assertions that unsupported entries return `CCORD_UNSUPPORTED` without dispatch.**
- [ ] **Step 2: Run `make -C test fluxer_guild`; verify the new assertions fail against unported routes/behavior.**
- [ ] **Step 3: Port verified Fluxer guild/member/role/permission paths and codecs; update the catalog for each operation.**
- [ ] **Step 4: Run `make -C test fluxer_guild` and catalog coverage check; expect PASS.**
- [ ] **Step 5: Commit as `feat: port Fluxer guild and permission APIs`.**

### Task 6: Port moderation and audit-log operations

**Files:**
- Modify: `src/audit_log.c`, `src/auto_moderation.c`, `include/audit_log.h`, `include/auto_moderation.h`
- Modify as needed: generated PRE templates under `gencodecs/api/`
- Modify: `docs/FLUXER_COMPATIBILITY.md`, `test/Makefile`
- Create: `test/fluxer_moderation.c`

**Interfaces:**
- Consumes: Task 1 catalog and Task 3 REST transport.
- Produces: moderation/audit operations use verified Fluxer semantics or return `CCORD_UNSUPPORTED` before dispatch.

- [ ] **Step 1: Add fixture and unsupported-path tests for each public audit-log and auto-moderation operation.**
- [ ] **Step 2: Run `make -C test fluxer_moderation`; verify the new tests fail before implementation.**
- [ ] **Step 3: Port routes/fields supported by Fluxer and update catalog statuses; mark absent equivalents unsupported.**
- [ ] **Step 4: Run `make -C test fluxer_moderation` and catalog coverage check; expect PASS.**
- [ ] **Step 5: Commit as `feat: map Fluxer moderation and audit APIs`.**

### Task 7: Port emoji, sticker, invite and guild-template operations

**Files:**
- Modify: `src/emoji.c`, `src/sticker.c`, `src/invite.c`, `src/guild_template.c` and their public headers
- Modify as needed: generated PRE templates under `gencodecs/api/`
- Modify: `docs/FLUXER_COMPATIBILITY.md`, `test/Makefile`
- Create: `test/fluxer_assets.c`

**Interfaces:**
- Consumes: Task 1 catalog and Task 3 REST transport.
- Produces: asset/invite/template operations use verified Fluxer semantics or return `CCORD_UNSUPPORTED` before dispatch.

- [ ] **Step 1: Add fixture and unsupported-path tests for each public operation in these modules.**
- [ ] **Step 2: Run `make -C test fluxer_assets`; verify the new tests fail before implementation.**
- [ ] **Step 3: Port supported routes and codec fields; update catalog statuses for every operation.**
- [ ] **Step 4: Run `make -C test fluxer_assets` and catalog coverage check; expect PASS.**
- [ ] **Step 5: Commit as `feat: map Fluxer asset and invite APIs`.**

### Task 8: Port webhook, thread, voice, stage and scheduled-event operations

**Files:**
- Modify: `src/webhook.c`, `src/channel.c`, `src/voice.c`, `src/stage_instance.c`, `src/guild_scheduled_event.c` and their public headers
- Modify as needed: generated PRE templates under `gencodecs/api/`
- Modify: `docs/FLUXER_COMPATIBILITY.md`, `test/Makefile`
- Create: `test/fluxer_advanced_resources.c`

**Interfaces:**
- Consumes: Tasks 1-4 compatibility catalog, REST transport, codecs and Gateway dispatch.
- Produces: each operation uses documented Fluxer HTTP/Gateway behavior or returns `CCORD_UNSUPPORTED` before side effects.

- [ ] **Step 1: Add fixture tests for each supported operation and explicit unsupported-result tests for operations without an equivalent.**
- [ ] **Step 2: Run `make -C test fluxer_advanced_resources`; verify the new tests fail before implementation.**
- [ ] **Step 3: Port supported HTTP/Gateway behavior and update catalog entries; ensure unsupported APIs create no network traffic.**
- [ ] **Step 4: Run `make -C test fluxer_advanced_resources` and catalog coverage check; expect PASS.**
- [ ] **Step 5: Commit as `feat: map Fluxer advanced resources`.**

### Task 9: Mark application commands, interactions and OAuth operations accurately

**Files:**
- Modify: `src/application_command.c`, `src/interaction.c`, `src/oauth2.c`, `include/application_command.h`, `include/interaction.h`, `include/oauth2.h`
- Modify: `docs/FLUXER_COMPATIBILITY.md`, `test/Makefile`
- Create: `test/fluxer_unsupported.c`

**Interfaces:**
- Consumes: Task 1's `CCORD_UNSUPPORTED` and catalog.
- Produces: slash/application commands and interactions return `CCORD_UNSUPPORTED` unless Fluxer documents a compatible equivalent; OAuth functions are individually mapped or explicitly unsupported.

- [ ] **Step 1: Add tests asserting each unavailable slash/interaction call returns `CCORD_UNSUPPORTED` (or invokes the existing failure callback with it) and issues no request.**
- [ ] **Step 2: Run `make -C test fluxer_unsupported`; verify it fails because Discord endpoints are still attempted.**
- [ ] **Step 3: Replace unavailable operations with explicit unsupported handling and map only documented Fluxer OAuth equivalents.**
- [ ] **Step 4: Run `make -C test fluxer_unsupported` and catalog coverage check; expect PASS.**
- [ ] **Step 5: Commit as `feat: report unsupported Fluxer command APIs`.**

### Task 10: Update examples, documentation and end-to-end smoke test

**Files:**
- Modify: `README.md`, `docs/PROJECT_OUTLINE.md`
- Create: `docs/FLUXER.md`, `examples/fluxer-ping.c`
- Modify: `examples/Makefile`, `config.json`
- Test: `test/fluxer_smoke.c` or documented manual smoke test if no Fluxer test instance is available in CI

**Interfaces:**
- Consumes: completed client config, message APIs, Gateway event callbacks and compatibility catalog.
- Produces: documented setup for official Fluxer and a self-hosted origin; `!ping` replies `pong` to human messages only; unsupported slash commands are called out explicitly.

- [ ] **Step 1: Add docs/example checks** asserting the example reads token from config/environment, sets a Fluxer origin only when needed, and does not claim slash-command support.
- [ ] **Step 2: Verify the check fails against current Discord-oriented docs/examples.**
- [ ] **Step 3: Update the README/config example and add `examples/fluxer-ping.c`; use the existing public naming conventions and mention intents are not a Fluxer Gateway feature.**
- [ ] **Step 4: Run `make`, `make -C test`, and `make -C examples`; expect all PASS.**
- [ ] **Step 5: Run `test/check_fluxer_compatibility.sh --complete`; expect every public function mapped or explicitly unsupported and no `pending` rows. Then run the bot against the official Fluxer instance and a self-hosted instance where credentials/instances are available; verify human `!ping` gets `pong`, bot/webhook `!ping` is ignored, and the compatibility docs match actual behavior.**
- [ ] **Step 6: Commit as `docs: document Concord Fluxer client support`.**
