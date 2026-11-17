#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "discord.h"
#include "discord-internal.h"
#include "discord-request.h"

CCORDcode
discord_list_guild_scheduled_events(
    struct discord *client,
    u64snowflake guild_id,
    struct discord_list_guild_scheduled_events *params,
    struct discord_ret_guild_scheduled_events *ret)
{
    (void)guild_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_create_guild_scheduled_event(
    struct discord *client,
    u64snowflake guild_id,
    struct discord_create_guild_scheduled_event *params,
    struct discord_ret_guild_scheduled_event *ret)
{
    (void)guild_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_guild_scheduled_event(
    struct discord *client,
    u64snowflake guild_id,
    u64snowflake guild_scheduled_event_id,
    struct discord_get_guild_scheduled_event *params,
    struct discord_ret_guild_scheduled_event *ret)
{
    (void)guild_id;
    (void)guild_scheduled_event_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_modify_guild_scheduled_event(
    struct discord *client,
    u64snowflake guild_id,
    u64snowflake guild_scheduled_event_id,
    struct discord_modify_guild_scheduled_event *params,
    struct discord_ret_guild_scheduled_event *ret)
{
    (void)guild_id;
    (void)guild_scheduled_event_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_delete_guild_scheduled_event(struct discord *client,
                                     u64snowflake guild_id,
                                     u64snowflake guild_scheduled_event_id,
                                     struct discord_ret *ret)
{
    (void)guild_id;
    (void)guild_scheduled_event_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_guild_scheduled_event_users(
    struct discord *client,
    u64snowflake guild_id,
    u64snowflake guild_scheduled_event_id,
    struct discord_get_guild_scheduled_event_users *params,
    struct discord_ret_guild_scheduled_event_users *ret)
{
    (void)guild_id;
    (void)guild_scheduled_event_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}
