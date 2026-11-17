#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "discord.h"
#include "discord-internal.h"
#include "greatest.h"

#define MAX_MOCK_REQUESTS 24
#define MOCK_BUFFER_SIZE  8192

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
    size_t expected_requests;
    size_t received_requests;
    const char *statuses[MAX_MOCK_REQUESTS];
    const char *bodies[MAX_MOCK_REQUESTS];
    char requests[MAX_MOCK_REQUESTS][MOCK_BUFFER_SIZE];
};

static int
mock_open(struct http_mock *mock, size_t expected_requests)
{
    struct sockaddr_in address = { .sin_family = AF_INET,
                                   .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
                                   .sin_port = 0 };
    socklen_t length = sizeof(address);
    mock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (mock->fd < 0) return -1;
    if (bind(mock->fd, (struct sockaddr *)&address, sizeof(address)) < 0
        || listen(mock->fd, (int)expected_requests + 4) < 0
        || getsockname(mock->fd, (struct sockaddr *)&address, &length) < 0)
    {
        close(mock->fd);
        return -1;
    }
    mock->port = ntohs(address.sin_port);
    mock->expected_requests = expected_requests;
    mock->received_requests = 0;
    memset(mock->requests, 0, sizeof(mock->requests));
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
            return (size_t)strtoul(line + sizeof("Content-Length:") - 1, NULL,
                                   10);
        }
        line = end + 2;
    }
    return 0;
}

static void *
mock_serve(void *data)
{
    struct http_mock *mock = data;
    for (size_t i = 0; i < mock->expected_requests; ++i) {
        int connection = accept(mock->fd, NULL, NULL);
        if (connection < 0) break;
        ++mock->received_requests;

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
                expected_size =
                    header_size + request_content_length(request, header_size);
            }
            if (header_size && used >= expected_size) break;
        }

        const char *body = mock->bodies[i] ? mock->bodies[i] : "";
        const char *status = mock->statuses[i] ? mock->statuses[i] : "200 OK";
        char response[MOCK_BUFFER_SIZE];
        int response_size = snprintf(
            response, sizeof(response),
            "HTTP/1.1 %s\r\nContent-Type: application/json\r\n"
            "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
            status, strlen(body), body);
        if (response_size > 0 && (size_t)response_size < sizeof(response))
            (void)send(connection, response, (size_t)response_size, 0);
        close(connection);
    }
    close(mock->fd);
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

static const char *const k_init_user_json =
    "{\"id\":\"9223372036854775808\",\"username\":\"modbot\",\"bot\":true}";

#define ASSERT_UNSUPPORTED_CALL(expr, ctx)                                    \
    do {                                                                      \
        memset(&(ctx), 0, sizeof(ctx));                                       \
        ASSERT_EQ(CCORD_UNSUPPORTED, (expr));                                 \
        ASSERT_EQ(1, (ctx).calls);                                            \
        ASSERT_EQ(CCORD_UNSUPPORTED, (ctx).code);                             \
        ASSERT_EQ(&(ctx), (ctx).data);                                        \
        ASSERT_EQ(&(ctx), (ctx).keep);                                        \
    } while (0)

TEST
unsupported_moderation_pack_and_template_calls_emit_no_network_traffic(void)
{
    struct http_mock mock = { 0 };
    ASSERT_EQ(0, mock_open(&mock, 2));

    char origin[64], discovery_json[512];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", mock.port);
    snprintf(discovery_json, sizeof(discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);
    mock.statuses[0] = "200 OK";
    mock.bodies[0] = discovery_json;
    mock.statuses[1] = "200 OK";
    mock.bodies[1] = k_init_user_json;

    char config_path[] = "/tmp/concord-fluxer-mod-unsup-XXXXXX";
    ASSERT(write_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);
    /* Wait for mock server to finish serving the 2 startup requests; any
     * subsequent HTTP call would fail to connect rather than hitting network. */
    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    ASSERT_EQ(2, mock.received_requests);

    const u64snowflake guild_id = UINT64_C(9223372036854775809);
    const u64snowflake rule_id = UINT64_C(9223372036854775810);
    struct unsupported_context ctx = { 0 };

    /* Auto-moderation (5 functions) */
    struct discord_ret_auto_moderation_rules rules_ret = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(discord_list_auto_moderation_rules_for_guild(
                                client, guild_id, &rules_ret),
                            ctx);
    ASSERT_EQ(CCORD_UNSUPPORTED,
              discord_list_auto_moderation_rules_for_guild(client, 0, NULL));

    struct discord_ret_auto_moderation_rule rule_ret = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_get_auto_moderation_rule(client, guild_id, rule_id, &rule_ret),
        ctx);
    ASSERT_UNSUPPORTED_CALL(
        discord_create_auto_moderation_rule(client, guild_id, NULL, &rule_ret),
        ctx);
    ASSERT_UNSUPPORTED_CALL(discord_modify_auto_moderation_rule(
                                client, guild_id, rule_id, NULL, &rule_ret),
                            ctx);

    struct discord_ret blank_ret = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(discord_delete_auto_moderation_rule(
                                client, guild_id, rule_id, NULL, &blank_ret),
                            ctx);

    /* Nitro sticker packs (1 function) */
    struct discord_ret_list_nitro_sticker_packs packs_ret = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_list_nitro_sticker_packs(client, &packs_ret), ctx);
    ASSERT_EQ(CCORD_UNSUPPORTED,
              discord_list_nitro_sticker_packs(client, NULL));

    /* Guild templates (7 functions) */
    struct discord_ret_guild_template tmpl_ret = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    struct discord_ret_guild_templates tmpls_ret = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    struct discord_ret_guild guild_ret = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };

    ASSERT_UNSUPPORTED_CALL(
        discord_get_guild_template(client, "tmpl123", &tmpl_ret), ctx);
    ASSERT_UNSUPPORTED_CALL(discord_create_guild_from_guild_template(
                                client, "tmpl123", NULL, &guild_ret),
                            ctx);
    ASSERT_UNSUPPORTED_CALL(
        discord_get_guild_templates(client, guild_id, &tmpls_ret), ctx);
    ASSERT_UNSUPPORTED_CALL(
        discord_create_guild_template(client, guild_id, NULL, &tmpl_ret), ctx);
    ASSERT_UNSUPPORTED_CALL(
        discord_sync_guild_template(client, guild_id, "tmpl123", &tmpl_ret),
        ctx);
    ASSERT_UNSUPPORTED_CALL(discord_modify_guild_template(
                                client, guild_id, "tmpl123", NULL, &tmpl_ret),
                            ctx);
    ASSERT_UNSUPPORTED_CALL(
        discord_delete_guild_template(client, guild_id, "tmpl123", &tmpl_ret),
        ctx);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

TEST
audit_log_uses_fluxer_route_query_params_and_decodes_large_snowflakes(void)
{
    static const char audit_json[] =
        "{"
        "\"audit_log_entries\":[{"
        "\"id\":\"9223372036854775900\","
        "\"user_id\":\"9223372036854775901\","
        "\"target_id\":\"9223372036854775902\","
        "\"action_type\":60,"
        "\"reason\":\"uploaded mascot emoji\","
        "\"options\":{"
        "\"channel_id\":\"9223372036854775903\","
        "\"count\":3,"
        "\"type\":0,"
        "\"role_name\":\"Mods\""
        "},"
        "\"changes\":[{"
        "\"key\":\"name\","
        "\"old_value\":\"old_wave\","
        "\"new_value\":\"wave\""
        "}]"
        "}],"
        "\"users\":[{\"id\":\"9223372036854775901\",\"username\":\"alice\"}],"
        "\"webhooks\":[{\"id\":\"9223372036854775904\",\"name\":\"hook\"}]"
        "}";

    struct http_mock mock = { 0 };
    ASSERT_EQ(0, mock_open(&mock, 3));

    char origin[64], discovery_json[512];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", mock.port);
    snprintf(discovery_json, sizeof(discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);
    mock.bodies[0] = discovery_json;
    mock.bodies[1] = k_init_user_json;
    mock.bodies[2] = audit_json;

    char config_path[] = "/tmp/concord-fluxer-audit-XXXXXX";
    ASSERT(write_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    const u64snowflake guild_id = UINT64_C(9223372036854775809);
    struct discord_get_guild_audit_log params = {
        .user_id = UINT64_C(9223372036854775901),
        .action_type = DISCORD_AUDIT_LOG_EMOJI_CREATE,
        .before = UINT64_C(9223372036854775999),
        .after = UINT64_C(9223372036854775850),
        .limit = 25,
    };
    struct discord_audit_log log = { 0 };
    struct discord_ret_audit_log ret = { .sync = &log };
    ASSERT_EQ(CCORD_OK,
              discord_get_guild_audit_log(client, guild_id, &params, &ret));

    ASSERT(log.audit_log_entries != NULL);
    ASSERT_EQ(1, log.audit_log_entries->size);
    ASSERT_EQ(UINT64_C(9223372036854775900),
              log.audit_log_entries->array[0].id);
    ASSERT_EQ(UINT64_C(9223372036854775901),
              log.audit_log_entries->array[0].user_id);
    ASSERT_EQ(UINT64_C(9223372036854775902),
              log.audit_log_entries->array[0].target_id);
    ASSERT_EQ(DISCORD_AUDIT_LOG_EMOJI_CREATE,
              log.audit_log_entries->array[0].action_type);
    ASSERT_STR_EQ("uploaded mascot emoji",
                  log.audit_log_entries->array[0].reason);

    ASSERT(log.audit_log_entries->array[0].options != NULL);
    ASSERT_EQ(UINT64_C(9223372036854775903),
              log.audit_log_entries->array[0].options->channel_id);
    ASSERT_STR_EQ("3", log.audit_log_entries->array[0].options->count);
    ASSERT_STR_EQ("0", log.audit_log_entries->array[0].options->type);
    ASSERT_STR_EQ("Mods", log.audit_log_entries->array[0].options->role_name);

    ASSERT(log.audit_log_entries->array[0].changes != NULL);
    ASSERT_EQ(1, log.audit_log_entries->array[0].changes->size);
    ASSERT_STR_EQ("name",
                  log.audit_log_entries->array[0].changes->array[0].key);
    ASSERT_STR_EQ("wave",
                  log.audit_log_entries->array[0].changes->array[0].new_value);

    ASSERT(log.users != NULL);
    ASSERT_EQ(1, log.users->size);
    ASSERT_EQ(UINT64_C(9223372036854775901), log.users->array[0].id);
    ASSERT(log.webhooks != NULL);
    ASSERT_EQ(1, log.webhooks->size);
    ASSERT_EQ(UINT64_C(9223372036854775904), log.webhooks->array[0].id);

    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    ASSERT(strstr(mock.requests[2],
                  "GET /api-root/v1/guilds/9223372036854775809/audit-logs"
                  "?user_id=9223372036854775901"
                  "&action_type=60"
                  "&before=9223372036854775999"
                  "&after=9223372036854775850"
                  "&limit=25 ")
           != NULL);
    ASSERT(strstr(mock.requests[2], "Authorization: Bot ") != NULL);

    discord_audit_log_cleanup(&log);
    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

TEST
emoji_sticker_and_invite_endpoints_match_fluxer_v1_protocol(void)
{
    static const char emoji_list_json[] =
        "[{"
        "\"id\":\"9223372036854775910\","
        "\"name\":\"flux_wave\","
        "\"animated\":true,"
        "\"user\":{\"id\":\"9223372036854775911\",\"username\":\"uploader\"}"
        "}]";
    static const char emoji_created_json[] =
        "{"
        "\"id\":\"9223372036854775912\","
        "\"name\":\"party_blob\","
        "\"animated\":false"
        "}";
    static const char emoji_modified_json[] =
        "{"
        "\"id\":\"9223372036854775912\","
        "\"name\":\"party_wave\","
        "\"animated\":false"
        "}";
    static const char sticker_meta_json[] =
        "{"
        "\"id\":\"9223372036854775920\","
        "\"guild_id\":\"9223372036854775809\","
        "\"name\":\"coelho_hi\","
        "\"animated\":true,"
        "\"allow_cloning\":false"
        "}";
    static const char sticker_list_json[] =
        "[{"
        "\"id\":\"9223372036854775920\","
        "\"name\":\"coelho_hi\","
        "\"description\":\"A friendly bunny\","
        "\"tags\":[\"wave\",\"hello\"],"
        "\"animated\":true,"
        "\"user\":{\"id\":\"9223372036854775911\",\"username\":\"uploader\"}"
        "}]";
    static const char sticker_modified_json[] =
        "{"
        "\"id\":\"9223372036854775920\","
        "\"name\":\"coelho_yay\","
        "\"description\":\"Updated bunny\","
        "\"tags\":[\"yay\",\"bunny\"],"
        "\"animated\":true"
        "}";
    static const char invite_json[] =
        "{"
        "\"code\":\"FluxerInvite42\","
        "\"type\":0,"
        "\"temporary\":true,"
        "\"member_count\":128,"
        "\"presence_count\":37,"
        "\"expires_at\":\"2026-12-01T00:00:00.000Z\","
        "\"guild\":{\"id\":\"9223372036854775809\",\"name\":\"Fluxer Hub\"},"
        "\"channel\":{\"id\":\"9223372036854775930\",\"type\":0,\"name\":\"general\"},"
        "\"inviter\":{\"id\":\"9223372036854775931\",\"username\":\"host\"}"
        "}";

    /* 2 init + 6 emoji + 6 sticker + 3 invite = 17 HTTP requests */
    struct http_mock mock = { 0 };
    ASSERT_EQ(0, mock_open(&mock, 17));

    char origin[64], discovery_json[512];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", mock.port);
    snprintf(discovery_json, sizeof(discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);

    mock.bodies[0] = discovery_json;
    mock.bodies[1] = k_init_user_json;
    /* Emoji requests: 2..7 */
    mock.bodies[2] = emoji_list_json;     /* list_guild_emojis */
    mock.bodies[3] = emoji_list_json;     /* get_guild_emoji (found) */
    mock.bodies[4] = emoji_list_json;     /* get_guild_emoji (missing) */
    mock.bodies[5] = emoji_created_json;  /* create_guild_emoji */
    mock.bodies[6] = emoji_modified_json; /* modify_guild_emoji */
    mock.statuses[7] = "204 No Content";  /* delete_guild_emoji */
    mock.bodies[7] = "";
    /* Sticker requests: 8..13 */
    mock.bodies[8] = sticker_meta_json;     /* get_sticker (/metadata) */
    mock.bodies[9] = sticker_list_json;     /* list_guild_stickers */
    mock.bodies[10] = sticker_list_json;    /* get_guild_sticker (found) */
    mock.bodies[11] = sticker_list_json;    /* get_guild_sticker (missing) */
    mock.bodies[12] = sticker_modified_json;/* modify_guild_sticker */
    mock.statuses[13] = "204 No Content";   /* delete_guild_sticker */
    mock.bodies[13] = "";
    /* Invite requests: 14..16 */
    mock.bodies[14] = invite_json;          /* get_invite (NULL params) */
    mock.bodies[15] = invite_json;          /* get_invite (with params) */
    mock.statuses[16] = "204 No Content";   /* delete_invite */
    mock.bodies[16] = "";

    char config_path[] = "/tmp/concord-fluxer-assets-XXXXXX";
    ASSERT(write_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    const u64snowflake guild_id = UINT64_C(9223372036854775809);
    const u64snowflake emoji_id = UINT64_C(9223372036854775910);
    const u64snowflake created_emoji_id = UINT64_C(9223372036854775912);
    const u64snowflake sticker_id = UINT64_C(9223372036854775920);

    /* 1. List guild emojis */
    struct discord_emojis emojis = { 0 };
    struct discord_ret_emojis emojis_ret = { .sync = &emojis };
    ASSERT_EQ(CCORD_OK,
              discord_list_guild_emojis(client, guild_id, &emojis_ret));
    ASSERT_EQ(1, emojis.size);
    ASSERT_EQ(emoji_id, emojis.array[0].id);
    ASSERT_STR_EQ("flux_wave", emojis.array[0].name);
    ASSERT_EQ(true, emojis.array[0].animated);
    ASSERT(emojis.array[0].user != NULL);
    ASSERT_EQ(UINT64_C(9223372036854775911), emojis.array[0].user->id);
    discord_emojis_cleanup(&emojis);

    /* 2. Get guild emoji (adapted over GET /guilds/{id}/emojis) */
    struct discord_emoji emoji = { 0 };
    struct discord_ret_emoji emoji_ret = { .sync = &emoji };
    ASSERT_EQ(CCORD_OK,
              discord_get_guild_emoji(client, guild_id, emoji_id, &emoji_ret));
    ASSERT_EQ(emoji_id, emoji.id);
    ASSERT_STR_EQ("flux_wave", emoji.name);
    ASSERT_EQ(true, emoji.animated);
    ASSERT(emoji.user != NULL);
    ASSERT_EQ(UINT64_C(9223372036854775911), emoji.user->id);
    discord_emoji_cleanup(&emoji);

    struct unsupported_context missing_emoji_ctx = { 0 };
    struct discord_emoji missing_emoji = { 0 };
    struct discord_ret_emoji missing_emoji_ret = {
        .data = &missing_emoji_ctx,
        .keep = &missing_emoji_ctx,
        .fail = &on_unsupported,
        .sync = &missing_emoji,
    };
    ASSERT_EQ(CCORD_RESOURCE_UNAVAILABLE,
              discord_get_guild_emoji(client, guild_id,
                                      UINT64_C(9223372036854775999),
                                      &missing_emoji_ret));
    ASSERT_EQ(1, missing_emoji_ctx.calls);
    ASSERT_EQ(CCORD_RESOURCE_UNAVAILABLE, missing_emoji_ctx.code);

    /* 3. Create guild emoji */
    struct discord_create_guild_emoji create_emoji = {
        .reason = "Add blob",
        .name = "party_blob",
        .image = "data:image/png;base64,AAAA",
    };
    struct discord_emoji created_emoji = { 0 };
    emoji_ret = (struct discord_ret_emoji){ .sync = &created_emoji };
    ASSERT_EQ(CCORD_OK, discord_create_guild_emoji(
                            client, guild_id, &create_emoji, &emoji_ret));
    ASSERT_EQ(created_emoji_id, created_emoji.id);
    ASSERT_STR_EQ("party_blob", created_emoji.name);
    discord_emoji_cleanup(&created_emoji);

    /* 4. Modify guild emoji (must send only {"name":"..."}, no "image":"") */
    struct discord_modify_guild_emoji modify_emoji = {
        .reason = "Rename blob",
        .name = "party_wave",
    };
    struct discord_emoji modified_emoji = { 0 };
    emoji_ret = (struct discord_ret_emoji){ .sync = &modified_emoji };
    ASSERT_EQ(CCORD_OK,
              discord_modify_guild_emoji(client, guild_id, created_emoji_id,
                                         &modify_emoji, &emoji_ret));
    ASSERT_EQ(created_emoji_id, modified_emoji.id);
    ASSERT_STR_EQ("party_wave", modified_emoji.name);
    discord_emoji_cleanup(&modified_emoji);

    /* 5. Delete guild emoji */
    struct discord_delete_guild_emoji delete_emoji = {
        .reason = "Cleanup emoji"
    };
    struct discord_ret delete_ret = { .sync = true };
    ASSERT_EQ(CCORD_OK,
              discord_delete_guild_emoji(client, guild_id, created_emoji_id,
                                         &delete_emoji, &delete_ret));

    /* 6. Get sticker metadata (GET /v1/stickers/{sticker_id}/metadata) */
    struct discord_sticker sticker = { 0 };
    struct discord_ret_sticker sticker_ret = { .sync = &sticker };
    ASSERT_EQ(CCORD_OK, discord_get_sticker(client, sticker_id, &sticker_ret));
    ASSERT_EQ(sticker_id, sticker.id);
    ASSERT_EQ(guild_id, sticker.guild_id);
    ASSERT_STR_EQ("coelho_hi", sticker.name);
    ASSERT_EQ(true, sticker.animated);
    discord_sticker_cleanup(&sticker);

    /* 7. List guild stickers (decodes Fluxer array[string] tags) */
    struct discord_stickers stickers = { 0 };
    struct discord_ret_stickers stickers_ret = { .sync = &stickers };
    ASSERT_EQ(CCORD_OK,
              discord_list_guild_stickers(client, guild_id, &stickers_ret));
    ASSERT_EQ(1, stickers.size);
    ASSERT_EQ(sticker_id, stickers.array[0].id);
    ASSERT_STR_EQ("coelho_hi", stickers.array[0].name);
    ASSERT_STR_EQ("A friendly bunny", stickers.array[0].description);
    ASSERT_STR_EQ("wave,hello", stickers.array[0].tags);
    ASSERT_EQ(true, stickers.array[0].animated);
    discord_stickers_cleanup(&stickers);

    /* 8. Get guild sticker (adapted over GET /guilds/{id}/stickers) */
    sticker = (struct discord_sticker){ 0 };
    sticker_ret = (struct discord_ret_sticker){ .sync = &sticker };
    ASSERT_EQ(CCORD_OK,
              discord_get_guild_sticker(client, guild_id, sticker_id,
                                        &sticker_ret));
    ASSERT_EQ(sticker_id, sticker.id);
    ASSERT_EQ(guild_id, sticker.guild_id);
    ASSERT_STR_EQ("wave,hello", sticker.tags);
    discord_sticker_cleanup(&sticker);

    struct discord_sticker missing_sticker = { 0 };
    sticker_ret = (struct discord_ret_sticker){ .sync = &missing_sticker };
    ASSERT_EQ(CCORD_RESOURCE_UNAVAILABLE,
              discord_get_guild_sticker(client, guild_id,
                                        UINT64_C(9223372036854775999),
                                        &sticker_ret));

    /* 9. Modify guild sticker (encodes comma-separated tags as JSON array) */
    struct discord_modify_guild_sticker modify_sticker = {
        .reason = "Update sticker tags",
        .name = "coelho_yay",
        .tags = "yay, bunny",
    };
    struct discord_sticker modified_sticker = { 0 };
    sticker_ret = (struct discord_ret_sticker){ .sync = &modified_sticker };
    ASSERT_EQ(CCORD_OK,
              discord_modify_guild_sticker(client, guild_id, sticker_id,
                                           &modify_sticker, &sticker_ret));
    ASSERT_EQ(sticker_id, modified_sticker.id);
    ASSERT_STR_EQ("coelho_yay", modified_sticker.name);
    ASSERT_STR_EQ("yay,bunny", modified_sticker.tags);
    discord_sticker_cleanup(&modified_sticker);

    /* 10. Delete guild sticker */
    struct discord_delete_guild_sticker delete_sticker = {
        .reason = "Retire sticker"
    };
    ASSERT_EQ(CCORD_OK,
              discord_delete_guild_sticker(client, guild_id, sticker_id,
                                           &delete_sticker, &delete_ret));

    /* 11. Get invite (both NULL and non-NULL params, no GET body) */
    struct discord_invite invite = { 0 };
    struct discord_ret_invite invite_ret = { .sync = &invite };
    ASSERT_EQ(CCORD_OK,
              discord_get_invite(client, "FluxerInvite42", NULL, &invite_ret));
    ASSERT_STR_EQ("FluxerInvite42", invite.code);
    ASSERT_EQ(0, invite.type);
    ASSERT_EQ(true, invite.temporary);
    ASSERT_EQ(128, invite.approximate_member_count);
    ASSERT_EQ(37, invite.approximate_presence_count);
    ASSERT(invite.expires_at > 0);
    ASSERT(invite.guild != NULL);
    ASSERT_EQ(guild_id, invite.guild->id);
    ASSERT(invite.channel != NULL);
    ASSERT_EQ(UINT64_C(9223372036854775930), invite.channel->id);
    ASSERT(invite.inviter != NULL);
    ASSERT_EQ(UINT64_C(9223372036854775931), invite.inviter->id);
    discord_invite_cleanup(&invite);

    struct discord_get_invite invite_params = {
        .with_counts = true,
        .with_expiration = true,
    };
    invite = (struct discord_invite){ 0 };
    invite_ret = (struct discord_ret_invite){ .sync = &invite };
    ASSERT_EQ(CCORD_OK,
              discord_get_invite(client, "FluxerInvite42", &invite_params,
                                 &invite_ret));
    ASSERT_STR_EQ("FluxerInvite42", invite.code);
    discord_invite_cleanup(&invite);

    /* 12. Delete invite (Fluxer 204 No Content populates invite.code) */
    struct discord_delete_invite delete_invite = { .reason = "Revoke invite" };
    struct discord_invite deleted_invite = { 0 };
    invite_ret = (struct discord_ret_invite){ .sync = &deleted_invite };
    ASSERT_EQ(CCORD_OK,
              discord_delete_invite(client, "FluxerInvite42", &delete_invite,
                                    &invite_ret));
    ASSERT_STR_EQ("FluxerInvite42", deleted_invite.code);
    discord_invite_cleanup(&deleted_invite);

    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    ASSERT_EQ(17, mock.received_requests);

    /* Verify exact Fluxer v1 paths, methods, headers, and JSON payloads */
    ASSERT(strstr(mock.requests[2],
                  "GET /api-root/v1/guilds/9223372036854775809/emojis ")
           != NULL);
    ASSERT(strstr(mock.requests[3],
                  "GET /api-root/v1/guilds/9223372036854775809/emojis ")
           != NULL);
    ASSERT(strstr(mock.requests[4],
                  "GET /api-root/v1/guilds/9223372036854775809/emojis ")
           != NULL);

    ASSERT(strstr(mock.requests[5],
                  "POST /api-root/v1/guilds/9223372036854775809/emojis ")
           != NULL);
    ASSERT(strstr(mock.requests[5], "X-Audit-Log-Reason: Add blob\r\n")
           != NULL);
    ASSERT(strstr(mock.requests[5], "\"name\":\"party_blob\"") != NULL);
    ASSERT(strstr(mock.requests[5], "\"image\":\"data:image/png;base64,AAAA\"")
           != NULL);
    ASSERT(strstr(mock.requests[5], "\"roles\"") == NULL);

    ASSERT(strstr(mock.requests[6],
                  "PATCH /api-root/v1/guilds/9223372036854775809/emojis/"
                  "9223372036854775912 ")
           != NULL);
    ASSERT(strstr(mock.requests[6], "X-Audit-Log-Reason: Rename blob\r\n")
           != NULL);
    ASSERT(strstr(mock.requests[6], "{\"name\":\"party_wave\"}") != NULL);
    ASSERT(strstr(mock.requests[6], "\"image\"") == NULL);

    ASSERT(strstr(mock.requests[7],
                  "DELETE /api-root/v1/guilds/9223372036854775809/emojis/"
                  "9223372036854775912 ")
           != NULL);
    ASSERT(strstr(mock.requests[7], "X-Audit-Log-Reason: Cleanup emoji\r\n")
           != NULL);

    ASSERT(strstr(mock.requests[8],
                  "GET /api-root/v1/stickers/9223372036854775920/metadata ")
           != NULL);
    ASSERT(strstr(mock.requests[9],
                  "GET /api-root/v1/guilds/9223372036854775809/stickers ")
           != NULL);
    ASSERT(strstr(mock.requests[10],
                  "GET /api-root/v1/guilds/9223372036854775809/stickers ")
           != NULL);
    ASSERT(strstr(mock.requests[11],
                  "GET /api-root/v1/guilds/9223372036854775809/stickers ")
           != NULL);

    ASSERT(strstr(mock.requests[12],
                  "PATCH /api-root/v1/guilds/9223372036854775809/stickers/"
                  "9223372036854775920 ")
           != NULL);
    ASSERT(strstr(mock.requests[12],
                  "X-Audit-Log-Reason: Update sticker tags\r\n")
           != NULL);
    ASSERT(strstr(mock.requests[12], "\"name\":\"coelho_yay\"") != NULL);
    ASSERT(strstr(mock.requests[12], "\"tags\":[\"yay\",\"bunny\"]") != NULL);
    ASSERT(strstr(mock.requests[12], "\"description\"") == NULL);

    ASSERT(strstr(mock.requests[13],
                  "DELETE /api-root/v1/guilds/9223372036854775809/stickers/"
                  "9223372036854775920 ")
           != NULL);
    ASSERT(strstr(mock.requests[13], "X-Audit-Log-Reason: Retire sticker\r\n")
           != NULL);

    ASSERT(strstr(mock.requests[14],
                  "GET /api-root/v1/invites/FluxerInvite42 ")
           != NULL);
    ASSERT(strstr(mock.requests[14], "Content-Type:") == NULL);
    ASSERT(strstr(mock.requests[15],
                  "GET /api-root/v1/invites/FluxerInvite42 ")
           != NULL);
    ASSERT(strstr(mock.requests[15], "Content-Type:") == NULL);

    ASSERT(strstr(mock.requests[16],
                  "DELETE /api-root/v1/invites/FluxerInvite42 ")
           != NULL);
    ASSERT(strstr(mock.requests[16], "X-Audit-Log-Reason: Revoke invite\r\n")
           != NULL);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

struct async_asset_ctx {
    int done_calls;
    int fail_calls;
    CCORDcode code;
    u64snowflake id;
    u64snowflake guild_id;
    char text[64];
};

static void
on_async_emoji_done(struct discord *client,
                    struct discord_response *resp,
                    const struct discord_emoji *emoji)
{
    (void)client;
    struct async_asset_ctx *ctx = resp->data;
    ++ctx->done_calls;
    ctx->code = resp->code;
    ctx->id = emoji->id;
    snprintf(ctx->text, sizeof(ctx->text), "%s", emoji->name ? emoji->name : "");
}

static void
on_async_sticker_done(struct discord *client,
                      struct discord_response *resp,
                      const struct discord_sticker *sticker)
{
    (void)client;
    struct async_asset_ctx *ctx = resp->data;
    ++ctx->done_calls;
    ctx->code = resp->code;
    ctx->id = sticker->id;
    ctx->guild_id = sticker->guild_id;
    snprintf(ctx->text, sizeof(ctx->text), "%s",
             sticker->tags ? sticker->tags : "");
}

static void
on_async_invite_done(struct discord *client,
                     struct discord_response *resp,
                     const struct discord_invite *invite)
{
    (void)client;
    struct async_asset_ctx *ctx = resp->data;
    ++ctx->done_calls;
    ctx->code = resp->code;
    snprintf(ctx->text, sizeof(ctx->text), "%s",
             invite->code ? invite->code : "");
}

static void
on_async_asset_fail(struct discord *client, struct discord_response *resp)
{
    (void)client;
    struct async_asset_ctx *ctx = resp->data;
    ++ctx->fail_calls;
    ctx->code = resp->code;
}

static void
wait_for_async_callbacks(struct discord *client,
                         const int *counter,
                         int target)
{
    for (int i = 0; i < 1000 && *counter < target; ++i) {
        discord_requestor_dispatch_responses(&client->rest.requestor);
        if (*counter >= target) break;
        cog_sleep_us(1000);
    }
}

TEST
async_adapted_emoji_sticker_and_invite_callbacks_dispatch_cleanly(void)
{
    static const char emoji_list_json[] =
        "[{\"id\":\"9223372036854775910\",\"name\":\"async_wave\","
        "\"animated\":true}]";
    static const char sticker_list_json[] =
        "[{\"id\":\"9223372036854775920\",\"name\":\"async_bunny\","
        "\"tags\":[\"hop\",\"fast\"],\"animated\":false}]";

    struct http_mock mock = { 0 };
    ASSERT_EQ(0, mock_open(&mock, 6));

    char origin[64], discovery_json[512];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", mock.port);
    snprintf(discovery_json, sizeof(discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);
    mock.bodies[0] = discovery_json;
    mock.bodies[1] = k_init_user_json;
    mock.bodies[2] = emoji_list_json;
    mock.bodies[3] = emoji_list_json;
    mock.bodies[4] = sticker_list_json;
    mock.statuses[5] = "204 No Content";
    mock.bodies[5] = "";

    char config_path[] = "/tmp/concord-fluxer-async-assets-XXXXXX";
    ASSERT(write_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    const u64snowflake guild_id = UINT64_C(9223372036854775809);
    struct async_asset_ctx emoji_ctx = { 0 };
    struct discord_ret_emoji emoji_ret = {
        .data = &emoji_ctx,
        .done = &on_async_emoji_done,
        .fail = &on_async_asset_fail,
    };
    ASSERT_EQ(CCORD_PENDING,
              discord_get_guild_emoji(client, guild_id,
                                      UINT64_C(9223372036854775910),
                                      &emoji_ret));
    wait_for_async_callbacks(client, &emoji_ctx.done_calls, 1);
    ASSERT_EQ(1, emoji_ctx.done_calls);
    ASSERT_EQ(0, emoji_ctx.fail_calls);
    ASSERT_EQ(UINT64_C(9223372036854775910), emoji_ctx.id);
    ASSERT_STR_EQ("async_wave", emoji_ctx.text);

    struct async_asset_ctx missing_ctx = { 0 };
    struct discord_ret_emoji missing_ret = {
        .data = &missing_ctx,
        .done = &on_async_emoji_done,
        .fail = &on_async_asset_fail,
    };
    ASSERT_EQ(CCORD_PENDING,
              discord_get_guild_emoji(client, guild_id,
                                      UINT64_C(9223372036854775999),
                                      &missing_ret));
    wait_for_async_callbacks(client, &missing_ctx.fail_calls, 1);
    ASSERT_EQ(0, missing_ctx.done_calls);
    ASSERT_EQ(1, missing_ctx.fail_calls);
    ASSERT_EQ(CCORD_RESOURCE_UNAVAILABLE, missing_ctx.code);

    struct async_asset_ctx sticker_ctx = { 0 };
    struct discord_ret_sticker sticker_ret = {
        .data = &sticker_ctx,
        .done = &on_async_sticker_done,
        .fail = &on_async_asset_fail,
    };
    ASSERT_EQ(CCORD_PENDING,
              discord_get_guild_sticker(client, guild_id,
                                        UINT64_C(9223372036854775920),
                                        &sticker_ret));
    wait_for_async_callbacks(client, &sticker_ctx.done_calls, 1);
    ASSERT_EQ(1, sticker_ctx.done_calls);
    ASSERT_EQ(UINT64_C(9223372036854775920), sticker_ctx.id);
    ASSERT_EQ(guild_id, sticker_ctx.guild_id);
    ASSERT_STR_EQ("hop,fast", sticker_ctx.text);

    struct async_asset_ctx invite_ctx = { 0 };
    struct discord_ret_invite invite_ret = {
        .data = &invite_ctx,
        .done = &on_async_invite_done,
        .fail = &on_async_asset_fail,
    };
    ASSERT_EQ(CCORD_PENDING,
              discord_delete_invite(client, "AsyncCode99", NULL, &invite_ret));
    wait_for_async_callbacks(client, &invite_ctx.done_calls, 1);
    ASSERT_EQ(1, invite_ctx.done_calls);
    ASSERT_STR_EQ("AsyncCode99", invite_ctx.text);

    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

GREATEST_MAIN_DEFS();

int
main(int argc, char *argv[])
{
    GREATEST_MAIN_BEGIN();

    RUN_TEST(
        unsupported_moderation_pack_and_template_calls_emit_no_network_traffic);
    RUN_TEST(
        audit_log_uses_fluxer_route_query_params_and_decodes_large_snowflakes);
    RUN_TEST(emoji_sticker_and_invite_endpoints_match_fluxer_v1_protocol);
    RUN_TEST(async_adapted_emoji_sticker_and_invite_callbacks_dispatch_cleanly);

    GREATEST_MAIN_END();
}
