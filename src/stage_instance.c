#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "discord.h"
#include "discord-internal.h"
#include "discord-request.h"

CCORDcode
discord_create_stage_instance(struct discord *client,
                              struct discord_create_stage_instance *params,
                              struct discord_ret_stage_instance *ret)
{
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_stage_instance(struct discord *client,
                           u64snowflake channel_id,
                           struct discord_ret_stage_instance *ret)
{
    (void)channel_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_modify_stage_instance(struct discord *client,
                              u64snowflake channel_id,
                              struct discord_modify_stage_instance *params,
                              struct discord_ret_stage_instance *ret)
{
    (void)channel_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_delete_stage_instance(struct discord *client,
                              u64snowflake channel_id,
                              struct discord_delete_stage_instance *params,
                              struct discord_ret *ret)
{
    (void)channel_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}
