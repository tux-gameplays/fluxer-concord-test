#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "discord.h"
#include "discord-internal.h"
#include "discord-request.h"

struct _discord_get_guild_sticker_ctx {
    u64snowflake guild_id;
    u64snowflake sticker_id;
    struct discord_ret_sticker ret;
};

static void
_free_get_guild_sticker_ctx(struct discord *client, void *data)
{
    (void)client;
    free(data);
}

static void
_done_get_guild_sticker(struct discord *client,
                        struct discord_response *resp,
                        const struct discord_stickers *stickers)
{
    struct _discord_get_guild_sticker_ctx *cxt = resp->data;
    struct discord_sticker *found = NULL;

    for (int i = 0; i < stickers->size; ++i) {
        if (stickers->array[i].id == cxt->sticker_id) {
            found = &stickers->array[i];
            if (!found->guild_id) found->guild_id = cxt->guild_id;
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
_fail_get_guild_sticker(struct discord *client, struct discord_response *resp)
{
    struct _discord_get_guild_sticker_ctx *cxt = resp->data;

    resp->data = cxt->ret.data;
    resp->keep = cxt->ret.keep;

    if (cxt->ret.fail) cxt->ret.fail(client, resp);

    if (cxt->ret.keep)
        discord_refcounter_decr(&client->refcounter, (void *)cxt->ret.keep);
    if (cxt->ret.data)
        discord_refcounter_decr(&client->refcounter, cxt->ret.data);
}

/******************************************************************************
 * REST functions
 ******************************************************************************/

CCORDcode
discord_get_sticker(struct discord *client,
                    u64snowflake sticker_id,
                    struct discord_ret_sticker *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, sticker_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_INIT(attr, discord_sticker, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/stickers/%" PRIu64 "/metadata", sticker_id);
}

CCORDcode
discord_list_nitro_sticker_packs(
    struct discord *client, struct discord_ret_list_nitro_sticker_packs *ret)
{
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                     ret ? ret->data : NULL,
                                     ret ? ret->keep : NULL);
}

CCORDcode
discord_list_guild_stickers(struct discord *client,
                            u64snowflake guild_id,
                            struct discord_ret_stickers *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_LIST_INIT(attr, discord_stickers, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/guilds/%" PRIu64 "/stickers", guild_id);
}

CCORDcode
discord_get_guild_sticker(struct discord *client,
                          u64snowflake guild_id,
                          u64snowflake sticker_id,
                          struct discord_ret_sticker *ret)
{
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, sticker_id != 0, CCORD_BAD_PARAMETER, "");

    if (ret && ret->sync) {
        struct discord_stickers stickers = { 0 };
        struct discord_ret_stickers list_ret = {
            .high_priority = ret->high_priority,
            .sync = &stickers,
        };
        CCORDcode code =
            discord_list_guild_stickers(client, guild_id, &list_ret);
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
        for (int i = 0; i < stickers.size; ++i) {
            if (stickers.array[i].id == sticker_id) {
                found_idx = i;
                break;
            }
        }
        if (found_idx < 0) {
            discord_stickers_cleanup(&stickers);
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

        struct discord_sticker matched = stickers.array[found_idx];
        if (!matched.guild_id) matched.guild_id = guild_id;
        memset(&stickers.array[found_idx], 0, sizeof(struct discord_sticker));
        discord_stickers_cleanup(&stickers);

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
            discord_sticker_cleanup(&matched);
        }
        return CCORD_OK;
    }

    if (!ret) {
        return discord_list_guild_stickers(client, guild_id, NULL);
    }

    struct _discord_get_guild_sticker_ctx *cxt = malloc(sizeof *cxt);
    if (!cxt) return CCORD_OUT_OF_MEMORY;
    *cxt = (struct _discord_get_guild_sticker_ctx){
        .guild_id = guild_id,
        .sticker_id = sticker_id,
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

    struct discord_ret_stickers list_ret = {
        .data = cxt,
        .cleanup = &_free_get_guild_sticker_ctx,
        .high_priority = ret->high_priority,
        .done = &_done_get_guild_sticker,
        .fail = &_fail_get_guild_sticker,
    };
    return discord_list_guild_stickers(client, guild_id, &list_ret);
}

CCORDcode
discord_modify_guild_sticker(struct discord *client,
                             u64snowflake guild_id,
                             u64snowflake sticker_id,
                             struct discord_modify_guild_sticker *params,
                             struct discord_ret_sticker *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, sticker_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_modify_guild_sticker_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_sticker, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_PATCH,
                            "/guilds/%" PRIu64 "/stickers/%" PRIu64, guild_id,
                            sticker_id);
}

CCORDcode
discord_delete_guild_sticker(struct discord *client,
                             u64snowflake guild_id,
                             u64snowflake sticker_id,
                             struct discord_delete_guild_sticker *params,
                             struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, sticker_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/guilds/%" PRIu64 "/stickers/%" PRIu64, guild_id,
                            sticker_id);
}
