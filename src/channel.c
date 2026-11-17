#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "discord.h"
#include "discord-internal.h"
#include "discord-request.h"

/******************************************************************************
 * Internal helpers
 ******************************************************************************/

static PRINTF_LIKE(5, 6) bool
_discord_query_append(char query[],
                      size_t size,
                      size_t *offset,
                      const char key[],
                      const char fmt[],
                      ...)
{
    char value[128];
    va_list args;
    int val_len, written;

    va_start(args, fmt);
    val_len = vsnprintf(value, sizeof(value), fmt, args);
    va_end(args);
    if (val_len < 0 || (size_t)val_len >= sizeof(value)) return false;

    written = snprintf(query + *offset, size - *offset, "%c%s=%s",
                       *offset == 0 ? '?' : '&', key, value);
    if (written < 0 || (size_t)written >= size - *offset) return false;

    *offset += (size_t)written;
    return true;
}

static bool
_discord_unix_ms_to_iso8601(char buf[], size_t size, u64unix_ms ms)
{
    time_t sec = (time_t)(ms / 1000);
    unsigned millis = (unsigned)(ms % 1000);
    struct tm tm = { 0 };
    int written;

    if (!gmtime_r(&sec, &tm)) return false;
    written = snprintf(buf, size, "%04d-%02d-%02dT%02d:%02d:%02d.%03uZ",
                       tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                       tm.tm_hour, tm.tm_min, tm.tm_sec, millis);
    return written > 0 && (size_t)written < size;
}

static CCORDcode
_discord_format_reaction_emoji(struct discord *client,
                               char buf[],
                               size_t size,
                               u64snowflake emoji_id,
                               const char emoji_name[])
{
    char *pct_emoji_name = NULL;
    int written;

    CCORD_EXPECT(client, emoji_id != 0 || NOT_EMPTY_STR(emoji_name),
                 CCORD_BAD_PARAMETER, "");
    if (NOT_EMPTY_STR(emoji_name)) {
        pct_emoji_name = curl_escape(emoji_name, (int)strlen(emoji_name));
        CCORD_EXPECT(client, pct_emoji_name != NULL, CCORD_OUT_OF_MEMORY, "");
    }
    if (emoji_id) {
        written = snprintf(buf, size, "%s:%" PRIu64,
                           pct_emoji_name ? pct_emoji_name : "_", emoji_id);
    }
    else {
        written = snprintf(buf, size, "%s", pct_emoji_name);
    }
    if (pct_emoji_name) curl_free(pct_emoji_name);

    CCORD_EXPECT(client, written > 0 && (size_t)written < size,
                 CCORD_MALFORMED_PAYLOAD, "Emoji identifier is too long");
    return CCORD_OK;
}

static size_t
_discord_pinned_messages_from_json(const char buf[], size_t size, void *p_self)
{
    struct discord_messages *self = p_self;
    jsmnf_loader loader;
    jsmnf_table *table = NULL;
    size_t table_len = 0;
    long ret = 0;

    if (!buf || !size || !self) return 0;
    jsmnf_init(&loader);
    if (jsmnf_load_auto(&loader, buf, size, &table, &table_len) <= 0
        || !loader.root || !loader.root->v)
    {
        free(table);
        return 0;
    }

    if (JSMN_ARRAY == loader.root->v->type) {
        ret = discord_messages_from_jsmnf(loader.root, buf, self);
    }
    else if (JSMN_OBJECT == loader.root->v->type) {
        const jsmnf_pair *items = jsmnf_find(loader.root, "items", 5);
        if (items && items->v && JSMN_ARRAY == items->v->type
            && items->length > 0)
        {
            ret = (long)(sizeof *self * items->length);
            for (unsigned i = 0; i < items->length; ++i) {
                const jsmnf_pair *entry = items->fields + i;
                const jsmnf_pair *msg_field =
                    (entry->v && JSMN_OBJECT == entry->v->type)
                        ? jsmnf_find(entry, "message", 7)
                        : NULL;
                const jsmnf_pair *target =
                    (msg_field && msg_field->v
                     && JSMN_OBJECT == msg_field->v->type)
                        ? msg_field
                        : entry;
                struct discord_message msg = { 0 };
                long sub = discord_message_from_jsmnf(target, buf, &msg);
                if (sub >= 0) {
                    ret += sub;
                    carray_append(self, msg);
                }
                else {
                    discord_message_cleanup(&msg);
                }
            }
        }
    }

    free(table);
    return ret > 0 ? (size_t)ret : 0;
}

static size_t
_discord_reaction_users_from_json(const char buf[], size_t size, void *p_self)
{
    struct discord_users *self = p_self;
    jsmnf_loader loader;
    jsmnf_table *table = NULL;
    size_t table_len = 0;
    long ret = 0;

    if (!buf || !size || !self) return 0;
    jsmnf_init(&loader);
    if (jsmnf_load_auto(&loader, buf, size, &table, &table_len) <= 0
        || !loader.root || !loader.root->v)
    {
        free(table);
        return 0;
    }

    if (JSMN_ARRAY == loader.root->v->type) {
        ret = discord_users_from_jsmnf(loader.root, buf, self);
    }
    else if (JSMN_OBJECT == loader.root->v->type) {
        const jsmnf_pair *items = jsmnf_find(loader.root, "items", 5);
        if (items && items->v && JSMN_ARRAY == items->v->type) {
            ret = discord_users_from_jsmnf(items, buf, self);
        }
    }

    free(table);
    return ret > 0 ? (size_t)ret : 0;
}

/******************************************************************************
 * Custom functions
 ******************************************************************************/

struct _discord_get_channel_at_pos {
    enum discord_channel_types type;
    int position;
    struct discord_ret_channel ret;
};

static void
_done_get_channels(struct discord *client,
                   struct discord_response *resp,
                   const struct discord_channels *chs)
{
    struct _discord_get_channel_at_pos *cxt = resp->data;
    const struct discord_channel *found_ch = NULL;

    for (int i = 0, pos = 0; i < chs->size; ++i) {
        if (cxt->type == chs->array[i].type && pos++ == cxt->position) {
            found_ch = &chs->array[i];
            break;
        }
    }

    resp->data = cxt->ret.data;
    resp->keep = cxt->ret.keep;

    if (found_ch) {
        if (cxt->ret.done) cxt->ret.done(client, resp, found_ch);
    }
    else if (cxt->ret.fail) {
        resp->code = CCORD_BAD_PARAMETER;
        cxt->ret.fail(client, resp);
    }

    if (cxt->ret.keep)
        discord_refcounter_decr(&client->refcounter, (void *)cxt->ret.keep);
    if (cxt->ret.data)
        discord_refcounter_decr(&client->refcounter, cxt->ret.data);
}

static void
_fail_get_channels(struct discord *client, struct discord_response *resp)
{
    struct _discord_get_channel_at_pos *cxt = resp->data;

    resp->data = cxt->ret.data;
    resp->keep = cxt->ret.keep;

    if (cxt->ret.fail) cxt->ret.fail(client, resp);

    if (cxt->ret.keep)
        discord_refcounter_decr(&client->refcounter, (void *)cxt->ret.keep);
    if (cxt->ret.data)
        discord_refcounter_decr(&client->refcounter, cxt->ret.data);
}

static void
_cleanup_get_channels(struct discord *client, void *data)
{
    (void)client;
    free(data);
}

CCORDcode
discord_get_channel_at_pos(struct discord *client,
                           u64snowflake guild_id,
                           enum discord_channel_types type,
                           int position,
                           struct discord_ret_channel *ret)
{
    struct _discord_get_channel_at_pos *cxt;
    struct discord_ret_channels channels_ret = { 0 };

    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, position >= 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, ret != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, ret->done != NULL || ret->sync != NULL,
                 CCORD_BAD_PARAMETER, "");

    if (ret->sync) {
        struct discord_channels chs = { 0 };
        struct discord_ret_channels sync_ret = { .sync = &chs };
        int matched = -1;
        CCORDcode code =
            discord_get_guild_channels(client, guild_id, &sync_ret);
        if (code != CCORD_OK) {
            discord_channels_cleanup(&chs);
            return code;
        }
        for (int i = 0, pos = 0; i < chs.size; ++i) {
            if (type == chs.array[i].type && pos++ == position) {
                matched = i;
                break;
            }
        }
        if (matched < 0) {
            discord_channels_cleanup(&chs);
            return CCORD_BAD_PARAMETER;
        }
        if (ret->sync != DISCORD_SYNC_FLAG) {
            *ret->sync = chs.array[matched];
            memset(&chs.array[matched], 0, sizeof chs.array[matched]);
        }
        discord_channels_cleanup(&chs);
        return CCORD_OK;
    }

    cxt = malloc(sizeof *cxt);
    CCORD_EXPECT(client, cxt != NULL, CCORD_OUT_OF_MEMORY, "");
    *cxt = (struct _discord_get_channel_at_pos){ .type = type,
                                                 .position = position,
                                                 .ret = *ret };

    channels_ret.done = &_done_get_channels;
    channels_ret.fail = &_fail_get_channels;
    channels_ret.data = cxt;
    channels_ret.cleanup = &_cleanup_get_channels;

    if (ret->keep) {
        CCORDcode code =
            discord_refcounter_incr(&client->refcounter, (void *)ret->keep);
        if (code != CCORD_OK) {
            free(cxt);
            logmod_log(ERROR, client->logger,
                       "'.keep' data must be a Concord callback parameter");
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

    return discord_get_guild_channels(client, guild_id, &channels_ret);
}

/******************************************************************************
 * REST functions
 ******************************************************************************/

CCORDcode
discord_get_channel(struct discord *client,
                    u64snowflake channel_id,
                    struct discord_ret_channel *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_INIT(attr, discord_channel, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/channels/%" PRIu64, channel_id);
}

CCORDcode
discord_modify_channel(struct discord *client,
                       u64snowflake channel_id,
                       struct discord_modify_channel *params,
                       struct discord_ret_channel *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_modify_channel_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_channel, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_PATCH,
                            "/channels/%" PRIu64, channel_id);
}

CCORDcode
discord_delete_channel(struct discord *client,
                       u64snowflake channel_id,
                       struct discord_delete_channel *params,
                       struct discord_ret_channel *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_INIT(attr, discord_channel, ret,
                      params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/channels/%" PRIu64, channel_id);
}

CCORDcode
discord_get_channel_messages(struct discord *client,
                             u64snowflake channel_id,
                             struct discord_get_channel_messages *params,
                             struct discord_ret_messages *ret)
{
    struct discord_attributes attr = { 0 };
    char query[1024] = "";
    size_t qoff = 0;

    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    if (params) {
        bool ok = true;
        if (params->limit) {
            ok = _discord_query_append(query, sizeof(query), &qoff, "limit",
                                       "%d", params->limit);
            CCORD_EXPECT(client, ok, CCORD_MALFORMED_PAYLOAD,
                         "Out of bounds write attempt");
        }
        if (params->around) {
            ok = _discord_query_append(query, sizeof(query), &qoff, "around",
                                       "%" PRIu64, params->around);
            CCORD_EXPECT(client, ok, CCORD_MALFORMED_PAYLOAD,
                         "Out of bounds write attempt");
        }
        if (params->before) {
            ok = _discord_query_append(query, sizeof(query), &qoff, "before",
                                       "%" PRIu64, params->before);
            CCORD_EXPECT(client, ok, CCORD_MALFORMED_PAYLOAD,
                         "Out of bounds write attempt");
        }
        if (params->after) {
            ok = _discord_query_append(query, sizeof(query), &qoff, "after",
                                       "%" PRIu64, params->after);
            CCORD_EXPECT(client, ok, CCORD_MALFORMED_PAYLOAD,
                         "Out of bounds write attempt");
        }
    }
    DISCORD_ATTR_LIST_INIT(attr, discord_messages, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/channels/%" PRIu64 "/messages%s", channel_id,
                            query);
}

CCORDcode
discord_get_channel_message(struct discord *client,
                            u64snowflake channel_id,
                            u64snowflake message_id,
                            struct discord_ret_message *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_INIT(attr, discord_message, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/channels/%" PRIu64 "/messages/%" PRIu64,
                            channel_id, message_id);
}

CCORDcode
discord_create_message(struct discord *client,
                       u64snowflake channel_id,
                       struct discord_create_message *params,
                       struct discord_ret_message *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    enum http_method method;
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_create_message_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_message, ret, NULL);
    if (params->attachments) {
        method = HTTP_MIMEPOST;
        DISCORD_ATTACHMENTS_IDS_INIT(params->attachments);
        attr.attachments = *params->attachments;
    }
    else {
        method = HTTP_POST;
    }
    return discord_rest_run(&client->rest, &attr, &body, method,
                            "/channels/%" PRIu64 "/messages", channel_id);
}

CCORDcode
discord_crosspost_message(struct discord *client,
                          u64snowflake channel_id,
                          u64snowflake message_id,
                          struct discord_ret_message *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_INIT(attr, discord_message, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_POST,
                            "/channels/%" PRIu64 "/messages/%" PRIu64
                            "/crosspost",
                            channel_id, message_id);
}

CCORDcode
discord_create_reaction(struct discord *client,
                        u64snowflake channel_id,
                        u64snowflake message_id,
                        u64snowflake emoji_id,
                        const char emoji_name[],
                        struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    char emoji_endpoint[256];
    CCORDcode code;

    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    code = _discord_format_reaction_emoji(
        client, emoji_endpoint, sizeof(emoji_endpoint), emoji_id, emoji_name);
    if (code != CCORD_OK) return code;

    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_PUT,
                            "/channels/%" PRIu64 "/messages/%" PRIu64
                            "/reactions/%s/@me",
                            channel_id, message_id, emoji_endpoint);
}

CCORDcode
discord_delete_own_reaction(struct discord *client,
                            u64snowflake channel_id,
                            u64snowflake message_id,
                            u64snowflake emoji_id,
                            const char emoji_name[],
                            struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    char emoji_endpoint[256];
    CCORDcode code;

    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    code = _discord_format_reaction_emoji(
        client, emoji_endpoint, sizeof(emoji_endpoint), emoji_id, emoji_name);
    if (code != CCORD_OK) return code;

    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/channels/%" PRIu64 "/messages/%" PRIu64
                            "/reactions/%s/@me",
                            channel_id, message_id, emoji_endpoint);
}

CCORDcode
discord_delete_user_reaction(struct discord *client,
                             u64snowflake channel_id,
                             u64snowflake message_id,
                             u64snowflake user_id,
                             u64snowflake emoji_id,
                             const char emoji_name[],
                             struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    char emoji_endpoint[256];
    CCORDcode code;

    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, user_id != 0, CCORD_BAD_PARAMETER, "");
    code = _discord_format_reaction_emoji(
        client, emoji_endpoint, sizeof(emoji_endpoint), emoji_id, emoji_name);
    if (code != CCORD_OK) return code;

    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/channels/%" PRIu64 "/messages/%" PRIu64
                            "/reactions/%s/%" PRIu64,
                            channel_id, message_id, emoji_endpoint, user_id);
}

CCORDcode
discord_get_reactions(struct discord *client,
                      u64snowflake channel_id,
                      u64snowflake message_id,
                      u64snowflake emoji_id,
                      const char emoji_name[],
                      struct discord_get_reactions *params,
                      struct discord_ret_users *ret)
{
    struct discord_attributes attr = { 0 };
    char emoji_endpoint[256];
    char query[1024] = "";
    size_t qoff = 0;
    CCORDcode code;

    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    code = _discord_format_reaction_emoji(
        client, emoji_endpoint, sizeof(emoji_endpoint), emoji_id, emoji_name);
    if (code != CCORD_OK) return code;

    if (params) {
        bool ok = true;
        if (params->after) {
            ok = _discord_query_append(query, sizeof(query), &qoff, "after",
                                       "%" PRIu64, params->after);
            CCORD_EXPECT(client, ok, CCORD_MALFORMED_PAYLOAD,
                         "Out of bounds write attempt");
        }
        if (params->limit) {
            CCORD_EXPECT(client, params->limit > 0 && params->limit <= 100,
                         CCORD_BAD_PARAMETER, "");
            ok = _discord_query_append(query, sizeof(query), &qoff, "limit",
                                       "%d", params->limit);
            CCORD_EXPECT(client, ok, CCORD_MALFORMED_PAYLOAD,
                         "Out of bounds write attempt");
        }
    }

    DISCORD_ATTR_LIST_INIT(attr, discord_users, ret, NULL);
    attr.response.from_json = _discord_reaction_users_from_json;
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/channels/%" PRIu64 "/messages/%" PRIu64
                            "/reactions/%s%s",
                            channel_id, message_id, emoji_endpoint, query);
}

CCORDcode
discord_delete_all_reactions(struct discord *client,
                             u64snowflake channel_id,
                             u64snowflake message_id,
                             struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/channels/%" PRIu64 "/messages/%" PRIu64
                            "/reactions",
                            channel_id, message_id);
}

CCORDcode
discord_delete_all_reactions_for_emoji(struct discord *client,
                                       u64snowflake channel_id,
                                       u64snowflake message_id,
                                       u64snowflake emoji_id,
                                       const char emoji_name[],
                                       struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    char emoji_endpoint[256];
    CCORDcode code;

    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    code = _discord_format_reaction_emoji(
        client, emoji_endpoint, sizeof(emoji_endpoint), emoji_id, emoji_name);
    if (code != CCORD_OK) return code;

    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/channels/%" PRIu64 "/messages/%" PRIu64
                            "/reactions/%s",
                            channel_id, message_id, emoji_endpoint);
}

CCORDcode
discord_edit_message(struct discord *client,
                     u64snowflake channel_id,
                     u64snowflake message_id,
                     struct discord_edit_message *params,
                     struct discord_ret_message *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(
        client, discord_edit_message_to_json(&body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_message, ret, NULL);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_PATCH,
                            "/channels/%" PRIu64 "/messages/%" PRIu64,
                            channel_id, message_id);
}

CCORDcode
discord_delete_message(struct discord *client,
                       u64snowflake channel_id,
                       u64snowflake message_id,
                       struct discord_delete_message *params,
                       struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/channels/%" PRIu64 "/messages/%" PRIu64,
                            channel_id, message_id);
}

CCORDcode
discord_bulk_delete_messages(struct discord *client,
                             u64snowflake channel_id,
                             struct discord_bulk_delete_messages *params,
                             struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params->messages != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client,
                 params->messages->size >= 1 && params->messages->size <= 100,
                 CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_bulk_delete_messages_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_BLANK_INIT(attr, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_POST,
                            "/channels/%" PRIu64 "/messages/bulk-delete",
                            channel_id);
}

CCORDcode
discord_edit_channel_permissions(
    struct discord *client,
    u64snowflake channel_id,
    u64snowflake overwrite_id,
    struct discord_edit_channel_permissions *params,
    struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, overwrite_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_edit_channel_permissions_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_BLANK_INIT(attr, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_PUT,
                            "/channels/%" PRIu64 "/permissions/%" PRIu64,
                            channel_id, overwrite_id);
}

CCORDcode
discord_get_channel_invites(struct discord *client,
                            u64snowflake channel_id,
                            struct discord_ret_invites *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_LIST_INIT(attr, discord_invites, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/channels/%" PRIu64 "/invites", channel_id);
}

CCORDcode
discord_create_channel_invite(struct discord *client,
                              u64snowflake channel_id,
                              struct discord_create_channel_invite *params,
                              struct discord_ret_invite *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    if (params) {
        CCORD_EXPECT_OK(client, discord_create_channel_invite_to_json(
                                    &body.start, &body.size, params));
    }
    else {
        body = (struct ccord_szbuf){ "{}", 2, true };
    }
    DISCORD_ATTR_INIT(attr, discord_invite, ret,
                      params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_POST,
                            "/channels/%" PRIu64 "/invites", channel_id);
}

CCORDcode
discord_delete_channel_permission(
    struct discord *client,
    u64snowflake channel_id,
    u64snowflake overwrite_id,
    struct discord_delete_channel_permission *params,
    struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, overwrite_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/channels/%" PRIu64 "/permissions/%" PRIu64,
                            channel_id, overwrite_id);
}

CCORDcode
discord_follow_news_channel(struct discord *client,
                            u64snowflake channel_id,
                            struct discord_follow_news_channel *params,
                            struct discord_ret_followed_channel *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params->webhook_channel_id != 0, CCORD_BAD_PARAMETER,
                 "");
    CCORD_EXPECT_OK(client, discord_follow_news_channel_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_followed_channel, ret, NULL);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_POST,
                            "/channels/%" PRIu64 "/followers", channel_id);
}

CCORDcode
discord_trigger_typing_indicator(struct discord *client,
                                 u64snowflake channel_id,
                                 struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_POST,
                            "/channels/%" PRIu64 "/typing", channel_id);
}

CCORDcode
discord_get_pinned_messages(struct discord *client,
                            u64snowflake channel_id,
                            struct discord_ret_messages *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_LIST_INIT(attr, discord_messages, ret, NULL);
    attr.response.from_json = _discord_pinned_messages_from_json;
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/channels/%" PRIu64 "/messages/pins", channel_id);
}

CCORDcode
discord_pin_message(struct discord *client,
                    u64snowflake channel_id,
                    u64snowflake message_id,
                    struct discord_pin_message *params,
                    struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_PUT,
                            "/channels/%" PRIu64 "/pins/%" PRIu64, channel_id,
                            message_id);
}

CCORDcode
discord_unpin_message(struct discord *client,
                      u64snowflake channel_id,
                      u64snowflake message_id,
                      struct discord_unpin_message *params,
                      struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/channels/%" PRIu64 "/pins/%" PRIu64, channel_id,
                            message_id);
}

CCORDcode
discord_group_dm_add_recipient(struct discord *client,
                               u64snowflake channel_id,
                               u64snowflake user_id,
                               struct discord_group_dm_add_recipient *params,
                               struct discord_ret *ret)
{
    (void)channel_id;
    (void)user_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_group_dm_remove_recipient(struct discord *client,
                                  u64snowflake channel_id,
                                  u64snowflake user_id,
                                  struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, user_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/channels/%" PRIu64 "/recipients/%" PRIu64,
                            channel_id, user_id);
}

CCORDcode
discord_start_thread_with_message(
    struct discord *client,
    u64snowflake channel_id,
    u64snowflake message_id,
    struct discord_start_thread_with_message *params,
    struct discord_ret_channel *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_start_thread_with_message_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_channel, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_POST,
                            "/channels/%" PRIu64 "/messages/%" PRIu64
                            "/threads",
                            channel_id, message_id);
}

CCORDcode
discord_start_thread_without_message(
    struct discord *client,
    u64snowflake channel_id,
    struct discord_start_thread_without_message *params,
    struct discord_ret_channel *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_start_thread_without_message_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_channel, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_POST,
                            "/channels/%" PRIu64 "/threads", channel_id);
}

CCORDcode
discord_join_thread(struct discord *client,
                    u64snowflake channel_id,
                    struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_PUT,
                            "/channels/%" PRIu64 "/thread-members/@me",
                            channel_id);
}

CCORDcode
discord_add_thread_member(struct discord *client,
                          u64snowflake channel_id,
                          u64snowflake user_id,
                          struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, user_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_PUT,
                            "/channels/%" PRIu64 "/thread-members/%" PRIu64,
                            channel_id, user_id);
}

CCORDcode
discord_leave_thread(struct discord *client,
                     u64snowflake channel_id,
                     struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/channels/%" PRIu64 "/thread-members/@me",
                            channel_id);
}

CCORDcode
discord_remove_thread_member(struct discord *client,
                             u64snowflake channel_id,
                             u64snowflake user_id,
                             struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, user_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/channels/%" PRIu64 "/thread-members/%" PRIu64,
                            channel_id, user_id);
}

CCORDcode
discord_list_thread_members(struct discord *client,
                            u64snowflake channel_id,
                            struct discord_ret_thread_members *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_LIST_INIT(attr, discord_thread_members, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/channels/%" PRIu64 "/thread-members",
                            channel_id);
}

CCORDcode
discord_list_public_archived_threads(
    struct discord *client,
    u64snowflake channel_id,
    u64unix_ms before,
    int limit,
    struct discord_ret_thread_response_body *ret)
{
    struct discord_attributes attr = { 0 };
    char query[1024] = "";
    size_t qoff = 0;

    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    if (before) {
        char iso[64];
        bool ok = _discord_unix_ms_to_iso8601(iso, sizeof(iso), before)
                  && _discord_query_append(query, sizeof(query), &qoff,
                                           "before", "%s", iso);
        CCORD_EXPECT(client, ok, CCORD_MALFORMED_PAYLOAD,
                     "Out of bounds write attempt");
    }
    if (limit) {
        bool ok = _discord_query_append(query, sizeof(query), &qoff, "limit",
                                        "%d", limit);
        CCORD_EXPECT(client, ok, CCORD_MALFORMED_PAYLOAD,
                     "Out of bounds write attempt");
    }
    DISCORD_ATTR_INIT(attr, discord_thread_response_body, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/channels/%" PRIu64 "/threads/archived/public%s",
                            channel_id, query);
}

CCORDcode
discord_list_private_archived_threads(
    struct discord *client,
    u64snowflake channel_id,
    u64unix_ms before,
    int limit,
    struct discord_ret_thread_response_body *ret)
{
    struct discord_attributes attr = { 0 };
    char query[1024] = "";
    size_t qoff = 0;

    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    if (before) {
        char iso[64];
        bool ok = _discord_unix_ms_to_iso8601(iso, sizeof(iso), before)
                  && _discord_query_append(query, sizeof(query), &qoff,
                                           "before", "%s", iso);
        CCORD_EXPECT(client, ok, CCORD_MALFORMED_PAYLOAD,
                     "Out of bounds write attempt");
    }
    if (limit) {
        bool ok = _discord_query_append(query, sizeof(query), &qoff, "limit",
                                        "%d", limit);
        CCORD_EXPECT(client, ok, CCORD_MALFORMED_PAYLOAD,
                     "Out of bounds write attempt");
    }
    DISCORD_ATTR_INIT(attr, discord_thread_response_body, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/channels/%" PRIu64 "/threads/archived/private%s",
                            channel_id, query);
}

CCORDcode
discord_list_joined_private_archived_threads(
    struct discord *client,
    u64snowflake channel_id,
    u64unix_ms before,
    int limit,
    struct discord_ret_thread_response_body *ret)
{
    struct discord_attributes attr = { 0 };
    char query[1024] = "";
    size_t qoff = 0;

    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    if (before) {
        bool ok = _discord_query_append(query, sizeof(query), &qoff, "before",
                                        "%" PRIu64, before);
        CCORD_EXPECT(client, ok, CCORD_MALFORMED_PAYLOAD,
                     "Out of bounds write attempt");
    }
    if (limit) {
        bool ok = _discord_query_append(query, sizeof(query), &qoff, "limit",
                                        "%d", limit);
        CCORD_EXPECT(client, ok, CCORD_MALFORMED_PAYLOAD,
                     "Out of bounds write attempt");
    }
    DISCORD_ATTR_INIT(attr, discord_thread_response_body, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/channels/%" PRIu64
                            "/users/@me/threads/archived/private%s",
                            channel_id, query);
}
