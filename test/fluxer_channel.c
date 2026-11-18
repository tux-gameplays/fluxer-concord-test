#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "discord.h"
#include "discord-internal.h"
#include "greatest.h"

#define MAX_MOCK_REQUESTS 32
#define MOCK_BUFFER_SIZE 8192

struct unsupported_context {
    int calls;
    CCORDcode code;
    void *data;
    const void *keep;
};

static void
on_unsupported(struct discord *client, struct discord_response *response)
{
    (void)client;
    struct unsupported_context *context = response->data;
    ++context->calls;
    context->code = response->code;
    context->data = response->data;
    context->keep = response->keep;
}

struct http_mock {
    int fd;
    unsigned short port;
    size_t expected;
    size_t served;
    bool keep_listening;
    const char *statuses[MAX_MOCK_REQUESTS];
    const char *bodies[MAX_MOCK_REQUESTS];
    char requests[MAX_MOCK_REQUESTS][MOCK_BUFFER_SIZE];
};

static int
mock_open(struct http_mock *mock, size_t expected)
{
    struct sockaddr_in address = { .sin_family = AF_INET,
                                   .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
                                   .sin_port = 0 };
    socklen_t length = sizeof(address);
    mock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (mock->fd < 0) return -1;
    if (bind(mock->fd, (struct sockaddr *)&address, sizeof(address)) < 0
        || listen(mock->fd, MAX_MOCK_REQUESTS) < 0
        || getsockname(mock->fd, (struct sockaddr *)&address, &length) < 0)
    {
        close(mock->fd);
        return -1;
    }
    mock->port = ntohs(address.sin_port);
    mock->expected = expected;
    mock->served = 0;
    memset(mock->requests, 0, sizeof(mock->requests));
    for (size_t i = 0; i < MAX_MOCK_REQUESTS; ++i) {
        mock->statuses[i] = "200 OK";
        mock->bodies[i] = "{}";
    }
    return 0;
}

static size_t
request_content_length(const char request[], size_t header_size)
{
    const char *line = request;
    while (line < request + header_size) {
        const char *end = strstr(line, "\r\n");
        if (!end || end > request + header_size) break;
        if (strncasecmp(line, "Content-Length:", sizeof("Content-Length:") - 1)
            == 0)
        {
            return (size_t)strtoul(line + sizeof("Content-Length:") - 1,
                                   NULL, 10);
        }
        line = end + 2;
    }
    return 0;
}

static void *
mock_serve(void *data)
{
    struct http_mock *mock = data;
    for (size_t i = 0; i < mock->expected; ++i) {
        int connection = accept(mock->fd, NULL, NULL);
        if (connection < 0) break;

        char *request = mock->requests[i];
        size_t used = 0, header_size = 0, expected_size = 0;
        while (used + 1 < MOCK_BUFFER_SIZE) {
            ssize_t count = recv(connection, request + used,
                                 MOCK_BUFFER_SIZE - used - 1, 0);
            if (count <= 0) break;
            used += (size_t)count;
            request[used] = '\0';
            char *headers_end = strstr(request, "\r\n\r\n");
            if (headers_end && !header_size) {
                header_size = (size_t)(headers_end + 4 - request);
                expected_size = header_size
                                + request_content_length(request, header_size);
            }
            if (header_size && used >= expected_size) break;
        }

        const char *body = mock->bodies[i];
        const char *status = mock->statuses[i];
        char response[MOCK_BUFFER_SIZE];
        int response_size = snprintf(
            response, sizeof(response),
            "HTTP/1.1 %s\r\nContent-Type: application/json\r\n"
            "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
            status, strlen(body), body);
        if (response_size > 0 && (size_t)response_size < sizeof(response))
            (void)send(connection, response, (size_t)response_size, 0);
        close(connection);
        ++mock->served;
    }
    if (!mock->keep_listening) close(mock->fd);
    return NULL;
}

static bool
write_config(const char origin[], char path[])
{
    static const char token[] =
        "123456789012345678.abcdefghijklmnopqrstuvwxyz0123456789ABCD";
    char json[512];
    int length = snprintf(json, sizeof(json),
                          "{\"token\":\"%s\",\"instance_origin\":\"%s\","
                          "\"log\":{\"quiet\":true}}",
                          token, origin);
    if (length < 0 || (size_t)length >= sizeof(json)) return false;
    int fd = mkstemp(path);
    if (fd < 0) return false;
    ssize_t written = write(fd, json, (size_t)length);
    close(fd);
    return written == length;
}

static void
setup_init_responses(struct http_mock *mock,
                     char origin[],
                     size_t origin_sz,
                     char discovery_json[],
                     size_t discovery_sz)
{
    static const char self_user_json[] =
        "{\"id\":\"9223372036854775812\",\"username\":\"fluxer-bot\","
        "\"discriminator\":\"0000\",\"bot\":true}";
    snprintf(origin, origin_sz, "http://127.0.0.1:%u", mock->port);
    snprintf(discovery_json, discovery_sz,
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);
    mock->bodies[0] = discovery_json;
    mock->bodies[1] = self_user_json;
}

TEST
overwrite_append_builds_large_snowflake_and_bitmask_list(void)
{
    struct discord_overwrites overwrites = { 0 };
    const u64snowflake role_id = UINT64_C(9223372036854775901);
    const u64snowflake member_id = UINT64_C(9223372036854775902);
    const u64bitmask allow = UINT64_C(1) << 40;
    const u64bitmask deny = UINT64_C(1) << 41;

    discord_overwrite_append(&overwrites, role_id, 0, allow, 0);
    discord_overwrite_append(&overwrites, member_id, 1, 0, deny);

    ASSERT_EQ(2, overwrites.size);
    ASSERT_EQ(role_id, overwrites.array[0].id);
    ASSERT_EQ(0, overwrites.array[0].type);
    ASSERT_EQ(allow, overwrites.array[0].allow);
    ASSERT_EQ(0, overwrites.array[0].deny);
    ASSERT_EQ(member_id, overwrites.array[1].id);
    ASSERT_EQ(1, overwrites.array[1].type);
    ASSERT_EQ(0, overwrites.array[1].allow);
    ASSERT_EQ(deny, overwrites.array[1].deny);

    free(overwrites.array);
    PASS();
}

TEST
channel_crud_permissions_invites_typing_and_helpers(void)
{
    struct http_mock mock = { 0 };
    ASSERT_EQ(0, mock_open(&mock, 13));
    char origin[64], discovery_json[512];
    setup_init_responses(&mock, origin, sizeof(origin), discovery_json,
                         sizeof(discovery_json));

    static const char channel_json[] =
        "{\"id\":\"9223372036854775809\",\"type\":0,"
        "\"guild_id\":\"9223372036854775900\","
        "\"parent_id\":\"9223372036854775905\","
        "\"last_message_id\":\"9223372036854775906\","
        "\"name\":\"general\",\"topic\":\"welcome\"}";
    static const char modified_channel_json[] =
        "{\"id\":\"9223372036854775809\",\"type\":0,"
        "\"guild_id\":\"9223372036854775900\","
        "\"name\":\"renamed\",\"topic\":\"updated\"}";
    static const char guild_channels_json[] =
        "[{\"id\":\"9223372036854775910\",\"type\":2,\"name\":\"voice\"},"
        "{\"id\":\"9223372036854775911\",\"type\":0,\"name\":\"first\"},"
        "{\"id\":\"9223372036854775912\",\"type\":0,\"name\":\"second\"}]";
    static const char invite_json[] =
        "{\"code\":\"fluxer123\","
        "\"channel\":{\"id\":\"9223372036854775809\",\"type\":0,\"name\":\"general\"}}";
    static const char invite_list_json[] =
        "[{\"code\":\"fluxer123\","
        "\"channel\":{\"id\":\"9223372036854775809\",\"type\":0,\"name\":\"general\"}}]";

    mock.bodies[2] = channel_json;
    mock.bodies[3] = modified_channel_json;
    mock.statuses[4] = "204 No Content";
    mock.bodies[4] = "";
    mock.bodies[5] = guild_channels_json;
    mock.statuses[6] = "204 No Content";
    mock.bodies[6] = "";
    mock.statuses[7] = "204 No Content";
    mock.bodies[7] = "";
    mock.bodies[8] = invite_list_json;
    mock.bodies[9] = invite_json;
    mock.bodies[10] = invite_json;
    mock.statuses[11] = "204 No Content";
    mock.bodies[11] = "";
    mock.statuses[12] = "204 No Content";
    mock.bodies[12] = "";

    char config_path[] = "/tmp/concord-fluxer-channel-crud-XXXXXX";
    ASSERT(write_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    const u64snowflake channel_id = UINT64_C(9223372036854775809);
    const u64snowflake guild_id = UINT64_C(9223372036854775900);
    const u64snowflake overwrite_id = UINT64_C(9223372036854775920);
    const u64snowflake recipient_id = UINT64_C(9223372036854775930);

    /* 2: get_channel */
    struct discord_channel ch = { 0 };
    struct discord_ret_channel ch_ret = { .sync = &ch };
    ASSERT_EQ(CCORD_OK, discord_get_channel(client, channel_id, &ch_ret));
    ASSERT_EQ(channel_id, ch.id);
    ASSERT_EQ(guild_id, ch.guild_id);
    ASSERT_EQ(UINT64_C(9223372036854775905), ch.parent_id);
    ASSERT_EQ(UINT64_C(9223372036854775906), ch.last_message_id);
    ASSERT_STR_EQ("general", ch.name);
    ASSERT_STR_EQ("welcome", ch.topic);
    discord_channel_cleanup(&ch);

    /* 3: modify_channel */
    struct discord_modify_channel mod_params = {
        .reason = "rename channel",
        .name = "renamed",
        .topic = "updated",
    };
    memset(&ch, 0, sizeof(ch));
    ch_ret = (struct discord_ret_channel){ .sync = &ch };
    ASSERT_EQ(CCORD_OK,
              discord_modify_channel(client, channel_id, &mod_params, &ch_ret));
    ASSERT_EQ(channel_id, ch.id);
    ASSERT_STR_EQ("renamed", ch.name);
    ASSERT_STR_EQ("updated", ch.topic);
    discord_channel_cleanup(&ch);

    /* 4: delete_channel (Fluxer 204 No Content) */
    struct discord_delete_channel del_params = { .reason = "cleanup" };
    memset(&ch, 0, sizeof(ch));
    ch_ret = (struct discord_ret_channel){ .sync = &ch };
    ASSERT_EQ(CCORD_OK,
              discord_delete_channel(client, channel_id, &del_params, &ch_ret));
    ASSERT_EQ(0, ch.id);
    discord_channel_cleanup(&ch);

    /* 5: get_channel_at_pos */
    memset(&ch, 0, sizeof(ch));
    ch_ret = (struct discord_ret_channel){ .sync = &ch };
    ASSERT_EQ(CCORD_OK,
              discord_get_channel_at_pos(client, guild_id,
                                         DISCORD_CHANNEL_GUILD_TEXT, 1,
                                         &ch_ret));
    ASSERT_EQ(UINT64_C(9223372036854775912), ch.id);
    ASSERT_STR_EQ("second", ch.name);
    discord_channel_cleanup(&ch);

    /* 6: edit_channel_permissions */
    struct discord_edit_channel_permissions perm_params = {
        .reason = "grant member view",
        .allow = UINT64_C(1024),
        .deny = UINT64_C(2048),
        .type = 1,
    };
    struct discord_ret blank_ret = { .sync = true };
    ASSERT_EQ(CCORD_OK, discord_edit_channel_permissions(
                            client, channel_id, overwrite_id, &perm_params,
                            &blank_ret));

    /* 7: delete_channel_permission */
    struct discord_delete_channel_permission del_perm = {
        .reason = "revoke overwrite",
    };
    ASSERT_EQ(CCORD_OK, discord_delete_channel_permission(
                            client, channel_id, overwrite_id, &del_perm,
                            &blank_ret));

    /* 8: get_channel_invites */
    struct discord_invites invites = { 0 };
    struct discord_ret_invites invites_ret = { .sync = &invites };
    ASSERT_EQ(CCORD_OK,
              discord_get_channel_invites(client, channel_id, &invites_ret));
    ASSERT_EQ(1, invites.size);
    ASSERT_STR_EQ("fluxer123", invites.array[0].code);
    ASSERT(invites.array[0].channel != NULL);
    ASSERT_EQ(channel_id, invites.array[0].channel->id);
    discord_invites_cleanup(&invites);

    /* 9 & 10: create_channel_invite (with params and NULL params) */
    struct discord_create_channel_invite inv_params = {
        .reason = "invite link",
        .max_age = 3600,
        .max_uses = 5,
        .unique = true,
    };
    struct discord_invite invite = { 0 };
    struct discord_ret_invite invite_ret = { .sync = &invite };
    ASSERT_EQ(CCORD_OK, discord_create_channel_invite(
                            client, channel_id, &inv_params, &invite_ret));
    ASSERT_STR_EQ("fluxer123", invite.code);
    discord_invite_cleanup(&invite);

    memset(&invite, 0, sizeof(invite));
    invite_ret = (struct discord_ret_invite){ .sync = &invite };
    ASSERT_EQ(CCORD_OK,
              discord_create_channel_invite(client, channel_id, NULL,
                                            &invite_ret));
    ASSERT_STR_EQ("fluxer123", invite.code);
    discord_invite_cleanup(&invite);

    /* 11: trigger_typing_indicator */
    ASSERT_EQ(CCORD_OK,
              discord_trigger_typing_indicator(client, channel_id, &blank_ret));

    /* 12: group_dm_remove_recipient */
    ASSERT_EQ(CCORD_OK, discord_group_dm_remove_recipient(
                            client, channel_id, recipient_id, &blank_ret));

    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    ASSERT_EQ(13, mock.served);

    ASSERT(strstr(mock.requests[2],
                  "GET /api-root/v1/channels/9223372036854775809 ")
           != NULL);
    ASSERT(strstr(mock.requests[3],
                  "PATCH /api-root/v1/channels/9223372036854775809 ")
           != NULL);
    ASSERT(strstr(mock.requests[3], "X-Audit-Log-Reason: rename channel")
           != NULL);
    ASSERT(strstr(mock.requests[3], "\"name\":\"renamed\"") != NULL);
    ASSERT(strstr(mock.requests[3], "\"topic\":\"updated\"") != NULL);
    ASSERT(strstr(mock.requests[3], "\"archived\"") == NULL);
    ASSERT(strstr(mock.requests[3], "\"locked\"") == NULL);
    ASSERT(strstr(mock.requests[3], "\"invitable\"") == NULL);
    ASSERT(strstr(mock.requests[4],
                  "DELETE /api-root/v1/channels/9223372036854775809 ")
           != NULL);
    ASSERT(strstr(mock.requests[5],
                  "GET /api-root/v1/guilds/9223372036854775900/channels ")
           != NULL);
    ASSERT(strstr(mock.requests[6],
                  "PUT /api-root/v1/channels/9223372036854775809/permissions/9223372036854775920 ")
           != NULL);
    ASSERT(strstr(mock.requests[6], "\"allow\":\"1024\"") != NULL);
    ASSERT(strstr(mock.requests[6], "\"deny\":\"2048\"") != NULL);
    ASSERT(strstr(mock.requests[6], "\"type\":1") != NULL);
    ASSERT(strstr(mock.requests[7],
                  "DELETE /api-root/v1/channels/9223372036854775809/permissions/9223372036854775920 ")
           != NULL);
    ASSERT(strstr(mock.requests[8],
                  "GET /api-root/v1/channels/9223372036854775809/invites ")
           != NULL);
    ASSERT(strstr(mock.requests[9],
                  "POST /api-root/v1/channels/9223372036854775809/invites ")
           != NULL);
    ASSERT(strstr(mock.requests[9], "\"max_age\":3600") != NULL);
    ASSERT(strstr(mock.requests[9], "\"max_uses\":5") != NULL);
    ASSERT(strstr(mock.requests[9], "\"unique\":true") != NULL);
    ASSERT(strstr(mock.requests[10],
                  "POST /api-root/v1/channels/9223372036854775809/invites ")
           != NULL);
    ASSERT(strstr(mock.requests[10], "\r\n\r\n{}") != NULL);
    ASSERT(strstr(mock.requests[11],
                  "POST /api-root/v1/channels/9223372036854775809/typing ")
           != NULL);
    ASSERT(strstr(mock.requests[12],
                  "DELETE /api-root/v1/channels/9223372036854775809/recipients/9223372036854775930 ")
           != NULL);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

TEST
messages_bulk_delete_crosspost_follow_reactions_and_pins(void)
{
    struct http_mock mock = { 0 };
    ASSERT_EQ(0, mock_open(&mock, 16));
    char origin[64], discovery_json[512];
    setup_init_responses(&mock, origin, sizeof(origin), discovery_json,
                         sizeof(discovery_json));

    static const char messages_json[] =
        "[{\"id\":\"9223372036854775821\",\"channel_id\":\"9223372036854775809\","
        "\"content\":\"hello\"}]";
    static const char crossposted_json[] =
        "{\"id\":\"9223372036854775822\",\"channel_id\":\"9223372036854775809\","
        "\"content\":\"announcement\",\"flags\":\"1\"}";
    static const char followed_json[] =
        "{\"channel_id\":\"9223372036854775809\","
        "\"webhook_id\":\"9223372036854775831\"}";
    static const char reaction_users_json[] =
        "[{\"id\":\"9223372036854775845\",\"username\":\"alice\"}]";
    static const char pins_page_json[] =
        "{\"items\":[{\"pinned_at\":\"2026-11-11T00:00:00.000Z\","
        "\"message\":{\"id\":\"9223372036854775850\","
        "\"channel_id\":\"9223372036854775809\","
        "\"content\":\"pinned-note\",\"pinned\":true}}],"
        "\"has_more\":false}";

    mock.bodies[2] = messages_json;
    mock.statuses[3] = "204 No Content";
    mock.bodies[3] = "";
    mock.bodies[4] = crossposted_json;
    mock.bodies[5] = followed_json;
    mock.statuses[6] = "204 No Content";
    mock.bodies[6] = "";
    mock.statuses[7] = "204 No Content";
    mock.bodies[7] = "";
    mock.statuses[8] = "204 No Content";
    mock.bodies[8] = "";
    mock.bodies[9] = reaction_users_json;
    mock.bodies[10] = reaction_users_json;
    mock.statuses[11] = "204 No Content";
    mock.bodies[11] = "";
    mock.statuses[12] = "204 No Content";
    mock.bodies[12] = "";
    mock.bodies[13] = pins_page_json;
    mock.statuses[14] = "204 No Content";
    mock.bodies[14] = "";
    mock.statuses[15] = "204 No Content";
    mock.bodies[15] = "";

    char config_path[] = "/tmp/concord-fluxer-channel-msg-XXXXXX";
    ASSERT(write_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    const u64snowflake channel_id = UINT64_C(9223372036854775809);
    const u64snowflake message_id = UINT64_C(9223372036854775822);
    const u64snowflake emoji_id = UINT64_C(9223372036854775840);
    const u64snowflake user_id = UINT64_C(9223372036854775841);
    const u64snowflake pin_msg_id = UINT64_C(9223372036854775850);
    struct discord_ret blank_ret = { .sync = true };

    /* 2: get_channel_messages with query params > INT64_MAX */
    struct discord_get_channel_messages get_msgs = {
        .limit = 25,
        .after = UINT64_C(9223372036854775820),
    };
    struct discord_messages msgs = { 0 };
    struct discord_ret_messages msgs_ret = { .sync = &msgs };
    ASSERT_EQ(CCORD_OK, discord_get_channel_messages(client, channel_id,
                                                     &get_msgs, &msgs_ret));
    ASSERT_EQ(1, msgs.size);
    ASSERT_EQ(UINT64_C(9223372036854775821), msgs.array[0].id);
    discord_messages_cleanup(&msgs);

    /* 3: bulk_delete_messages (single message of arbitrary age / large snowflake) */
    u64snowflake bulk_ids[1] = { UINT64_C(9223372036854775821) };
    struct snowflakes id_list = { .size = 1, .array = bulk_ids };
    struct discord_bulk_delete_messages bulk_params = {
        .reason = "moderation",
        .messages = &id_list,
    };
    ASSERT_EQ(CCORD_OK, discord_bulk_delete_messages(
                            client, channel_id, &bulk_params, &blank_ret));

    /* 4: crosspost_message */
    struct discord_message crossposted = { 0 };
    struct discord_ret_message crosspost_ret = { .sync = &crossposted };
    ASSERT_EQ(CCORD_OK, discord_crosspost_message(
                            client, channel_id, message_id, &crosspost_ret));
    ASSERT_EQ(message_id, crossposted.id);
    ASSERT_EQ(1, crossposted.flags);
    discord_message_cleanup(&crossposted);

    /* 5: follow_news_channel */
    struct discord_follow_news_channel follow_params = {
        .webhook_channel_id = UINT64_C(9223372036854775830),
    };
    struct discord_followed_channel followed = { 0 };
    struct discord_ret_followed_channel follow_ret = { .sync = &followed };
    ASSERT_EQ(CCORD_OK, discord_follow_news_channel(
                            client, channel_id, &follow_params, &follow_ret));
    ASSERT_EQ(channel_id, followed.channel_id);
    ASSERT_EQ(UINT64_C(9223372036854775831), followed.webhook_id);
    discord_followed_channel_cleanup(&followed);

    /* 6-12: reactions */
    ASSERT_EQ(CCORD_OK,
              discord_create_reaction(client, channel_id, message_id, emoji_id,
                                      "flux", &blank_ret));
    ASSERT_EQ(CCORD_OK,
              discord_delete_own_reaction(client, channel_id, message_id, 0,
                                          "+", &blank_ret));
    ASSERT_EQ(CCORD_OK,
              discord_delete_user_reaction(client, channel_id, message_id,
                                           user_id, emoji_id, "flux",
                                           &blank_ret));

    struct discord_get_reactions react_limit_only = { .limit = 10 };
    struct discord_users rusers = { 0 };
    struct discord_ret_users rusers_ret = { .sync = &rusers };
    ASSERT_EQ(CCORD_OK,
              discord_get_reactions(client, channel_id, message_id, emoji_id,
                                    "flux", &react_limit_only, &rusers_ret));
    ASSERT_EQ(1, rusers.size);
    ASSERT_EQ(UINT64_C(9223372036854775845), rusers.array[0].id);
    discord_users_cleanup(&rusers);

    struct discord_get_reactions react_paged = {
        .after = UINT64_C(9223372036854775842),
        .limit = 10,
    };
    memset(&rusers, 0, sizeof(rusers));
    rusers_ret = (struct discord_ret_users){ .sync = &rusers };
    ASSERT_EQ(CCORD_OK,
              discord_get_reactions(client, channel_id, message_id, emoji_id,
                                    "flux", &react_paged, &rusers_ret));
    ASSERT_EQ(1, rusers.size);
    discord_users_cleanup(&rusers);

    ASSERT_EQ(CCORD_OK,
              discord_delete_all_reactions(client, channel_id, message_id,
                                           &blank_ret));
    ASSERT_EQ(CCORD_OK, discord_delete_all_reactions_for_emoji(
                            client, channel_id, message_id, emoji_id, "flux",
                            &blank_ret));

    /* 13-15: pins */
    struct discord_messages pins = { 0 };
    struct discord_ret_messages pins_ret = { .sync = &pins };
    ASSERT_EQ(CCORD_OK,
              discord_get_pinned_messages(client, channel_id, &pins_ret));
    ASSERT_EQ(1, pins.size);
    ASSERT_EQ(pin_msg_id, pins.array[0].id);
    ASSERT_EQ(channel_id, pins.array[0].channel_id);
    ASSERT(pins.array[0].pinned);
    ASSERT_STR_EQ("pinned-note", pins.array[0].content);
    discord_messages_cleanup(&pins);

    struct discord_pin_message pin_params = { .reason = "important" };
    ASSERT_EQ(CCORD_OK, discord_pin_message(client, channel_id, pin_msg_id,
                                            &pin_params, &blank_ret));
    struct discord_unpin_message unpin_params = { .reason = "outdated" };
    ASSERT_EQ(CCORD_OK, discord_unpin_message(client, channel_id, pin_msg_id,
                                              &unpin_params, &blank_ret));

    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    ASSERT_EQ(16, mock.served);

    ASSERT(strstr(mock.requests[2],
                  "GET /api-root/v1/channels/9223372036854775809/messages?limit=25&after=9223372036854775820 ")
           != NULL);
    ASSERT(strstr(mock.requests[3],
                  "POST /api-root/v1/channels/9223372036854775809/messages/bulk-delete ")
           != NULL);
    ASSERT(strstr(mock.requests[3],
                  "\"messages\":[\"9223372036854775821\"]")
           != NULL);
    ASSERT(strstr(mock.requests[4],
                  "POST /api-root/v1/channels/9223372036854775809/messages/9223372036854775822/crosspost ")
           != NULL);
    ASSERT(strstr(mock.requests[5],
                  "POST /api-root/v1/channels/9223372036854775809/followers ")
           != NULL);
    ASSERT(strstr(mock.requests[5],
                  "\"webhook_channel_id\":\"9223372036854775830\"")
           != NULL);
    ASSERT(strstr(mock.requests[6],
                  "PUT /api-root/v1/channels/9223372036854775809/messages/9223372036854775822/reactions/flux:9223372036854775840/@me ")
           != NULL);
    ASSERT(strstr(mock.requests[7],
                  "DELETE /api-root/v1/channels/9223372036854775809/messages/9223372036854775822/reactions/%2B/@me ")
           != NULL);
    ASSERT(strstr(mock.requests[8],
                  "DELETE /api-root/v1/channels/9223372036854775809/messages/9223372036854775822/reactions/flux:9223372036854775840/9223372036854775841 ")
           != NULL);
    ASSERT(strstr(mock.requests[9],
                  "GET /api-root/v1/channels/9223372036854775809/messages/9223372036854775822/reactions/flux:9223372036854775840?limit=10 ")
           != NULL);
    ASSERT(strstr(mock.requests[10],
                  "GET /api-root/v1/channels/9223372036854775809/messages/9223372036854775822/reactions/flux:9223372036854775840?after=9223372036854775842&limit=10 ")
           != NULL);
    ASSERT(strstr(mock.requests[11],
                  "DELETE /api-root/v1/channels/9223372036854775809/messages/9223372036854775822/reactions ")
           != NULL);
    ASSERT(strstr(mock.requests[12],
                  "DELETE /api-root/v1/channels/9223372036854775809/messages/9223372036854775822/reactions/flux:9223372036854775840 ")
           != NULL);
    ASSERT(strstr(mock.requests[13],
                  "GET /api-root/v1/channels/9223372036854775809/messages/pins ")
           != NULL);
    ASSERT(strstr(mock.requests[14],
                  "PUT /api-root/v1/channels/9223372036854775809/pins/9223372036854775850 ")
           != NULL);
    ASSERT(strstr(mock.requests[15],
                  "DELETE /api-root/v1/channels/9223372036854775809/pins/9223372036854775850 ")
           != NULL);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

TEST
thread_operations_and_archived_thread_cursors(void)
{
    struct http_mock mock = { 0 };
    ASSERT_EQ(0, mock_open(&mock, 12));
    char origin[64], discovery_json[512];
    setup_init_responses(&mock, origin, sizeof(origin), discovery_json,
                         sizeof(discovery_json));

    static const char thread_json[] =
        "{\"id\":\"9223372036854775860\",\"type\":11,"
        "\"parent_id\":\"9223372036854775809\",\"name\":\"discussion\"}";
    static const char thread_members_json[] =
        "[{\"id\":\"9223372036854775860\","
        "\"user_id\":\"9223372036854775861\","
        "\"join_timestamp\":\"2026-11-11T00:00:00.000Z\",\"flags\":0}]";
    static const char archived_threads_json[] =
        "{\"threads\":[{\"id\":\"9223372036854775860\",\"type\":11,\"name\":\"old-thread\"}],"
        "\"members\":[{\"id\":\"9223372036854775860\",\"user_id\":\"9223372036854775861\","
        "\"join_timestamp\":\"2026-11-11T00:00:00.000Z\",\"flags\":0}],"
        "\"has_more\":true}";

    mock.bodies[2] = thread_json;
    mock.bodies[3] = thread_json;
    mock.statuses[4] = "204 No Content";
    mock.bodies[4] = "";
    mock.statuses[5] = "204 No Content";
    mock.bodies[5] = "";
    mock.statuses[6] = "204 No Content";
    mock.bodies[6] = "";
    mock.statuses[7] = "204 No Content";
    mock.bodies[7] = "";
    mock.bodies[8] = thread_members_json;
    mock.bodies[9] = archived_threads_json;
    mock.bodies[10] = archived_threads_json;
    mock.bodies[11] = archived_threads_json;

    char config_path[] = "/tmp/concord-fluxer-channel-threads-XXXXXX";
    ASSERT(write_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    const u64snowflake channel_id = UINT64_C(9223372036854775809);
    const u64snowflake message_id = UINT64_C(9223372036854775822);
    const u64snowflake thread_id = UINT64_C(9223372036854775860);
    const u64snowflake member_id = UINT64_C(9223372036854775861);
    struct discord_ret blank_ret = { .sync = true };

    /* 2: start_thread_with_message */
    struct discord_start_thread_with_message start_msg = {
        .name = "discussion",
        .auto_archive_duration = 1440,
    };
    struct discord_channel thread = { 0 };
    struct discord_ret_channel thread_ret = { .sync = &thread };
    ASSERT_EQ(CCORD_OK,
              discord_start_thread_with_message(client, channel_id, message_id,
                                                &start_msg, &thread_ret));
    ASSERT_EQ(thread_id, thread.id);
    discord_channel_cleanup(&thread);

    /* 3: start_thread_without_message */
    struct discord_start_thread_without_message start_standalone = {
        .name = "discussion",
        .auto_archive_duration = 1440,
        .type = DISCORD_CHANNEL_GUILD_PUBLIC_THREAD,
    };
    memset(&thread, 0, sizeof(thread));
    thread_ret = (struct discord_ret_channel){ .sync = &thread };
    ASSERT_EQ(CCORD_OK,
              discord_start_thread_without_message(
                  client, channel_id, &start_standalone, &thread_ret));
    ASSERT_EQ(thread_id, thread.id);
    discord_channel_cleanup(&thread);

    /* 4-7: join/leave/add/remove thread member */
    ASSERT_EQ(CCORD_OK, discord_join_thread(client, thread_id, &blank_ret));
    ASSERT_EQ(CCORD_OK, discord_leave_thread(client, thread_id, &blank_ret));
    ASSERT_EQ(CCORD_OK, discord_add_thread_member(client, thread_id, member_id,
                                                  &blank_ret));
    ASSERT_EQ(CCORD_OK, discord_remove_thread_member(
                            client, thread_id, member_id, &blank_ret));

    /* 8: list_thread_members */
    struct discord_thread_members members = { 0 };
    struct discord_ret_thread_members members_ret = { .sync = &members };
    ASSERT_EQ(CCORD_OK,
              discord_list_thread_members(client, thread_id, &members_ret));
    ASSERT_EQ(1, members.size);
    ASSERT_EQ(thread_id, members.array[0].id);
    ASSERT_EQ(member_id, members.array[0].user_id);
    discord_thread_members_cleanup(&members);

    /* 9-11: archived thread listings */
    const u64unix_ms before_ts = UINT64_C(1793882096789);
    struct discord_thread_response_body archived = { 0 };
    struct discord_ret_thread_response_body archived_ret = {
        .sync = &archived,
    };
    ASSERT_EQ(CCORD_OK, discord_list_public_archived_threads(
                            client, channel_id, before_ts, 10, &archived_ret));
    ASSERT(archived.has_more);
    ASSERT(archived.threads != NULL);
    ASSERT_EQ(1, archived.threads->size);
    ASSERT_EQ(thread_id, archived.threads->array[0].id);
    discord_thread_response_body_cleanup(&archived);

    memset(&archived, 0, sizeof(archived));
    archived_ret =
        (struct discord_ret_thread_response_body){ .sync = &archived };
    ASSERT_EQ(CCORD_OK, discord_list_private_archived_threads(
                            client, channel_id, before_ts, 5, &archived_ret));
    ASSERT(archived.has_more);
    discord_thread_response_body_cleanup(&archived);

    const u64snowflake before_snowflake = UINT64_C(9223372036854775899);
    memset(&archived, 0, sizeof(archived));
    archived_ret =
        (struct discord_ret_thread_response_body){ .sync = &archived };
    ASSERT_EQ(CCORD_OK,
              discord_list_joined_private_archived_threads(
                  client, channel_id, before_snowflake, 5, &archived_ret));
    ASSERT(archived.has_more);
    discord_thread_response_body_cleanup(&archived);

    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    ASSERT_EQ(12, mock.served);

    ASSERT(strstr(mock.requests[2],
                  "POST /api-root/v1/channels/9223372036854775809/messages/9223372036854775822/threads ")
           != NULL);
    ASSERT(strstr(mock.requests[3],
                  "POST /api-root/v1/channels/9223372036854775809/threads ")
           != NULL);
    ASSERT(strstr(mock.requests[3], "\"type\":11") != NULL);
    ASSERT(strstr(mock.requests[3], "\"invitable\"") == NULL);
    ASSERT(strstr(mock.requests[4],
                  "PUT /api-root/v1/channels/9223372036854775860/thread-members/@me ")
           != NULL);
    ASSERT(strstr(mock.requests[5],
                  "DELETE /api-root/v1/channels/9223372036854775860/thread-members/@me ")
           != NULL);
    ASSERT(strstr(mock.requests[6],
                  "PUT /api-root/v1/channels/9223372036854775860/thread-members/9223372036854775861 ")
           != NULL);
    ASSERT(strstr(mock.requests[7],
                  "DELETE /api-root/v1/channels/9223372036854775860/thread-members/9223372036854775861 ")
           != NULL);
    ASSERT(strstr(mock.requests[8],
                  "GET /api-root/v1/channels/9223372036854775860/thread-members ")
           != NULL);
    ASSERT(strstr(mock.requests[9],
                  "GET /api-root/v1/channels/9223372036854775809/threads/archived/public?before=2026-11-05T12:34:56.789Z&limit=10 ")
           != NULL);
    ASSERT(strstr(mock.requests[10],
                  "GET /api-root/v1/channels/9223372036854775809/threads/archived/private?before=2026-11-05T12:34:56.789Z&limit=5 ")
           != NULL);
    ASSERT(strstr(mock.requests[11],
                  "GET /api-root/v1/channels/9223372036854775809/users/@me/threads/archived/private?before=9223372036854775899&limit=5 ")
           != NULL);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

TEST
unsupported_group_dm_add_recipient_returns_unsupported_without_network_traffic(
    void)
{
    struct http_mock mock = { .keep_listening = true };
    ASSERT_EQ(0, mock_open(&mock, 2));
    char origin[64], discovery_json[512];
    setup_init_responses(&mock, origin, sizeof(origin), discovery_json,
                         sizeof(discovery_json));

    char config_path[] = "/tmp/concord-fluxer-channel-unsup-XXXXXX";
    ASSERT(write_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);
    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    ASSERT_EQ(2, mock.served);

    struct unsupported_context ctx = { 0 };
    struct discord_group_dm_add_recipient add_params = {
        .access_token = "oauth-token",
        .nick = "friend",
    };
    struct discord_ret ret = {
        .data = &ctx,
        .keep = &ctx,
        .fail = &on_unsupported,
    };
    ASSERT_EQ(CCORD_UNSUPPORTED,
              discord_group_dm_add_recipient(
                  client, UINT64_C(9223372036854775809),
                  UINT64_C(9223372036854775890), &add_params, &ret));
    ASSERT_EQ(1, ctx.calls);
    ASSERT_EQ(CCORD_UNSUPPORTED, ctx.code);
    ASSERT_EQ(&ctx, ctx.data);
    ASSERT_EQ(&ctx, ctx.keep);

    ASSERT_EQ(CCORD_UNSUPPORTED,
              discord_group_dm_add_recipient(
                  client, UINT64_C(9223372036854775809),
                  UINT64_C(9223372036854775890), NULL, NULL));

    struct pollfd pfd = { .fd = mock.fd, .events = POLLIN };
    ASSERT_EQ(0, poll(&pfd, 1, 50));
    close(mock.fd);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

TEST
embed_and_reply_serialization_omits_unset_fields(void)
{
    struct discord_embed embed = {
        .title = "Titulo",
        .description = "Descricao",
        .color = 0xC27C0E,
    };
    struct discord_create_message embed_msg = {
        .embeds =
            &(struct discord_embeds){
                .size = 1,
                .array = &embed,
            },
    };
    char *json = NULL;
    size_t json_sz = 0;
    ASSERT_EQ(CCORD_OK,
              discord_create_message_to_json(&json, &json_sz, &embed_msg));
    ASSERT(json != NULL);
    ASSERT(strstr(json, "\"title\":\"Titulo\"") != NULL);
    ASSERT(strstr(json, "\"description\":\"Descricao\"") != NULL);
    ASSERT(strstr(json, "\"color\":12745742") != NULL);
    ASSERT(strstr(json, "\"url\"") == NULL);
    ASSERT(strstr(json, "\"type\"") == NULL);
    ASSERT(strstr(json, "\"content\"") == NULL);
    free(json);

    struct discord_message_reference mref = {
        .message_id = UINT64_C(9223372036854775810),
        .channel_id = UINT64_C(1565385514767360000),
        .fail_if_not_exists = true,
    };
    struct discord_create_message reply_msg = {
        .content = "oi coelho",
        .message_reference = &mref,
    };
    json = NULL;
    json_sz = 0;
    ASSERT_EQ(CCORD_OK,
              discord_create_message_to_json(&json, &json_sz, &reply_msg));
    ASSERT(json != NULL);
    ASSERT(strstr(json, "\"content\":\"oi coelho\"") != NULL);
    ASSERT(strstr(json, "\"message_id\":\"9223372036854775810\"") != NULL);
    ASSERT(strstr(json, "\"channel_id\":\"1565385514767360000\"") != NULL);
    ASSERT(strstr(json, "\"guild_id\"") == NULL);
    ASSERT(strstr(json, "\"embeds\"") == NULL);
    free(json);

    PASS();
}

GREATEST_MAIN_DEFS();

int
main(int argc, char *argv[])
{
    GREATEST_MAIN_BEGIN();

    RUN_TEST(overwrite_append_builds_large_snowflake_and_bitmask_list);
    RUN_TEST(channel_crud_permissions_invites_typing_and_helpers);
    RUN_TEST(messages_bulk_delete_crosspost_follow_reactions_and_pins);
    RUN_TEST(thread_operations_and_archived_thread_cursors);
    RUN_TEST(
        unsupported_group_dm_add_recipient_returns_unsupported_without_network_traffic);
    RUN_TEST(embed_and_reply_serialization_omits_unset_fields);

    GREATEST_MAIN_END();
}
