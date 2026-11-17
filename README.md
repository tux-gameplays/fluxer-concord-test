[discord-shield]: https://img.shields.io/discord/928763123362578552?color=5865F2&logo=discord&logoColor=white
[discord-invite]: https://discord.gg/Y7Xa6MA82v
[discord-config-init]: https://cogmasters.github.io/concord/group__DiscordClient.html#ga75bbe1d3eb9e6d03953b6313e5543afb
[discord-config-get-field]: https://cogmasters.github.io/concord/group__DiscordClient.html#gac4486003ffab83de397c0bebb74b3307
[coglink-link]: https://github.com/PerformanC/CogLink

<div align="center">
<img src="https://raw.githubusercontent.com/Cogmasters/concord/bd1436a84af21384d93d92aed32b4c7828d0d793/docs/static/logo.svg" width="250" alt="Concord Logo">
</div>

# Concord - C Fluxer API library

[ ![discord-shield][] ][discord-invite]

## About

Concord is an asynchronous C99 client library adapted for **Fluxer** (HTTP API `v1` and Gateway `v1`) with minimal external dependencies. It preserves Concord's familiar `discord_*` C symbol names (`struct discord`, `discord_from_token()`, `discord_create_message()`, etc.) while discovering Fluxer service endpoints dynamically from `GET /.well-known/fluxer`.

- **[Fluxer Client Guide](docs/FLUXER.md)** — Official & self-hosted discovery (`/.well-known/fluxer`), `discord_set_instance_origin()`, Gateway v1 behavior, and `examples/fluxer-ping.c`.
- **[Fluxer Compatibility Inventory](docs/FLUXER_COMPATIBILITY.md)** — Complete per-function status matrix (`supported`, `adapted`, or `unsupported` returning `CCORD_UNSUPPORTED`).

| Contents                                   | Description                                  |
|-------------------------------------------|----------------------------------------------|
| [Fluxer Guide](docs/FLUXER.md)            | Fluxer discovery, self-hosting, & Gateway v1 |
| [Compatibility](docs/FLUXER_COMPATIBILITY.md) | Per-function Fluxer support matrix       |
| [Build Instructions](#build-instructions) | How to build libcurl and Concord             |
| [Environment Setup](#setting-up-your-environment) | Project setup and compilation        |
| [Configuration](#configuring-concord)     | Configure token and `instance_origin`        |
| [Example Bot](#test-fluxer-ping-bot)      | Test Concord with the `!ping` Fluxer bot     |
| [Build Options](#configure-your-build)    | Special flags and build targets              |
| [Installation](#installing-concord)       | Installing Concord on your system            |
| [Contributing](#contributing)             | How to contribute to Concord                 |

### Getting Started

- [Fluxer Setup & Protocol Guide](docs/FLUXER.md)
- [Fluxer Compatibility Matrix](docs/FLUXER_COMPATIBILITY.md)
- [API Reference Documentation](https://cogmasters.github.io/concord/)

#### Configuration

- [Config.json Directives](docs/guides/config.json_directives.md) - Configure your bot using JSON
- [Setup with Environment Variables](docs/guides/setup_concord_with_env.md) - Use environment variables

#### Features & Protocol Notes

- **Service Discovery (`/.well-known/fluxer`):** Concord resolves `endpoints.api_public` (appending `/v1`), `endpoints.gateway`, and `endpoints.media` from `GET {instance_origin}/.well-known/fluxer`. By default `discord_from_token()` uses `https://api.fluxer.app`; self-hosted Fluxer origins can be configured via `discord_from_config()`, `"instance_origin"` in `config.json`, or `discord_set_instance_origin()` before `discord_run()`.
- **No Gateway Intents:** Fluxer Gateway v1 does not use Discord intent bitmasks; `MESSAGE_CREATE` delivers message `content` without intents. `discord_add_intents()` and `discord_remove_intents()` remain as source-compatible local helpers and never transmit intents on `IDENTIFY`.
- **Unsupported APIs (`CCORD_UNSUPPORTED`):** Fluxer currently has no slash/application-command or interaction protocol. Calling functions in `include/application_command.h`, `include/interaction.h`, or other unsupported operations immediately returns `CCORD_UNSUPPORTED` (and invokes `ret->fail` if provided) with zero network traffic instead of emulating slash commands as prefix commands.
- [Embeds](docs/guides/embeds.md) - Create rich message embeds
- [Event Scheduler](docs/guides/scheduler_event.md) - Control Gateway event processing
- [SQLite3 with Concord](docs/guides/sqlite3_db_with_concord.md) - Persist data with SQLite
- [Databases with Concord](docs/guides/databases_with_concord.md) - General database integration

#### Debugging & Troubleshooting

- [Debugging](docs/guides/debugging.md) - Debug your Concord applications

### Other Guides

Following are other standalone guides created by the community to help you get started with Concord, organized by category:

- [Compiling on Windows](docs/guides/compiling_on_windows.md) - Build Concord on Windows systems
- [Installing Concord (Termux)](docs/guides/installing_concord_termux.md) - Install on Android using Termux
- [Msys2 and Mingw64](docs/guides/msys2_and_mingw64.md) - Setup using MSYS2 environment
- [Cross Compiling](docs/guides/cross_compiling.md) - Compile for different target platforms
- [Concord on Old Systems](docs/guides/concord_on_old_systems.md) - Support for legacy systems

### Examples

*Refer to [`examples/fluxer-ping.c`](examples/fluxer-ping.c) and [`docs/FLUXER.md`](docs/FLUXER.md) for a complete Fluxer walkthrough.*

#### Prefix Command Bot (`!ping` -> `pong`)

```c
#include <stdlib.h>
#include <concord/discord.h>
#include <concord/logmod.h>

static void on_ready(struct discord *client, const struct discord_ready *event) {
    (void)client;
    logmod_log(INFO, NULL, "Connected to Fluxer as %s!", event->user->username);
}

static void on_ping(struct discord *client, const struct discord_message *event) {
    if (!event || event->webhook_id != 0) return;
    if (!event->author || event->author->bot) return;

    struct discord_create_message params = { .content = "pong" };
    discord_create_message(client, event->channel_id, &params, NULL);
}

int main(void) {
    struct discord_config config = {
        .token = getenv("FLUXER_TOKEN"),
        .instance_origin = getenv("FLUXER_INSTANCE_ORIGIN"), /* NULL -> https://api.fluxer.app */
    };
    struct discord *client = discord_from_config(&config);
    if (!client) return EXIT_FAILURE;

    discord_set_prefix(client, "!");
    discord_set_on_ready(client, &on_ready);
    discord_set_on_command(client, "ping", &on_ping);
    discord_run(client);
    discord_cleanup(client);
    return EXIT_SUCCESS;
}
```

## Supported operating systems (minimum requirements)

* GNU/Linux 4.x
* FreeBSD 12
* NetBSD 8.1
* Windows 7 (Cygwin)
* GNU/Hurd 0.9
* Mac OS X 10.9

*Note: big-endian processors running certain OSes like SPARC Solaris, PowerPC AIX, System Z z/OS or Linux, or MIPS IRIX are NOT supported. There are currently a few issues that prevent some of the logic from correctly on big-endian systems. This will be fixed soon.*

## Build Instructions

The only dependency is `curl-8.7.1` or higher with websockets support. Since many system package managers don't provide this specific version with websockets enabled, we recommend compiling from source.

> [!IMPORTANT]
> Libcurl must be compiled with the `--enable-websockets` flag to work with Concord.

### On Windows

* Install **Cygwin**
* During Cygwin installation, select packages: `gcc`, `make`, `git`, `autoconf`, `libtool`, `pkg-config`, `openssl-devel`, `zlib-devel`
* Follow the Windows tutorial [here](docs/guides/compiling_on_windows.md)
* Then compile libcurl from source (instructions below)

### Compiling libcurl from source (recommended for all platforms)

```console
# Install build dependencies
# Ubuntu/Debian:
$ sudo apt-get update && sudo apt-get install -y build-essential autoconf libtool pkg-config libssl-dev zlib1g-dev

# Fedora/RHEL/CentOS:
$ sudo dnf install -y gcc make autoconf libtool pkgconfig openssl-devel zlib-devel

# Arch/Manjaro:
$ sudo pacman -S --needed base-devel openssl zlib

# Alpine:
$ apk add build-base autoconf libtool pkgconfig openssl-dev zlib-dev

# FreeBSD:
$ pkg install autoconf libtool pkgconf openssl

# OS X:
$ xcode-select --install
$ brew install autoconf libtool pkg-config openssl@3
# or with MacPorts:
$ port install autoconf libtool pkgconfig openssl

# Download and compile libcurl
$ curl -LO https://curl.se/download/curl-8.7.1.tar.gz
$ tar -xzf curl-8.7.1.tar.gz
$ cd curl-8.7.1
$ ./configure --with-openssl --enable-websockets
$ make
$ sudo make install

# You might need to inform the system's dynamic linker about the new library location with:
$ echo "/usr/local/lib" > /etc/ld.so.conf.d/curl.conf
$ sudo ldconfig
```

## Setting up your environment

### Clone Concord into your workspace

```console
$ git clone https://github.com/cogmasters/concord.git && cd concord
```

### Compile Concord

```console
$ make
```

#### Optional: speed up compilation

You can speed up the compilation process by using `-j` flag with `make`:
```console
$ make -j$(nproc)
```
This will use all available CPU cores to compile Concord. You can also specify a number instead of `$(nproc)` to limit the number of jobs.

### Special notes about Voice Connections

Concord does not support voice connections yet. If you want to use voice connections, you can use [CogLink][coglink-link] instead.
CogLink is a separate project that provides a LavaLink client for Concord. It is not part of the Concord library, but it is designed to work seamlessly with Concord.

### Special notes for non-Linux systems

You might run into trouble with the compiler and linker not finding your Libcurl headers. You can do something like this:
```console
$ CFLAGS=-I<some_path> LDFLAGS=-L<some_path> make
```
For instance, on a FreeBSD system:
```console
$ CFLAGS=-I/usr/local/include LDFLAGS=-L/usr/local/lib make
```
On OS X using MacPorts:
```console
$ CFLAGS=-I/opt/local/include LDFLAGS=-L/opt/local/lib make
```
On OS X using a self-compiled libcurl:
```console
$ CFLAGS=-I/usr/local/include LDFLAGS=-L/usr/local/include make
```
On Windows with Cygwin, you might need to pass both arguments to use POSIX threading:
```console
$ CFLAGS="-pthread -lpthread" make
```

### Special note about linking Concord against another library

In some cases, you might want to link Concord into a shared object, or link it as a shared object into another shared
object. In that case, you will need to compile Concord with `CFLAGS="-fpic" make`. 

## Configuring Concord

[discord\_config\_init()][discord-config-init] is the initialization method that allows configuring your bot without recompiling.

The following outlines `config.json` fields:
```js
{
  "token": "YOUR-FLUXER-BOT-TOKEN", // never commit real bot tokens
  "instance_origin": "https://api.fluxer.app", // optional; override for self-hosted Fluxer
  "log": {                   // logging directives
    "level": "TRACE",           // TRACE, DEBUG, INFO, WARN, ERROR, FATAL
    "trace": "bot.log",         // the log output file (null to disable)
    "quiet": false,             // true to disable logs in console
    "overwrite": true,          // true overwrites the file on each run
    "color": true,              // display color on console
    "http": "http.log",         // the HTTP log output file (null to disable)
    "ws": "ws.log",             // the WebSockets log output file (null to disable)
    "disable": ["WEBSOCKETS", "HTTP"] // disable logging for specific modules
  }
  // here you can add your custom fields *
}
```
\* *Your custom field contents can be fetched with [discord\_config\_get\_field()][discord-config-get-field]. You can also reconfigure the Fluxer origin before `discord_run()` with `discord_set_instance_origin(client, origin)`.*

See more information in [`docs/FLUXER.md`](docs/FLUXER.md) and the [config.json directives](docs/guides/config.json_directives.md) guide.

## Test Fluxer-Ping Bot

1. Create a bot application on Fluxer (or your self-hosted Fluxer instance) and export its token via `FLUXER_TOKEN`.
2. Build Concord and example executables:
   ```console
   $ make && make -C examples fluxer-ping
   ```
3. Run `fluxer-ping` (set `FLUXER_INSTANCE_ORIGIN` when targeting a self-hosted instance):
   ```console
   $ FLUXER_TOKEN="your-bot-token" ./examples/fluxer-ping
   ```

### Get Fluxer-Ping Response

Send `!ping` from a human account in any channel visible to the bot; the bot ignores bot/webhook messages and replies `pong`.

### Terminate Fluxer-Ping Bot

With <kbd>Ctrl</kbd>+<kbd>c</kbd> or with <kbd>Ctrl</kbd>+<kbd>|</kbd>

## Configure your build

The following outlines special flags and targets to override the default Makefile build with additional functionalities.

### Special compilation flags

* `-DCCORD_SIGINTCATCH`
    * By default Concord will not shutdown gracefully when a SIGINT is received (i.e. <kbd>Ctrl</kbd>+<kbd>c</kbd>), enable this flag if you wish it to be handled for you.
* `-DCCORD_DEBUG_WEBSOCKETS`
    * Enable verbose debugging for WebSockets communication.
* `-DCCORD_DEBUG_HTTP`
    * Enable verbose debugging for HTTP communication.

*Example:*
```console
$ CFLAGS="-DCCORD_SIGINTCATCH -DCCORD_DEBUG_HTTP" make
```

### Special targets

* `make shared`
    * Produce a dynamically-linked version of Concord. This Makefile is intended for GNU-style compilers, such as `gcc` or `clang`.
* `make shared_osx`
    * Produce a dynamically-linked version of Concord, for OS X and Darwin systems. 
* `make debug`
    * Enable some flags useful while developing, such as `-O0` and `-g`

## Installing Concord

*(note -- `#` means that you should be running as root)*

```console
# make install
```

This will install the headers and library files into $PREFIX. You can override this as such:
```console
# PREFIX=/opt/concord make install
```

### Cross-compiling Concord
To cross-compile Concord, see the manual [here](docs/guides/cross_compiling.md).

### Included dependencies

The following are `stable` and well documented dependencies that are packaged with Concord and can be included to your projects:

| File                                                  | Description                                        |
|-------------------------------------------------------|----------------------------------------------------|
| [cog-utils](https://github.com/Cogmasters/cog-utils)  | General purpose functions aimed at portability     |
| [logmod](https://github.com/lcsmuller/logmod)         | A modular logging library                          |
| [carray](https://github.com/c-ware/carray)\*          | Macro-based implementation of type-safe arrays     |
| [anomap](https://github.com/Anotra/anomap)\*          | Sorted key/value storage for C99                   |
| [oa_hash](https://github.com/lcsmuller/oa-hash)       | A lightweight open-addressing hashtable            |
| [json-build](https://github.com/lcsmuller/json-build) | Tiny, zero-allocation JSON serializer              |
| [jsmn-find](https://github.com/lcsmuller/jsmn-find)   | Tiny, zero-allocation JSON tokenizer               |

\* *Concord uses its own modified version that may be not up to date with the original*

Note that included headers must be `concord/` prefixed:
```c
#include <concord/discord.h>
#include <concord/logmod.h>
```

### Standalone executable

#### GCC 

```console
$ gcc myBot.c -o myBot -pthread -ldiscord -lcurl
```

#### Clang

```console
$ clang myBot.c -o myBot -pthread -ldiscord -lcurl
```

#### UNIX C compilers
##### This includes the following compilers:

* IBM XL C/C++ (AIX, z/OS, IBM i)
* Sun/Oracle Studio (Solaris)
* IRIX MIPSpro C++ (IRIX) -- NOTE: currently not supported
* HP aCC (HP-UX)
* Compaq C (Tru64 UNIX) -- NOTE: also currently not supported
*Note: if you want to actually compile this on one of the systems listed above, please see the "Compiling on old computers" guide.*

```console
$ cc myBot.c -o myBot -ldiscord -lcurl -lpthread
```

Note: some systems such as **Cygwin** require you to do this:
```console
$ gcc myBot.c -o myBot -pthread -lpthread -ldiscord -lcurl
```
(this links against libpthread.a in `/usr/lib`)

## Support

Problems? Check out our [Discord Server][discord-invite]

## Contributing

All kinds of contributions are welcome, all we ask is to abide to our [guidelines](docs/CONTRIBUTING.md)! If you want to help but is unsure where to get started then our [Discord API Roadmap](docs/DISCORD_ROADMAP.md) is a good starting point. Check the following guides for more information on how to contribute:

- [Contributing](docs/CONTRIBUTING.md) - How to contribute to Concord
- [Project Outline](docs/PROJECT_OUTLINE.md) - Overview of the project structure
- [Coding Guidelines](docs/CODING_GUIDELINES.md) - Coding style and best practices
- [Discord API Roadmap](docs/DISCORD_ROADMAP.md) - Overview of Discord API features and their status
- [Concord Internals](docs/INTERNALS.md) - Understand how Concord works internally
