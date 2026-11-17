#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "discord.h"
#include "discord-internal.h"
#include "discord-request.h"

CCORDcode
discord_get_guild_template(struct discord *client,
                           const char template_code[],
                           struct discord_ret_guild_template *ret)
{
    (void)template_code;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_create_guild_from_guild_template(
    struct discord *client,
    const char template_code[],
    struct discord_create_guild_from_guild_template *params,
    struct discord_ret_guild *ret)
{
    (void)template_code;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_guild_templates(struct discord *client,
                            u64snowflake guild_id,
                            struct discord_ret_guild_templates *ret)
{
    (void)guild_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_create_guild_template(struct discord *client,
                              u64snowflake guild_id,
                              struct discord_create_guild_template *params,
                              struct discord_ret_guild_template *ret)
{
    (void)guild_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_sync_guild_template(struct discord *client,
                            u64snowflake guild_id,
                            const char template_code[],
                            struct discord_ret_guild_template *ret)
{
    (void)guild_id;
    (void)template_code;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_modify_guild_template(struct discord *client,
                              u64snowflake guild_id,
                              const char template_code[],
                              struct discord_modify_guild_template *params,
                              struct discord_ret_guild_template *ret)
{
    (void)guild_id;
    (void)template_code;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_delete_guild_template(struct discord *client,
                              u64snowflake guild_id,
                              const char template_code[],
                              struct discord_ret_guild_template *ret)
{
    (void)guild_id;
    (void)template_code;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}
