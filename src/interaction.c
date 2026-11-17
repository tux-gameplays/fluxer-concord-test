#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "discord.h"
#include "discord-internal.h"
#include "discord-request.h"

CCORDcode
discord_create_interaction_response(
    struct discord *client,
    u64snowflake interaction_id,
    const char interaction_token[],
    struct discord_interaction_response *params,
    struct discord_ret_interaction_response *ret)
{
    (void)interaction_id;
    (void)interaction_token;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_original_interaction_response(
    struct discord *client,
    u64snowflake application_id,
    const char interaction_token[],
    struct discord_ret_interaction_response *ret)
{
    (void)application_id;
    (void)interaction_token;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_edit_original_interaction_response(
    struct discord *client,
    u64snowflake application_id,
    const char interaction_token[],
    struct discord_edit_original_interaction_response *params,
    struct discord_ret_interaction_response *ret)
{
    (void)application_id;
    (void)interaction_token;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_delete_original_interaction_response(struct discord *client,
                                             u64snowflake application_id,
                                             const char interaction_token[],
                                             struct discord_ret *ret)
{
    (void)application_id;
    (void)interaction_token;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_create_followup_message(struct discord *client,
                                u64snowflake application_id,
                                const char interaction_token[],
                                struct discord_create_followup_message *params,
                                struct discord_ret_webhook *ret)
{
    (void)application_id;
    (void)interaction_token;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_followup_message(struct discord *client,
                             u64snowflake application_id,
                             const char interaction_token[],
                             u64snowflake message_id,
                             struct discord_ret_message *ret)
{
    (void)application_id;
    (void)interaction_token;
    (void)message_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_edit_followup_message(struct discord *client,
                              u64snowflake application_id,
                              const char interaction_token[],
                              u64snowflake message_id,
                              struct discord_edit_followup_message *params,
                              struct discord_ret_message *ret)
{
    (void)application_id;
    (void)interaction_token;
    (void)message_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_delete_followup_message(struct discord *client,
                                u64snowflake application_id,
                                const char interaction_token[],
                                u64snowflake message_id,
                                struct discord_ret *ret)
{
    (void)application_id;
    (void)interaction_token;
    (void)message_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}
