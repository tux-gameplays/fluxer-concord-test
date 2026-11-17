# Using Concord with Fluxer

This version of Concord targets the **Fluxer** HTTP v1 API and Gateway v1 protocol while retaining Concord's existing C99 public symbol names (`struct discord`, `discord_from_token()`, `discord_create_message()`, etc.) for source compatibility.

> [!IMPORTANT]
> Concord connects exclusively to Fluxer instances (not Discord). Operations that have no documented Fluxer bot equivalent fail immediately with `CCORD_UNSUPPORTED` rather than issuing guessed HTTP requests or no-op successes.

## Service Discovery (`/.well-known/fluxer`)

Concord never hard-codes or guesses REST or WebSocket service URLs from a hostname. During initialization, the client fetches the unauthenticated discovery document:

```http
GET {instance_origin}/.well-known/fluxer
```

From the JSON response, Concord extracts and validates:

- `endpoints.api_public` — appended with `/v1` and used for all bot REST requests (`Authorization: Bot <token>`).
- `endpoints.gateway` — used for WebSocket Gateway v1 connections (`?v=1&encoding=json`), paired with `GET /v1/gateway/bot` session metadata.
- `endpoints.media` — retained for media endpoints advertised by the instance.

If the discovery document is unreachable, returns a non-`200` HTTP status, omits required endpoint fields, or advertises unsupported URL schemes, client initialization fails visibly without attempting downstream requests.

## Official and Self-Hosted Instance Configuration

By default, `discord_from_token()` uses Fluxer's official public instance origin (`https://api.fluxer.app`). Self-hosted Fluxer instances (including local `http://` / `ws://` test deployments) can be configured in three ways:

### 1. `struct discord_config` (`discord_from_config`)

Recommended when reading `FLUXER_TOKEN` and `FLUXER_INSTANCE_ORIGIN` from environment variables at startup so initial discovery and `/v1/users/@me` validation target your self-hosted instance directly:

```c
struct discord_config config = {
    .token = getenv("FLUXER_TOKEN"),
    .instance_origin = getenv("FLUXER_INSTANCE_ORIGIN"), /* NULL uses https://api.fluxer.app */
};
struct discord *client = discord_from_config(&config);
```

### 2. JSON configuration (`discord_from_json`)

Set `"instance_origin"` alongside `"token"` in `config.json` (never commit real bot tokens):

```json
{
  "token": "${FLUXER_TOKEN}",
  "instance_origin": "https://fluxer.example.org",
  "log": {
    "level": "INFO",
    "color": true
  }
}
```

### 3. Runtime origin setter (`discord_set_instance_origin`)

Before starting a Gateway session with `discord_run()`, an initialized client can switch to another Fluxer origin and re-run `GET /.well-known/fluxer` discovery:

```c
CCORDcode code = discord_set_instance_origin(client, "https://fluxer.example.org");
if (code != CCORD_OK) {
    fprintf(stderr, "Discovery failed: %s\n", discord_strerror(code, client));
}
```

`discord_set_instance_origin()` returns `CCORD_BAD_PARAMETER` if `origin` is not a bare `http://` or `https://` origin or if a Gateway connection/session is already active. Selected origins and discovered URLs are preserved across `discord_clone()`.

## Gateway v1 Protocol & Absence of Intents

- **No Gateway Intents:** Fluxer Gateway v1 does not use Discord intent bitmasks, and Concord omits `"intents"` from `IDENTIFY` frames. `discord_add_intents()` and `discord_remove_intents()` remain available as source-compatible local helpers so existing C code compiles without modification, and `MESSAGE_CREATE` includes message `content` without a `MESSAGE_CONTENT` intent.
- **Prefix Commands (`!ping`):** Prefix commands configured with `discord_set_prefix()` and `discord_set_on_command()` / `discord_set_on_commands()` work directly over Fluxer `MESSAGE_CREATE` dispatches.
- **Decimal-String Snowflakes:** Fluxer encodes 64-bit snowflakes (epoch `1420070400000`) as decimal strings, including values above `INT64_MAX`, which Concord encodes and decodes losslessly as `u64snowflake` (`uint64_t`).
- **Gateway Client Opcodes:** Concord maps `discord_update_presence()` (opcode `3`), `discord_update_voice_state()` (opcode `4`, sending `"channel_id": null` on disconnect), and `discord_request_guild_members()` (opcode `8`, receiving `GUILD_MEMBERS_CHUNK`).

## Unsupported Operations & `CCORD_UNSUPPORTED`

Fluxer currently does not expose a Discord-compatible slash/application-command or interaction protocol, nor certain Discord-specific resources/events (such as Auto-Moderation rules, Stage Instances, Guild Scheduled Events, or OAuth2 Bearer-only endpoints when authenticating as a bot).

- Calling any unsupported REST function (including all functions in `include/application_command.h` and `include/interaction.h`) immediately returns **`CCORD_UNSUPPORTED` (`-15`)** without opening a network connection.
- If a failure callback (`ret->fail`) is supplied, Concord invokes it synchronously with `response->code == CCORD_UNSUPPORTED`, preserving `ret->data` and `ret->keep`.
- Do **not** expect slash commands to be emulated as prefix commands; use `discord_set_prefix()` + `discord_set_on_command()` or `discord_set_on_message_create()` explicitly.
- Both `ccord_code_as_string(CCORD_UNSUPPORTED)` / `discord_code_as_string(CCORD_UNSUPPORTED)` and `ccord_strerror(CCORD_UNSUPPORTED)` / `discord_strerror(CCORD_UNSUPPORTED, client)` describe this status.

For the complete per-function support matrix (`supported`, `adapted`, or `unsupported`), see [`docs/FLUXER_COMPATIBILITY.md`](FLUXER_COMPATIBILITY.md).

## Running the `!ping` Example Bot

Build Concord and the examples:

```console
$ make
$ make -C examples fluxer-ping
```

Run against the official Fluxer instance:

```console
$ FLUXER_TOKEN="your-bot-token" ./examples/fluxer-ping
```

Run against a self-hosted Fluxer instance:

```console
$ FLUXER_TOKEN="your-bot-token" \
  FLUXER_INSTANCE_ORIGIN="https://fluxer.example.org" \
  ./examples/fluxer-ping
```

Send `!ping` from a human account in any channel visible to the bot; `fluxer-ping` ignores bot and webhook messages and replies `pong`.
