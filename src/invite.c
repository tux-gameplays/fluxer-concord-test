#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "discord.h"
#include "discord-internal.h"
#include "discord-request.h"
#include "cog-utils.h"

struct _discord_delete_invite_ctx {
    char *code;
    struct discord_ret_invite ret;
};

static void
_free_delete_invite_ctx(struct discord *client, void *data)
{
    struct _discord_delete_invite_ctx *cxt = data;
    (void)client;
    free(cxt->code);
    free(cxt);
}

static void
_done_delete_invite(struct discord *client,
                    struct discord_response *resp,
                    const struct discord_invite *invite)
{
    struct _discord_delete_invite_ctx *cxt = resp->data;
    struct discord_invite fallback = { 0 };
    const struct discord_invite *result = invite;

    if (!invite->code && cxt->code) {
        fallback = *invite;
        fallback.code = cxt->code;
        result = &fallback;
    }

    resp->data = cxt->ret.data;
    resp->keep = cxt->ret.keep;

    if (cxt->ret.done) cxt->ret.done(client, resp, result);

    if (cxt->ret.keep)
        discord_refcounter_decr(&client->refcounter, (void *)cxt->ret.keep);
    if (cxt->ret.data)
        discord_refcounter_decr(&client->refcounter, cxt->ret.data);
}

static void
_fail_delete_invite(struct discord *client, struct discord_response *resp)
{
    struct _discord_delete_invite_ctx *cxt = resp->data;

    resp->data = cxt->ret.data;
    resp->keep = cxt->ret.keep;

    if (cxt->ret.fail) cxt->ret.fail(client, resp);

    if (cxt->ret.keep)
        discord_refcounter_decr(&client->refcounter, (void *)cxt->ret.keep);
    if (cxt->ret.data)
        discord_refcounter_decr(&client->refcounter, cxt->ret.data);
}

CCORDcode
discord_get_invite(struct discord *client,
                   char *invite_code,
                   struct discord_get_invite *params,
                   struct discord_ret_invite *ret)
{
    struct discord_attributes attr = { 0 };
    (void)params;
    CCORD_EXPECT(client, NOT_EMPTY_STR(invite_code), CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_INIT(attr, discord_invite, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/invites/%s", invite_code);
}

CCORDcode
discord_delete_invite(struct discord *client,
                      char *invite_code,
                      struct discord_delete_invite *params,
                      struct discord_ret_invite *ret)
{
    struct discord_attributes attr = { 0 };
    char *reason = params ? params->reason : NULL;
    CCORD_EXPECT(client, NOT_EMPTY_STR(invite_code), CCORD_BAD_PARAMETER, "");

    if (!ret) {
        DISCORD_ATTR_INIT(attr, discord_invite, ret, reason);
        return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                                "/invites/%s", invite_code);
    }

    if (ret->sync) {
        struct discord_invite synced = { 0 };
        struct discord_ret_invite inner_ret = {
            .high_priority = ret->high_priority,
            .sync = (ret->sync == DISCORD_SYNC_FLAG) ? &synced : ret->sync,
        };
        struct discord_ret_invite *inner_ret_p = &inner_ret;
        DISCORD_ATTR_INIT(attr, discord_invite, inner_ret_p, reason);
        CCORDcode code = discord_rest_run(
            &client->rest, &attr, NULL, HTTP_DELETE, "/invites/%s",
            invite_code);
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
        if (!inner_ret.sync->code) {
            cog_strndup(invite_code, strlen(invite_code),
                        &inner_ret.sync->code);
        }
        if (ret->done) {
            struct discord_response resp = {
                .data = ret->data,
                .keep = ret->keep,
                .code = CCORD_OK,
            };
            ret->done(client, &resp, inner_ret.sync);
        }
        if (ret->sync == DISCORD_SYNC_FLAG) {
            discord_invite_cleanup(&synced);
        }
        return CCORD_OK;
    }

    struct _discord_delete_invite_ctx *cxt = calloc(1, sizeof *cxt);
    if (!cxt) return CCORD_OUT_OF_MEMORY;
    if (!cog_strndup(invite_code, strlen(invite_code), &cxt->code)) {
        free(cxt);
        return CCORD_OUT_OF_MEMORY;
    }
    cxt->ret = *ret;

    if (ret->keep) {
        CCORDcode code =
            discord_refcounter_incr(&client->refcounter, (void *)ret->keep);
        if (code < 0) {
            free(cxt->code);
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

    struct discord_ret_invite inner_ret = {
        .data = cxt,
        .cleanup = &_free_delete_invite_ctx,
        .high_priority = ret->high_priority,
        .done = &_done_delete_invite,
        .fail = &_fail_delete_invite,
    };
    struct discord_ret_invite *inner_ret_p = &inner_ret;
    DISCORD_ATTR_INIT(attr, discord_invite, inner_ret_p, reason);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/invites/%s", invite_code);
}
