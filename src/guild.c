#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "carray.h"
#include "discord.h"
#include "discord-internal.h"
#include "discord-request.h"

static bool
_fluxer_unescape_dup(const jsmnf_pair *f, const char *js, char **out)
{
    size_t len;
    char *buf;

    if (!f || f->v->type != JSMN_STRING) return true;
    len = (size_t)(f->v->end - f->v->start);
    buf = calloc(1, len + 1);
    if (!buf) return false;
    if (jsmnf_unescape(buf, len, js + f->v->start, len) < 0) {
        free(buf);
        return false;
    }
    *out = buf;
    return true;
}

static CCORDcode
_discord_search_guild_members_to_json(
    char *buf[],
    size_t *size,
    const struct discord_search_guild_members *params)
{
    jsonb b;

    jsonb_init(&b);
    if (jsonb_object_auto(&b, buf, size) < 0) goto fail;
    if (params) {
        if (params->query) {
            if (jsonb_key_auto(&b, buf, size, "query", sizeof("query") - 1) < 0
                || jsonb_string_auto(&b, buf, size, params->query,
                                     strlen(params->query))
                       < 0)
            {
                goto fail;
            }
        }
        if (params->limit > 0) {
            if (jsonb_key_auto(&b, buf, size, "limit", sizeof("limit") - 1) < 0
                || jsonb_number_auto(&b, buf, size, params->limit) < 0)
            {
                goto fail;
            }
        }
    }
    if (jsonb_object_pop_auto(&b, buf, size) != JSONB_END) goto fail;
    *size = b.pos;
    return CCORD_OK;

fail:
    free(*buf);
    *buf = NULL;
    *size = 0;
    return CCORD_MALFORMED_PAYLOAD;
}

static size_t
_discord_fluxer_member_search_from_json(const char *json,
                                        size_t len,
                                        void *p_data)
{
    struct discord_guild_members *self = p_data;
    jsmnf_loader loader;
    jsmnf_table *table = NULL;
    size_t table_len = 0;

    if (!json || !len) return 0;

    jsmnf_init(&loader);
    if (jsmnf_load_auto(&loader, json, len, &table, &table_len) <= 0) return 0;

    if (loader.root->v->type == JSMN_ARRAY) {
        discord_guild_members_from_jsmnf(loader.root, json, self);
        free(table);
        return len;
    }

    if (loader.root->v->type == JSMN_OBJECT) {
        const jsmnf_pair *members =
            jsmnf_find(loader.root, "members", sizeof("members") - 1);
        const jsmnf_pair *root_gid =
            jsmnf_find(loader.root, "guild_id", sizeof("guild_id") - 1);

        if (members && members->v->type == JSMN_ARRAY && members->length > 0) {
            int i, count = (int)members->length;
            self->realsize = count;
            self->size = 0;
            self->array = calloc(members->length, sizeof *self->array);
            if (!self->array) {
                free(table);
                return 0;
            }
            for (i = 0; i < count; ++i) {
                const jsmnf_pair *item = members->fields + i;
                const jsmnf_pair *f;
                struct discord_guild_member member = { 0 };

                if ((f = jsmnf_find(item, "guild_id", sizeof("guild_id") - 1))
                    || (f = root_gid))
                {
                    sscanf(json + f->v->start, "%" SCNu64, &member.guild_id);
                }

                f = jsmnf_find(item, "nickname", sizeof("nickname") - 1);
                if (!f) f = jsmnf_find(item, "nick", sizeof("nick") - 1);
                (void)_fluxer_unescape_dup(f, json, &member.nick);

                f = jsmnf_find(item, "role_ids", sizeof("role_ids") - 1);
                if (!f) f = jsmnf_find(item, "roles", sizeof("roles") - 1);
                if (f && f->v->type == JSMN_ARRAY) {
                    member.roles = calloc(1, sizeof *member.roles);
                    if (member.roles)
                        snowflakes_from_jsmnf(f, json, member.roles);
                }

                if ((f = jsmnf_find(item, "joined_at",
                                    sizeof("joined_at") - 1)))
                {
                    if (f->v->type == JSMN_PRIMITIVE) {
                        member.joined_at =
                            (u64unix_ms)strtoull(json + f->v->start, NULL, 10)
                            * 1000;
                    }
                    else if (f->v->type == JSMN_STRING) {
                        cog_iso8601_to_unix_ms(json + f->v->start,
                                               (size_t)(f->v->end
                                                        - f->v->start),
                                               &member.joined_at);
                    }
                }

                if ((f = jsmnf_find(item, "user", sizeof("user") - 1))
                    && f->v->type == JSMN_OBJECT)
                {
                    member.user = calloc(1, sizeof *member.user);
                    if (member.user)
                        discord_user_from_jsmnf(f, json, member.user);
                }
                else {
                    member.user = calloc(1, sizeof *member.user);
                    if (member.user) {
                        if ((f = jsmnf_find(item, "user_id",
                                            sizeof("user_id") - 1)))
                        {
                            sscanf(json + f->v->start, "%" SCNu64,
                                   &member.user->id);
                        }
                        f = jsmnf_find(item, "username",
                                       sizeof("username") - 1);
                        (void)_fluxer_unescape_dup(f, json,
                                                   &member.user->username);
                        f = jsmnf_find(item, "discriminator",
                                       sizeof("discriminator") - 1);
                        (void)_fluxer_unescape_dup(
                            f, json, &member.user->discriminator);
                        if ((f = jsmnf_find(item, "is_bot",
                                            sizeof("is_bot") - 1))
                            && f->v->type == JSMN_PRIMITIVE)
                        {
                            member.user->bot = ('t' == json[f->v->start]);
                        }
                    }
                }

                carray_insert(self, i, member);
            }
        }
    }

    free(table);
    return len;
}

static size_t
_discord_fluxer_invites_from_json(const char *json, size_t len, void *p_data)
{
    struct discord_invites *self = p_data;
    long parsed = discord_invites_from_json(json, len, self);
    jsmnf_loader loader;
    jsmnf_table *table = NULL;
    size_t table_len = 0;

    if (parsed <= 0 || !self->size) return parsed > 0 ? (size_t)parsed : 0;

    jsmnf_init(&loader);
    if (jsmnf_load_auto(&loader, json, len, &table, &table_len) > 0) {
        if (loader.root->v->type == JSMN_ARRAY) {
            int i;
            for (i = 0; i < self->size && (size_t)i < loader.root->length; ++i)
            {
                const jsmnf_pair *item = loader.root->fields + i;
                const jsmnf_pair *f;

                if (!self->array[i].approximate_member_count
                    && (f = jsmnf_find(item, "member_count",
                                       sizeof("member_count") - 1))
                    && f->v->type == JSMN_PRIMITIVE)
                {
                    self->array[i].approximate_member_count =
                        (int)strtol(json + f->v->start, NULL, 10);
                }
                if (!self->array[i].approximate_presence_count
                    && (f = jsmnf_find(item, "presence_count",
                                       sizeof("presence_count") - 1))
                    && f->v->type == JSMN_PRIMITIVE)
                {
                    self->array[i].approximate_presence_count =
                        (int)strtol(json + f->v->start, NULL, 10);
                }
            }
        }
        free(table);
    }
    return (size_t)parsed;
}

CCORDcode
discord_create_guild(struct discord *client,
                     struct discord_create_guild *params,
                     struct discord_ret_guild *ret)
{
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_guild(struct discord *client,
                  u64snowflake guild_id,
                  struct discord_ret_guild *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_INIT(attr, discord_guild, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/guilds/%" PRIu64, guild_id);
}

CCORDcode
discord_get_guild_preview(struct discord *client,
                          u64snowflake guild_id,
                          struct discord_ret_guild_preview *ret)
{
    (void)guild_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_modify_guild(struct discord *client,
                     u64snowflake guild_id,
                     struct discord_modify_guild *params,
                     struct discord_ret_guild *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(
        client, discord_modify_guild_to_json(&body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_guild, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_PATCH,
                            "/guilds/%" PRIu64, guild_id);
}

CCORDcode
discord_delete_guild(struct discord *client,
                     u64snowflake guild_id,
                     struct discord_ret *ret)
{
    (void)guild_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_guild_channels(struct discord *client,
                           u64snowflake guild_id,
                           struct discord_ret_channels *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_LIST_INIT(attr, discord_channels, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/guilds/%" PRIu64 "/channels", guild_id);
}

CCORDcode
discord_create_guild_channel(struct discord *client,
                             u64snowflake guild_id,
                             struct discord_create_guild_channel *params,
                             struct discord_ret_channel *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_create_guild_channel_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_channel, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_POST,
                            "/guilds/%" PRIu64 "/channels", guild_id);
}

CCORDcode
discord_modify_guild_channel_positions(
    struct discord *client,
    u64snowflake guild_id,
    struct discord_modify_guild_channel_positions *params,
    struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_modify_guild_channel_positions_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_PATCH,
                            "/guilds/%" PRIu64 "/channels", guild_id);
}

CCORDcode
discord_get_guild_member(struct discord *client,
                         u64snowflake guild_id,
                         u64snowflake user_id,
                         struct discord_ret_guild_member *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, user_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_INIT(attr, discord_guild_member, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/guilds/%" PRIu64 "/members/%" PRIu64, guild_id,
                            user_id);
}

CCORDcode
discord_list_guild_members(struct discord *client,
                           u64snowflake guild_id,
                           struct discord_list_guild_members *params,
                           struct discord_ret_guild_members *ret)
{
    struct discord_attributes attr = { 0 };
    char query[128] = "";
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_LIST_INIT(attr, discord_guild_members, ret, NULL);
    if (params) {
        size_t offset = 0;
        if (params->limit > 0) {
            int n = snprintf(query + offset, sizeof(query) - offset,
                             "?limit=%d", params->limit);
            CCORD_EXPECT(client, n > 0 && (size_t)n < sizeof(query) - offset,
                         CCORD_MALFORMED_PAYLOAD,
                         "Out of bounds write attempt");
            offset += (size_t)n;
        }
        if (params->after != 0) {
            int n = snprintf(query + offset, sizeof(query) - offset,
                             "%cafter=%" PRIu64, offset ? '&' : '?',
                             params->after);
            CCORD_EXPECT(client, n > 0 && (size_t)n < sizeof(query) - offset,
                         CCORD_MALFORMED_PAYLOAD,
                         "Out of bounds write attempt");
        }
    }
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/guilds/%" PRIu64 "/members%s", guild_id, query);
}

CCORDcode
discord_search_guild_members(struct discord *client,
                             u64snowflake guild_id,
                             struct discord_search_guild_members *params,
                             struct discord_ret_guild_members *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, _discord_search_guild_members_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_LIST_INIT(attr, discord_guild_members, ret, NULL);
    attr.response.from_json = &_discord_fluxer_member_search_from_json;
    return discord_rest_run(&client->rest, &attr, &body, HTTP_POST,
                            "/guilds/%" PRIu64 "/members-search", guild_id);
}

CCORDcode
discord_add_guild_member(struct discord *client,
                         u64snowflake guild_id,
                         u64snowflake user_id,
                         struct discord_add_guild_member *params,
                         struct discord_ret_guild_member *ret)
{
    (void)guild_id;
    (void)user_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_modify_guild_member(struct discord *client,
                            u64snowflake guild_id,
                            u64snowflake user_id,
                            struct discord_modify_guild_member *params,
                            struct discord_ret_guild_member *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, user_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_modify_guild_member_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_guild_member, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_PATCH,
                            "/guilds/%" PRIu64 "/members/%" PRIu64, guild_id,
                            user_id);
}

CCORDcode
discord_modify_current_member(struct discord *client,
                              u64snowflake guild_id,
                              struct discord_modify_current_member *params,
                              struct discord_ret_guild_member *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params->nick != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_modify_current_member_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_guild_member, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_PATCH,
                            "/guilds/%" PRIu64 "/members/@me", guild_id);
}

CCORDcode
discord_add_guild_member_role(struct discord *client,
                              u64snowflake guild_id,
                              u64snowflake user_id,
                              u64snowflake role_id,
                              struct discord_add_guild_member_role *params,
                              struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, user_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, role_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_PUT,
                            "/guilds/%" PRIu64 "/members/%" PRIu64
                            "/roles/%" PRIu64,
                            guild_id, user_id, role_id);
}

CCORDcode
discord_remove_guild_member_role(
    struct discord *client,
    u64snowflake guild_id,
    u64snowflake user_id,
    u64snowflake role_id,
    struct discord_remove_guild_member_role *params,
    struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, user_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, role_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/guilds/%" PRIu64 "/members/%" PRIu64
                            "/roles/%" PRIu64,
                            guild_id, user_id, role_id);
}

CCORDcode
discord_remove_guild_member(struct discord *client,
                            u64snowflake guild_id,
                            u64snowflake user_id,
                            struct discord_remove_guild_member *params,
                            struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, user_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/guilds/%" PRIu64 "/members/%" PRIu64, guild_id,
                            user_id);
}

CCORDcode
discord_get_guild_bans(struct discord *client,
                       u64snowflake guild_id,
                       struct discord_ret_bans *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_LIST_INIT(attr, discord_bans, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/guilds/%" PRIu64 "/bans", guild_id);
}

CCORDcode
discord_get_guild_ban(struct discord *client,
                      u64snowflake guild_id,
                      u64snowflake user_id,
                      struct discord_ret_ban *ret)
{
    (void)guild_id;
    (void)user_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_create_guild_ban(struct discord *client,
                         u64snowflake guild_id,
                         u64snowflake user_id,
                         struct discord_create_guild_ban *params,
                         struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, user_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client,
                 params->delete_message_days >= 0
                     && params->delete_message_days <= 7,
                 CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_create_guild_ban_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_BLANK_INIT(attr, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_PUT,
                            "/guilds/%" PRIu64 "/bans/%" PRIu64, guild_id,
                            user_id);
}

CCORDcode
discord_remove_guild_ban(struct discord *client,
                         u64snowflake guild_id,
                         u64snowflake user_id,
                         struct discord_remove_guild_ban *params,
                         struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, user_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/guilds/%" PRIu64 "/bans/%" PRIu64, guild_id,
                            user_id);
}

CCORDcode
discord_get_guild_roles(struct discord *client,
                        u64snowflake guild_id,
                        struct discord_ret_roles *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_LIST_INIT(attr, discord_roles, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/guilds/%" PRIu64 "/roles", guild_id);
}

CCORDcode
discord_create_guild_role(struct discord *client,
                          u64snowflake guild_id,
                          struct discord_create_guild_role *params,
                          struct discord_ret_role *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_create_guild_role_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_role, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_POST,
                            "/guilds/%" PRIu64 "/roles", guild_id);
}

CCORDcode
discord_modify_guild_role_positions(
    struct discord *client,
    u64snowflake guild_id,
    struct discord_modify_guild_role_positions *params,
    struct discord_ret_roles *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_modify_guild_role_positions_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_LIST_INIT(attr, discord_roles, ret, NULL);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_PATCH,
                            "/guilds/%" PRIu64 "/roles", guild_id);
}

CCORDcode
discord_modify_guild_role(struct discord *client,
                          u64snowflake guild_id,
                          u64snowflake role_id,
                          struct discord_modify_guild_role *params,
                          struct discord_ret_role *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, role_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_modify_guild_role_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_role, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_PATCH,
                            "/guilds/%" PRIu64 "/roles/%" PRIu64, guild_id,
                            role_id);
}

CCORDcode
discord_delete_guild_role(struct discord *client,
                          u64snowflake guild_id,
                          u64snowflake role_id,
                          struct discord_delete_guild_role *params,
                          struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, role_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/guilds/%" PRIu64 "/roles/%" PRIu64, guild_id,
                            role_id);
}

CCORDcode
discord_get_guild_prune_count(struct discord *client,
                              u64snowflake guild_id,
                              struct discord_get_guild_prune_count *params,
                              struct discord_ret_prune_count *ret)
{
    (void)guild_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_begin_guild_prune(struct discord *client,
                          u64snowflake guild_id,
                          struct discord_begin_guild_prune *params,
                          struct discord_ret *ret)
{
    (void)guild_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_guild_voice_regions(struct discord *client,
                                u64snowflake guild_id,
                                struct discord_ret_voice_regions *ret)
{
    (void)guild_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_guild_invites(struct discord *client,
                          u64snowflake guild_id,
                          struct discord_ret_invites *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_LIST_INIT(attr, discord_invites, ret, NULL);
    attr.response.from_json = &_discord_fluxer_invites_from_json;
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/guilds/%" PRIu64 "/invites", guild_id);
}

CCORDcode
discord_get_guild_integrations(struct discord *client,
                               u64snowflake guild_id,
                               struct discord_ret_integrations *ret)
{
    (void)guild_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_delete_guild_integrations(
    struct discord *client,
    u64snowflake guild_id,
    u64snowflake integration_id,
    struct discord_delete_guild_integrations *params,
    struct discord_ret *ret)
{
    (void)guild_id;
    (void)integration_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_guild_widget_settings(
    struct discord *client,
    u64snowflake guild_id,
    struct discord_ret_guild_widget_settings *ret)
{
    (void)guild_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_modify_guild_widget(struct discord *client,
                            u64snowflake guild_id,
                            struct discord_guild_widget_settings *params,
                            struct discord_ret_guild_widget_settings *ret)
{
    (void)guild_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_guild_widget(struct discord *client,
                         u64snowflake guild_id,
                         struct discord_ret_guild_widget *ret)
{
    (void)guild_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_get_guild_vanity_url(struct discord *client,
                             u64snowflake guild_id,
                             struct discord_ret_invite *ret)
{
    struct discord_attributes attr = { 0 };
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_INIT(attr, discord_invite, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/guilds/%" PRIu64 "/vanity-url", guild_id);
}

CCORDcode
discord_get_guild_welcome_screen(struct discord *client,
                                 u64snowflake guild_id,
                                 struct discord_ret_welcome_screen *ret)
{
    (void)guild_id;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_modify_guild_welcome_screen(
    struct discord *client,
    u64snowflake guild_id,
    struct discord_modify_guild_welcome_screen *params,
    struct discord_ret_welcome_screen *ret)
{
    (void)guild_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_modify_current_user_voice_state(
    struct discord *client,
    u64snowflake guild_id,
    struct discord_modify_current_user_voice_state *params,
    struct discord_ret *ret)
{
    (void)guild_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}

CCORDcode
discord_modify_user_voice_state(struct discord *client,
                                u64snowflake guild_id,
                                u64snowflake user_id,
                                struct discord_modify_user_voice_state *params,
                                struct discord_ret *ret)
{
    (void)guild_id;
    (void)user_id;
    (void)params;
    return discord_rest_unsupported(client, ret ? ret->fail : NULL,
                                    ret ? ret->data : NULL,
                                    ret ? ret->keep : NULL);
}
