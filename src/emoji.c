#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "discord.h"
#include "discord-internal.h"
#include "discord-request.h"

struct _discord_get_guild_emoji_ctx {
    u64snowflake emoji_id;
    struct discord_ret_emoji ret;
};

static void
_free_get_guild_emoji_ctx(struct discord *client, void *data)
{
    (void)client;
    free(data);
}

static void
_done_get_guild_emoji(struct discord *client,
                      struct discord_response *resp,
                      const struct discord_emojis *emojis)
{
    struct _discord_get_guild_emoji_ctx *cxt = resp->data;
    const struct discord_emoji *found = NULL;

    for (int i = 0; i < emojis->size; ++i) {
        if (emojis->array[i].id == cxt->emoji_id) {
            found = &emojis->array[i];
            break;
        }
    }

    resp->data = cxt->ret.data;
    resp->keep = cxt->ret.keep;

    if (found) {
        if (cxt->ret.done) cxt->ret.done(client, resp, found);
    }
    else {
        resp->code = CCORD_RESOURCE_UNAVAILABLE;
        if (cxt->ret.fail) cxt->ret.fail(client, resp);
    }

    if (cxt->ret.keep)
        discord_refcounter_decr(&client->refcounter, (void *)cxt->ret.keep);
    if (cxt->ret.data)
        discord_refcounter_decr(&client->refcounter, cxt->ret.data);
}

static void
_fail_get_guild_emoji(struct discord *client, struct discord_response *resp)
{
    struct _discord_get_guild_emoji_ctx *cxt = resp->data;

    resp->data = cxt->ret.data;
    resp->keep = cxt->ret.keep;

    if (cxt->ret.fail) cxt->ret.fail(client, resp);

    if (cxt->ret.keep)
        discord_refcounter_decr(&client->refcounter, (void *)cxt->ret.keep);
    if (cxt->ret.data)
        discord_refcounter_decr(&client->refcounter, cxt->ret.data);
}

CCORDcode
discord_list_guild_emojis(struct discord *client,
                          u64snowflake guild_id,
                          struct discord_ret_emojis *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_LIST_INIT(attr, discord_emojis, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/guilds/%" PRIu64 "/emojis", guild_id);
}

CCORDcode
discord_get_guild_emoji(struct discord *client,
                        u64snowflake guild_id,
                        u64snowflake emoji_id,
                        struct discord_ret_emoji *ret)
{
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, emoji_id != 0, CCORD_BAD_PARAMETER, "");

    if (ret && ret->sync) {
        struct discord_emojis emojis = { 0 };
        struct discord_ret_emojis list_ret = {
            .high_priority = ret->high_priority,
            .sync = &emojis,
        };
        CCORDcode code = discord_list_guild_emojis(client, guild_id, &list_ret);
        if (code != CCORD_OK) {
            if (ret->fail) {
                struct discord_response resp = {
                    .data = ret->data,
                    .keep = ret->keep,
                    .code = code,
                };
                ret->fail(client, &resp);
            }
            return code;
        }

        int found_idx = -1;
        for (int i = 0; i < emojis.size; ++i) {
            if (emojis.array[i].id == emoji_id) {
                found_idx = i;
                break;
            }
        }
        if (found_idx < 0) {
            discord_emojis_cleanup(&emojis);
            if (ret->fail) {
                struct discord_response resp = {
                    .data = ret->data,
                    .keep = ret->keep,
                    .code = CCORD_RESOURCE_UNAVAILABLE,
                };
                ret->fail(client, &resp);
            }
            return CCORD_RESOURCE_UNAVAILABLE;
        }

        struct discord_emoji matched = emojis.array[found_idx];
        memset(&emojis.array[found_idx], 0, sizeof(struct discord_emoji));
        discord_emojis_cleanup(&emojis);

        if (ret->sync != DISCORD_SYNC_FLAG) {
            *ret->sync = matched;
            if (ret->done) {
                struct discord_response resp = {
                    .data = ret->data,
                    .keep = ret->keep,
                    .code = CCORD_OK,
                };
                ret->done(client, &resp, ret->sync);
            }
        }
        else {
            if (ret->done) {
                struct discord_response resp = {
                    .data = ret->data,
                    .keep = ret->keep,
                    .code = CCORD_OK,
                };
                ret->done(client, &resp, &matched);
            }
            discord_emoji_cleanup(&matched);
        }
        return CCORD_OK;
    }

    if (!ret) {
        return discord_list_guild_emojis(client, guild_id, NULL);
    }

    struct _discord_get_guild_emoji_ctx *cxt = malloc(sizeof *cxt);
    if (!cxt) return CCORD_OUT_OF_MEMORY;
    *cxt = (struct _discord_get_guild_emoji_ctx){
        .emoji_id = emoji_id,
        .ret = *ret,
    };

    if (ret->keep) {
        CCORDcode code =
            discord_refcounter_incr(&client->refcounter, (void *)ret->keep);
        if (code < 0) {
            free(cxt);
            return code;
        }
    }
    if (ret->data
        && CCORD_RESOURCE_UNAVAILABLE
               == discord_refcounter_incr(&client->refcounter, ret->data))
    {
        discord_refcounter_add_client(&client->refcounter, ret->data,
                                      ret->cleanup, false);
    }

    struct discord_ret_emojis list_ret = {
        .data = cxt,
        .cleanup = &_free_get_guild_emoji_ctx,
        .high_priority = ret->high_priority,
        .done = &_done_get_guild_emoji,
        .fail = &_fail_get_guild_emoji,
    };
    return discord_list_guild_emojis(client, guild_id, &list_ret);
}

CCORDcode
discord_create_guild_emoji(struct discord *client,
                           u64snowflake guild_id,
                           struct discord_create_guild_emoji *params,
                           struct discord_ret_emoji *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, NOT_EMPTY_STR(params->name), CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, NOT_EMPTY_STR(params->image), CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_create_guild_emoji_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_emoji, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_POST,
                            "/guilds/%" PRIu64 "/emojis", guild_id);
}

CCORDcode
discord_modify_guild_emoji(struct discord *client,
                           u64snowflake guild_id,
                           u64snowflake emoji_id,
                           struct discord_modify_guild_emoji *params,
                           struct discord_ret_emoji *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, emoji_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, NOT_EMPTY_STR(params->name), CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_modify_guild_emoji_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_emoji, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_PATCH,
                            "/guilds/%" PRIu64 "/emojis/%" PRIu64, guild_id,
                            emoji_id);
}

CCORDcode
discord_delete_guild_emoji(struct discord *client,
                           u64snowflake guild_id,
                           u64snowflake emoji_id,
                           struct discord_delete_guild_emoji *params,
                           struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, emoji_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/guilds/%" PRIu64 "/emojis/%" PRIu64, guild_id,
                            emoji_id);
}
