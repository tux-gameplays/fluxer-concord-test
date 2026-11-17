#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "discord.h"
#include "discord-internal.h"
#include "discord-request.h"

CCORDcode
discord_list_auto_moderation_rules_for_guild(
    struct discord *client,
    u64snowflake guild_id,
    struct discord_ret_auto_moderation_rules *ret)
{
    (void)guild_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_auto_moderation_rule(struct discord *client,
                                 u64snowflake guild_id,
                                 u64snowflake auto_moderation_rule_id,
                                 struct discord_ret_auto_moderation_rule *ret)
{
    (void)guild_id;
    (void)auto_moderation_rule_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_create_auto_moderation_rule(
    struct discord *client,
    u64snowflake guild_id,
    struct discord_create_auto_moderation_rule *params,
    struct discord_ret_auto_moderation_rule *ret)
{
    (void)guild_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_modify_auto_moderation_rule(
    struct discord *client,
    u64snowflake guild_id,
    u64snowflake auto_moderation_rule_id,
    struct discord_modify_auto_moderation_rule *params,
    struct discord_ret_auto_moderation_rule *ret)
{
    (void)guild_id;
    (void)auto_moderation_rule_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_delete_auto_moderation_rule(
    struct discord *client,
    u64snowflake guild_id,
    u64snowflake auto_moderation_rule_id,
    struct discord_delete_auto_moderation_rule *params,
    struct discord_ret *ret)
{
    (void)guild_id;
    (void)auto_moderation_rule_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}
