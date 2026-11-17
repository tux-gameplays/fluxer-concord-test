#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "discord.h"
#include "discord-internal.h"
#include "discord-request.h"

CCORDcode
discord_get_global_application_commands(
    struct discord *client,
    u64snowflake application_id,
    struct discord_ret_application_commands *ret)
{
    (void)application_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_create_global_application_command(
    struct discord *client,
    u64snowflake application_id,
    struct discord_create_global_application_command *params,
    struct discord_ret_application_command *ret)
{
    (void)application_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_global_application_command(
    struct discord *client,
    u64snowflake application_id,
    u64snowflake command_id,
    struct discord_ret_application_command *ret)
{
    (void)application_id;
    (void)command_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_edit_global_application_command(
    struct discord *client,
    u64snowflake application_id,
    u64snowflake command_id,
    struct discord_edit_global_application_command *params,
    struct discord_ret_application_command *ret)
{
    (void)application_id;
    (void)command_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_delete_global_application_command(struct discord *client,
                                          u64snowflake application_id,
                                          u64snowflake command_id,
                                          struct discord_ret *ret)
{
    (void)application_id;
    (void)command_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_bulk_overwrite_global_application_commands(
    struct discord *client,
    u64snowflake application_id,
    struct discord_application_commands *params,
    struct discord_ret_application_commands *ret)
{
    (void)application_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_guild_application_commands(
    struct discord *client,
    u64snowflake application_id,
    u64snowflake guild_id,
    struct discord_ret_application_commands *ret)
{
    (void)application_id;
    (void)guild_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_create_guild_application_command(
    struct discord *client,
    u64snowflake application_id,
    u64snowflake guild_id,
    struct discord_create_guild_application_command *params,
    struct discord_ret_application_command *ret)
{
    (void)application_id;
    (void)guild_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_guild_application_command(
    struct discord *client,
    u64snowflake application_id,
    u64snowflake guild_id,
    u64snowflake command_id,
    struct discord_ret_application_command *ret)
{
    (void)application_id;
    (void)guild_id;
    (void)command_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_edit_guild_application_command(
    struct discord *client,
    u64snowflake application_id,
    u64snowflake guild_id,
    u64snowflake command_id,
    struct discord_edit_guild_application_command *params,
    struct discord_ret_application_command *ret)
{
    (void)application_id;
    (void)guild_id;
    (void)command_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_delete_guild_application_command(struct discord *client,
                                         u64snowflake application_id,
                                         u64snowflake guild_id,
                                         u64snowflake command_id,
                                         struct discord_ret *ret)
{
    (void)application_id;
    (void)guild_id;
    (void)command_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_bulk_overwrite_guild_application_commands(
    struct discord *client,
    u64snowflake application_id,
    u64snowflake guild_id,
    struct discord_bulk_overwrite_guild_application_commands *params,
    struct discord_ret_application_commands *ret)
{
    (void)application_id;
    (void)guild_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_guild_application_command_permissions(
    struct discord *client,
    u64snowflake application_id,
    u64snowflake guild_id,
    struct discord_ret_guild_application_command_permissions *ret)
{
    (void)application_id;
    (void)guild_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_application_command_permissions(
    struct discord *client,
    u64snowflake application_id,
    u64snowflake guild_id,
    u64snowflake command_id,
    struct discord_ret_application_command_permission *ret)
{
    (void)application_id;
    (void)guild_id;
    (void)command_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}
