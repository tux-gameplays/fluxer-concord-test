#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "discord.h"
#include "logmod.h"

static void
on_ready(struct discord *client, const struct discord_ready *event)
{
    (void)client;
    if (event && event->user && event->user->username) {
        logmod_log(INFO, NULL, "Connected to Fluxer as %s#%s",
                   event->user->username,
                   event->user->discriminator ? event->user->discriminator
                                              : "0000");
    }
}

static void
on_ping(struct discord *client, const struct discord_message *event)
{
    if (!event || event->webhook_id != 0) return;
    if (!event->author || event->author->bot) return;
    if (event->content && *event->content != '\0') return;

    struct discord_create_message params = { .content = "pong" };
    discord_create_message(client, event->channel_id, &params, NULL);
}

int
main(int argc, char *argv[])
{
    const char *token = getenv("FLUXER_TOKEN");
    const char *origin = getenv("FLUXER_INSTANCE_ORIGIN");
    struct discord *client = NULL;

    if (token && *token) {
        if (origin && *origin) {
            struct discord_config config = {
                .token = (char *)token,
                .instance_origin = (char *)origin,
            };
            client = discord_from_config(&config);
        }
        else {
            client = discord_from_token(token);
        }
    }
    else if (argc > 1 && argv[1] && *argv[1]) {
        client = discord_from_json(argv[1]);
        if (client && origin && *origin
            && discord_set_instance_origin(client, origin) != CCORD_OK)
        {
            fprintf(stderr,
                    "Failed to configure Fluxer instance origin: %s\n",
                    origin);
            discord_cleanup(client);
            return EXIT_FAILURE;
        }
    }
    else {
        fprintf(stderr,
                "Usage: FLUXER_TOKEN=<bot-token> "
                "[FLUXER_INSTANCE_ORIGIN=https://example.org] %s [config.json]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    if (!client) {
        fprintf(stderr, "Failed to initialize Fluxer client.\n");
        return EXIT_FAILURE;
    }

    discord_set_prefix(client, "!");
    discord_set_on_ready(client, &on_ready);
    discord_set_on_command(client, "ping", &on_ping);

    CCORDcode code = discord_run(client);
    discord_cleanup(client);

    return code == CCORD_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
