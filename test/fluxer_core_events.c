#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <openssl/evp.h>

#include "discord.h"
#include "discord-internal.h"
#include "greatest.h"

#define MOCK_MAX_REQUESTS 8
#define MOCK_BUF_SIZE     8192

static const char TEST_TOKEN[] =
    "123456789012345678.abcdefghijklmnopqrstuvwxyz0123456789ABCD";

struct unsupported_ctx {
    int calls;
    CCORDcode code;
    void *data;
    const void *keep;
};

static void
on_unsupported(struct discord *client, struct discord_response *resp)
{
    (void)client;
    struct unsupported_ctx *ctx = resp->data;
    ++ctx->calls;
    ctx->code = resp->code;
    ctx->data = resp->data;
    ctx->keep = resp->keep;
}

struct http_mock {
    int fd;
    unsigned short port;
    size_t expected_requests;
    size_t received_requests;
    size_t unexpected_requests;
    const char *statuses[MOCK_MAX_REQUESTS];
    const char *bodies[MOCK_MAX_REQUESTS];
    char requests[MOCK_MAX_REQUESTS][MOCK_BUF_SIZE];
};

static size_t
load_file(const char *path, char **contents)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) fp = fopen(path + 5, "rb"); /* fallback when run inside test/ */
    if (!fp) return 0;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    long size = ftell(fp);
    if (size < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 0;
    }
    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(fp);
        return 0;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return 0;
    }
    buf[size] = '\0';
    fclose(fp);
    *contents = buf;
    return (size_t)size;
}

static int
http_mock_open(struct http_mock *mock, size_t expected_requests)
{
    struct sockaddr_in addr = { .sin_family = AF_INET,
                                .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
                                .sin_port = 0 };
    socklen_t len = sizeof(addr);
    memset(mock, 0, sizeof(*mock));
    mock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (mock->fd < 0) return -1;
    if (bind(mock->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0
        || listen(mock->fd, 8) < 0
        || getsockname(mock->fd, (struct sockaddr *)&addr, &len) < 0)
    {
        close(mock->fd);
        return -1;
    }
    mock->port = ntohs(addr.sin_port);
    mock->expected_requests = expected_requests;
    for (size_t i = 0; i < MOCK_MAX_REQUESTS; ++i) {
        mock->statuses[i] = "200 OK";
        mock->bodies[i] = "{}";
    }
    return 0;
}

static size_t
parse_content_length(const char req[], size_t header_len)
{
    const char *line = req;
    while (line < req + header_len) {
        const char *end = strstr(line, "\r\n");
        if (!end || end > req + header_len) break;
        if (strncasecmp(line, "Content-Length:", 15) == 0) {
            return (size_t)strtoul(line + 15, NULL, 10);
        }
        line = end + 2;
    }
    return 0;
}

static void *
http_mock_serve(void *arg)
{
    struct http_mock *mock = arg;
    for (size_t i = 0; i < mock->expected_requests; ++i) {
        struct pollfd pfd = { .fd = mock->fd, .events = POLLIN };
        if (poll(&pfd, 1, 3000) <= 0) break;
        int conn = accept(mock->fd, NULL, NULL);
        if (conn < 0) break;

        char *req = mock->requests[i];
        size_t used = 0, hdr_len = 0, want_len = 0;
        while (used + 1 < MOCK_BUF_SIZE) {
            ssize_t n = recv(conn, req + used, MOCK_BUF_SIZE - used - 1, 0);
            if (n <= 0) break;
            used += (size_t)n;
            req[used] = '\0';
            char *hdr_end = strstr(req, "\r\n\r\n");
            if (hdr_end && !hdr_len) {
                hdr_len = (size_t)(hdr_end + 4 - req);
                want_len = hdr_len + parse_content_length(req, hdr_len);
            }
            if (hdr_len && used >= want_len) break;
        }
        ++mock->received_requests;

        char resp[MOCK_BUF_SIZE];
        int resp_len = snprintf(
            resp, sizeof(resp),
            "HTTP/1.1 %s\r\nContent-Type: application/json\r\n"
            "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
            mock->statuses[i], strlen(mock->bodies[i]), mock->bodies[i]);
        if (resp_len > 0 && (size_t)resp_len < sizeof(resp))
            (void)send(conn, resp, (size_t)resp_len, 0);
        close(conn);
    }

    struct pollfd extra = { .fd = mock->fd, .events = POLLIN };
    if (poll(&extra, 1, 50) > 0) {
        int conn = accept(mock->fd, NULL, NULL);
        if (conn >= 0) {
            ++mock->unexpected_requests;
            close(conn);
        }
    }
    close(mock->fd);
    return NULL;
}

TEST
all_application_command_and_interaction_apis_return_unsupported_without_http(
    void)
{
    char *user_json = NULL;
    ASSERT(load_file("test/fixtures/fluxer-user.json", &user_json) > 0);

    struct http_mock mock;
    ASSERT_EQ(0, http_mock_open(&mock, 2));
    char origin[64], discovery_json[512];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", mock.port);
    snprintf(discovery_json, sizeof(discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);
    mock.bodies[0] = discovery_json;
    mock.bodies[1] = user_json;

    pthread_t server_tid;
    ASSERT_EQ(0, pthread_create(&server_tid, NULL, http_mock_serve, &mock));

    struct discord_config cfg = {
        .token = (char *)TEST_TOKEN,
        .instance_origin = origin,
        .log = { .quiet = true },
    };
    struct discord *client = discord_from_config(&cfg);
    ASSERT(client != NULL);

    struct unsupported_ctx ctx = { 0 };
    const int keep_marker = 42;
    const u64snowflake app_id = UINT64_C(9223372036854775820);
    const u64snowflake guild_id = UINT64_C(9223372036854775821);
    const u64snowflake cmd_id = UINT64_C(9223372036854775822);
    const u64snowflake inter_id = UINT64_C(9223372036854775823);
    const u64snowflake msg_id = UINT64_C(9223372036854775824);

#define EXPECT_UNSUPPORTED_CALL(call_with_ret, call_null_ret)                 \
    do {                                                                      \
        int prev_calls = ctx.calls;                                           \
        ctx.code = CCORD_OK;                                                  \
        ctx.data = NULL;                                                      \
        ctx.keep = NULL;                                                      \
        ASSERT_EQ(CCORD_UNSUPPORTED, (call_with_ret));                        \
        ASSERT_EQ(prev_calls + 1, ctx.calls);                                 \
        ASSERT_EQ(CCORD_UNSUPPORTED, ctx.code);                               \
        ASSERT_EQ(&ctx, ctx.data);                                            \
        ASSERT_EQ(&keep_marker, ctx.keep);                                    \
        ASSERT_EQ(CCORD_UNSUPPORTED, (call_null_ret));                        \
        ASSERT_EQ(prev_calls + 1, ctx.calls);                                 \
    } while (0)

    struct discord_ret_application_commands ret_cmds = {
        .fail = &on_unsupported, .data = &ctx, .keep = &keep_marker
    };
    struct discord_ret_application_command ret_cmd = {
        .fail = &on_unsupported, .data = &ctx, .keep = &keep_marker
    };
    struct discord_ret ret_blank = {
        .fail = &on_unsupported, .data = &ctx, .keep = &keep_marker
    };
    struct discord_ret_guild_application_command_permissions ret_gperms = {
        .fail = &on_unsupported, .data = &ctx, .keep = &keep_marker
    };
    struct discord_ret_application_command_permission ret_perm = {
        .fail = &on_unsupported, .data = &ctx, .keep = &keep_marker
    };
    struct discord_ret_interaction_response ret_iresp = {
        .fail = &on_unsupported, .data = &ctx, .keep = &keep_marker
    };
    struct discord_ret_webhook ret_wh = {
        .fail = &on_unsupported, .data = &ctx, .keep = &keep_marker
    };
    struct discord_ret_message ret_msg = {
        .fail = &on_unsupported, .data = &ctx, .keep = &keep_marker
    };

    /* 14 application_command.h operations */
    EXPECT_UNSUPPORTED_CALL(
        discord_get_global_application_commands(client, app_id, &ret_cmds),
        discord_get_global_application_commands(client, app_id, NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_create_global_application_command(client, app_id, NULL,
                                                  &ret_cmd),
        discord_create_global_application_command(client, app_id, NULL, NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_get_global_application_command(client, app_id, cmd_id,
                                               &ret_cmd),
        discord_get_global_application_command(client, app_id, cmd_id, NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_edit_global_application_command(client, app_id, cmd_id, NULL,
                                                &ret_cmd),
        discord_edit_global_application_command(client, app_id, cmd_id, NULL,
                                                NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_delete_global_application_command(client, app_id, cmd_id,
                                                  &ret_blank),
        discord_delete_global_application_command(client, app_id, cmd_id,
                                                  NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_bulk_overwrite_global_application_commands(client, app_id,
                                                           NULL, &ret_cmds),
        discord_bulk_overwrite_global_application_commands(client, app_id,
                                                           NULL, NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_get_guild_application_commands(client, app_id, guild_id,
                                               &ret_cmds),
        discord_get_guild_application_commands(client, app_id, guild_id,
                                               NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_create_guild_application_command(client, app_id, guild_id,
                                                 NULL, &ret_cmd),
        discord_create_guild_application_command(client, app_id, guild_id,
                                                 NULL, NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_get_guild_application_command(client, app_id, guild_id, cmd_id,
                                              &ret_cmd),
        discord_get_guild_application_command(client, app_id, guild_id, cmd_id,
                                              NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_edit_guild_application_command(client, app_id, guild_id,
                                               cmd_id, NULL, &ret_cmd),
        discord_edit_guild_application_command(client, app_id, guild_id,
                                               cmd_id, NULL, NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_delete_guild_application_command(client, app_id, guild_id,
                                                 cmd_id, &ret_blank),
        discord_delete_guild_application_command(client, app_id, guild_id,
                                                 cmd_id, NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_bulk_overwrite_guild_application_commands(client, app_id,
                                                          guild_id, NULL,
                                                          &ret_cmds),
        discord_bulk_overwrite_guild_application_commands(client, app_id,
                                                          guild_id, NULL,
                                                          NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_get_guild_application_command_permissions(client, app_id,
                                                          guild_id,
                                                          &ret_gperms),
        discord_get_guild_application_command_permissions(client, app_id,
                                                          guild_id, NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_get_application_command_permissions(client, app_id, guild_id,
                                                    cmd_id, &ret_perm),
        discord_get_application_command_permissions(client, app_id, guild_id,
                                                    cmd_id, NULL));

    /* 8 interaction.h operations */
    EXPECT_UNSUPPORTED_CALL(
        discord_create_interaction_response(client, inter_id, "tok", NULL,
                                            &ret_iresp),
        discord_create_interaction_response(client, inter_id, "tok", NULL,
                                            NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_get_original_interaction_response(client, app_id, "tok",
                                                  &ret_iresp),
        discord_get_original_interaction_response(client, app_id, "tok",
                                                  NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_edit_original_interaction_response(client, app_id, "tok", NULL,
                                                   &ret_iresp),
        discord_edit_original_interaction_response(client, app_id, "tok", NULL,
                                                   NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_delete_original_interaction_response(client, app_id, "tok",
                                                     &ret_blank),
        discord_delete_original_interaction_response(client, app_id, "tok",
                                                     NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_create_followup_message(client, app_id, "tok", NULL, &ret_wh),
        discord_create_followup_message(client, app_id, "tok", NULL, NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_get_followup_message(client, app_id, "tok", msg_id, &ret_msg),
        discord_get_followup_message(client, app_id, "tok", msg_id, NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_edit_followup_message(client, app_id, "tok", msg_id, NULL,
                                      &ret_msg),
        discord_edit_followup_message(client, app_id, "tok", msg_id, NULL,
                                      NULL));
    EXPECT_UNSUPPORTED_CALL(
        discord_delete_followup_message(client, app_id, "tok", msg_id,
                                        &ret_blank),
        discord_delete_followup_message(client, app_id, "tok", msg_id, NULL));

#undef EXPECT_UNSUPPORTED_CALL

    ASSERT_EQ(22, ctx.calls);
    ASSERT_EQ(0, pthread_join(server_tid, NULL));
    ASSERT_EQ(2u, mock.received_requests);
    ASSERT_EQ(0u, mock.unexpected_requests);

    discord_cleanup(client);
    free(user_json);
    PASS();
}

TEST
oauth2_and_gateway_rest_routes_match_fluxer_v1(void)
{
    char *user_json = NULL;
    ASSERT(load_file("test/fixtures/fluxer-user.json", &user_json) > 0);

    struct http_mock mock;
    ASSERT_EQ(0, http_mock_open(&mock, 5));
    char origin[64], discovery_json[512];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", mock.port);
    snprintf(discovery_json, sizeof(discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);

    static const char app_json[] =
        "{\"id\":\"9223372036854775825\",\"name\":\"FluxerBotApp\","
        "\"bot_public\":true,\"bot_require_code_grant\":false}";
    static const char gw_bot_json[] =
        "{\"url\":\"wss://gateway.selfhost.test\",\"shards\":1,"
        "\"session_start_limit\":{\"total\":1000,\"remaining\":999,"
        "\"reset_after\":14400000,\"max_concurrency\":1}}";
    static const char member_json[] =
        "{\"nick\":\"disconnected-member\",\"roles\":[],"
        "\"joined_at\":\"2026-11-11T00:00:00.000Z\"}";

    mock.bodies[0] = discovery_json;
    mock.bodies[1] = user_json;
    mock.bodies[2] = app_json;
    mock.bodies[3] = gw_bot_json;
    mock.bodies[4] = member_json;

    pthread_t server_tid;
    ASSERT_EQ(0, pthread_create(&server_tid, NULL, http_mock_serve, &mock));

    struct discord_config cfg = {
        .token = (char *)TEST_TOKEN,
        .instance_origin = origin,
        .log = { .quiet = true },
    };
    struct discord *client = discord_from_config(&cfg);
    ASSERT(client != NULL);

    /* OAuth2: GET /v1/applications/@me */
    struct discord_application app = { 0 };
    struct discord_ret_application app_ret = { .sync = &app };
    ASSERT_EQ(CCORD_OK,
              discord_get_current_bot_application_information(client,
                                                              &app_ret));
    ASSERT_EQ(UINT64_C(9223372036854775825), app.id);
    ASSERT_STR_EQ("FluxerBotApp", app.name);
    ASSERT(app.bot_public == true);
    ASSERT(app.bot_require_code_grant == false);
    discord_application_cleanup(&app);

    /* OAuth2: GET /v1/oauth2/@me is Bearer-only -> CCORD_UNSUPPORTED */
    struct unsupported_ctx auth_ctx = { 0 };
    struct discord_ret_auth_response auth_ret = {
        .fail = &on_unsupported,
        .data = &auth_ctx,
        .keep = &auth_ctx,
    };
    ASSERT_EQ(CCORD_UNSUPPORTED,
              discord_get_current_authorization_information(client,
                                                            &auth_ret));
    ASSERT_EQ(1, auth_ctx.calls);
    ASSERT_EQ(CCORD_UNSUPPORTED, auth_ctx.code);

    /* Gateway REST: GET /v1/gateway is absent -> CCORD_UNSUPPORTED */
    struct ccord_szbuf gw_buf = { 0 };
    ASSERT_EQ(CCORD_UNSUPPORTED, discord_get_gateway(client, &gw_buf));
    ASSERT(gw_buf.start == NULL);

    /* Gateway REST: GET /v1/gateway/bot */
    ASSERT_EQ(CCORD_OK, discord_get_gateway_bot(client, &gw_buf));
    ASSERT(gw_buf.start != NULL);
    ASSERT(strstr(gw_buf.start, "wss://gateway.selfhost.test") != NULL);
    free(gw_buf.start);

    /* Gateway helper REST: PATCH /v1/guilds/{guild_id}/members/{user_id} */
    struct discord_modify_guild_member mod_params = {
        .reason = "voice-moderation",
    };
    struct discord_guild_member member = { 0 };
    struct discord_ret_guild_member member_ret = { .sync = &member };
    ASSERT_EQ(CCORD_OK,
              discord_disconnect_guild_member(
                  client, UINT64_C(9223372036854775830),
                  UINT64_C(9223372036854775831), &mod_params, &member_ret));
    ASSERT_STR_EQ("disconnected-member", member.nick);
    discord_guild_member_cleanup(&member);

    ASSERT_EQ(0, pthread_join(server_tid, NULL));
    ASSERT_EQ(5u, mock.received_requests);
    ASSERT_EQ(0u, mock.unexpected_requests);

    ASSERT(strstr(mock.requests[2],
                  "GET /api-root/v1/applications/@me ") != NULL);
    ASSERT(strstr(mock.requests[2], "Authorization: Bot ") != NULL);
    ASSERT(strstr(mock.requests[3],
                  "GET /api-root/v1/gateway/bot ") != NULL);
    ASSERT(strstr(
               mock.requests[4],
               "PATCH /api-root/v1/guilds/9223372036854775830/members/9223372036854775831 ")
           != NULL);
    const char *patch_body = strstr(mock.requests[4], "\r\n\r\n");
    ASSERT(patch_body != NULL);
    ASSERT_STR_EQ("{\"channel_id\":null}", patch_body + 4);

    discord_cleanup(client);
    free(user_json);
    PASS();
}

static void
noop_timer_cb(struct discord *client, struct discord_timer *ev)
{
    (void)client;
    (void)ev;
}

TEST
instance_origin_setter_and_client_helpers_work_before_gateway_run(void)
{
    char *user_json = NULL;
    ASSERT(load_file("test/fixtures/fluxer-user.json", &user_json) > 0);

    struct http_mock mock_a, mock_fail, mock_b;
    ASSERT_EQ(0, http_mock_open(&mock_a, 2));
    ASSERT_EQ(0, http_mock_open(&mock_fail, 1));
    ASSERT_EQ(0, http_mock_open(&mock_b, 2));

    char origin_a[64], origin_fail[64], origin_b[64];
    char discovery_a[512], discovery_b[512];
    snprintf(origin_a, sizeof(origin_a), "http://127.0.0.1:%u", mock_a.port);
    snprintf(origin_fail, sizeof(origin_fail), "http://127.0.0.1:%u",
             mock_fail.port);
    snprintf(origin_b, sizeof(origin_b), "http://127.0.0.1:%u", mock_b.port);

    snprintf(discovery_a, sizeof(discovery_a),
             "{\"endpoints\":{\"api_public\":\"%s/api-a\","
             "\"gateway\":\"ws://gateway-a.selfhost.test\","
             "\"media\":\"http://media-a.selfhost.test\"}}",
             origin_a);
    snprintf(discovery_b, sizeof(discovery_b),
             "{\"endpoints\":{\"api_public\":\"%s/api-b\","
             "\"gateway\":\"ws://gateway-b.selfhost.test/ws\","
             "\"media\":\"http://media-b.selfhost.test\"}}",
             origin_b);

    mock_a.bodies[0] = discovery_a;
    mock_a.bodies[1] = user_json;
    mock_fail.statuses[0] = "503 Service Unavailable";
    mock_fail.bodies[0] = "{}";
    mock_b.bodies[0] = discovery_b;
    mock_b.bodies[1] = user_json;

    pthread_t tid_a, tid_fail, tid_b;
    ASSERT_EQ(0, pthread_create(&tid_a, NULL, http_mock_serve, &mock_a));
    ASSERT_EQ(0, pthread_create(&tid_fail, NULL, http_mock_serve, &mock_fail));
    ASSERT_EQ(0, pthread_create(&tid_b, NULL, http_mock_serve, &mock_b));

    struct discord_config cfg = {
        .token = (char *)TEST_TOKEN,
        .instance_origin = origin_a,
        .log = { .quiet = true },
    };
    struct discord *client = discord_from_config(&cfg);
    ASSERT(client != NULL);
    ASSERT_EQ(0, pthread_join(tid_a, NULL));

    /* Local client, error, intent, and timer helpers */
    int marker = 99;
    ASSERT_EQ(&marker, discord_set_data(client, &marker));
    ASSERT_EQ(&marker, discord_get_data(client));
    ASSERT(discord_get_self(client) != NULL);
    ASSERT(discord_get_logmod(client) != NULL);
    ASSERT(discord_get_io_poller(client) != NULL);
    ASSERT_EQ(0, discord_get_ping(client));
    ASSERT(discord_timestamp(client) > 0);
    ASSERT(discord_timestamp_us(client) > 0);
    ASSERT_STR_EQ("CCORD_UNSUPPORTED",
                  discord_code_as_string(CCORD_UNSUPPORTED));
    ASSERT(strstr(discord_strerror(CCORD_UNSUPPORTED, client), "not supported")
           != NULL);

    discord_add_intents(client, DISCORD_GATEWAY_MESSAGE_CONTENT
                                    | DISCORD_GATEWAY_GUILDS);
    ASSERT_EQ((uint64_t)(DISCORD_GATEWAY_MESSAGE_CONTENT
                         | DISCORD_GATEWAY_GUILDS),
              client->gw.id.intents);
    discord_remove_intents(client, DISCORD_GATEWAY_GUILDS);
    ASSERT_EQ((uint64_t)DISCORD_GATEWAY_MESSAGE_CONTENT,
              client->gw.id.intents);

    unsigned tid1 = discord_timer(client, &noop_timer_cb, NULL, NULL, 60000);
    ASSERT(tid1 != 0);
    struct discord_timer inspected = { 0 };
    ASSERT(discord_timer_get(client, tid1, &inspected));
    ASSERT(discord_timer_stop(client, tid1));
    ASSERT(discord_timer_start(client, tid1));
    ASSERT(discord_timer_cancel_and_delete(client, tid1));

    unsigned tid2 = discord_timer_interval(client, &noop_timer_cb, NULL, NULL,
                                           60000, 60000, 2);
    ASSERT(tid2 != 0);
    ASSERT(discord_timer_delete(client, tid2));

    /* Invalid origins rejected before HTTP */
    ASSERT_EQ(CCORD_BAD_PARAMETER, discord_set_instance_origin(NULL, origin_b));
    ASSERT_EQ(CCORD_BAD_PARAMETER, discord_set_instance_origin(client, NULL));
    ASSERT_EQ(CCORD_BAD_PARAMETER, discord_set_instance_origin(client, ""));
    ASSERT_EQ(CCORD_BAD_PARAMETER,
              discord_set_instance_origin(client, "ftp://fluxer.example"));
    ASSERT_EQ(CCORD_BAD_PARAMETER,
              discord_set_instance_origin(client,
                                          "https://fluxer.example/subpath"));

    /* Failed discovery preserves previous endpoints */
    ASSERT_EQ(CCORD_HTTP_CODE,
              discord_set_instance_origin(client, origin_fail));
    ASSERT_EQ(0, pthread_join(tid_fail, NULL));
    ASSERT_STR_EQ(origin_a, client->config.instance_origin);

    /* Valid origin updates endpoints and subsequent REST requests */
    ASSERT_EQ(CCORD_OK, discord_set_instance_origin(client, origin_b));
    ASSERT_STR_EQ(origin_b, client->config.instance_origin);
    ASSERT_STR_EQ("ws://gateway-b.selfhost.test/ws", client->gateway_url);

    struct discord_user fetched = { 0 };
    struct discord_ret_user user_ret = { .sync = &fetched };
    ASSERT_EQ(CCORD_OK, discord_get_current_user(client, &user_ret));
    discord_user_cleanup(&fetched);
    ASSERT_EQ(0, pthread_join(tid_b, NULL));
    ASSERT(strstr(mock_b.requests[0], "GET /.well-known/fluxer ") != NULL);
    ASSERT(strstr(mock_b.requests[0], "Authorization:") == NULL);
    ASSERT(strstr(mock_b.requests[1], "GET /api-b/v1/users/@me ") != NULL);

    /* Cloning preserves instance_origin and discovered URLs */
    struct discord *clone = discord_clone(client);
    ASSERT(clone != NULL);
    ASSERT_STR_EQ(origin_b, clone->config.instance_origin);
    ASSERT_STR_EQ("ws://gateway-b.selfhost.test/ws", clone->gateway_url);
    ASSERT_EQ(CCORD_BAD_PARAMETER,
              discord_set_instance_origin(clone, origin_a));
    discord_cleanup(clone);

    /* Active Gateway session rejects origin changes */
    client->gw.session->is_ready = true;
    ASSERT_EQ(CCORD_BAD_PARAMETER,
              discord_set_instance_origin(client, origin_a));
    client->gw.session->is_ready = false;

    discord_cleanup(client);
    free(user_json);
    PASS();
}

/* =========================================================================
 * Scripted Gateway v1 server for opcodes (3, 4, 8), dispatch, cache, prefix
 * ========================================================================= */

struct gw_events_server {
    int fd;
    unsigned short port;
    char discovery_json[512];
    char gateway_bot_json[512];
    const char *user_json;
    char identify_frame[2048];
    char ready_hb_frame[256];
    char presence_frame[2048];
    char voice_join_frame[1024];
    char voice_leave_frame[1024];
    char req_members_frame[1024];
    int failed;
};

static struct {
    int ready_calls;
    int guild_create_calls;
    u64snowflake cached_guild_id;
    int typing_calls;
    u64unix_ms typing_timestamp_ms;
    int chunk_calls;
    int voice_state_calls;
    int webhooks_calls;
    int cache_hit_before_bulk_delete;
    int cache_miss_after_bulk_delete;
    int human_ping_calls;
    int ignored_bot_or_webhook_pings;
    char ping_arg[64];
} g_ev_state;

static bool
ws_read_exact(int fd, void *buf, size_t len)
{
    size_t used = 0;
    while (used < len) {
        ssize_t n = recv(fd, (char *)buf + used, len - used, 0);
        if (n <= 0) return false;
        used += (size_t)n;
    }
    return true;
}

static bool
ws_write_exact(int fd, const void *buf, size_t len)
{
    size_t used = 0;
    while (used < len) {
        ssize_t n = send(fd, (const char *)buf + used, len - used, 0);
        if (n <= 0) return false;
        used += (size_t)n;
    }
    return true;
}

static bool
ws_read_http_headers(int fd, char buf[], size_t cap)
{
    size_t used = 0;
    while (used + 1 < cap) {
        ssize_t n = recv(fd, buf + used, cap - used - 1, 0);
        if (n <= 0) return false;
        used += (size_t)n;
        buf[used] = '\0';
        if (strstr(buf, "\r\n\r\n")) return true;
    }
    return false;
}

static bool
ws_send_http(int fd, const char *status, const char *body)
{
    char resp[4096];
    int n = snprintf(resp, sizeof(resp),
                     "HTTP/1.1 %s\r\nContent-Type: application/json\r\n"
                     "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
                     status, strlen(body), body);
    return n > 0 && (size_t)n < sizeof(resp)
           && ws_write_exact(fd, resp, (size_t)n);
}

static bool
ws_read_frame(int fd, char out[], size_t out_size, unsigned *opcode)
{
    unsigned char hdr[2];
    if (!ws_read_exact(fd, hdr, sizeof(hdr))) return false;
    *opcode = hdr[0] & 0x0f;
    bool masked = (hdr[1] & 0x80) != 0;
    uint64_t len = hdr[1] & 0x7f;
    if (len == 126) {
        unsigned char ext[2];
        if (!ws_read_exact(fd, ext, sizeof(ext))) return false;
        len = ((uint64_t)ext[0] << 8) | ext[1];
    }
    else if (len == 127) {
        unsigned char ext[8];
        if (!ws_read_exact(fd, ext, sizeof(ext))) return false;
        len = 0;
        for (size_t i = 0; i < 8; ++i) len = (len << 8) | ext[i];
    }
    if (len + 1 > out_size) return false;
    unsigned char mask[4] = { 0 };
    if (masked && !ws_read_exact(fd, mask, sizeof(mask))) return false;
    if (!ws_read_exact(fd, out, (size_t)len)) return false;
    if (masked) {
        for (size_t i = 0; i < (size_t)len; ++i) out[i] ^= mask[i % 4];
    }
    out[len] = '\0';
    return true;
}

static bool
ws_send_frame(int fd, unsigned opcode, const char *text)
{
    size_t len = strlen(text);
    unsigned char hdr[10];
    size_t hdr_len;
    hdr[0] = (unsigned char)(0x80 | (opcode & 0x0f));
    if (len < 126) {
        hdr[1] = (unsigned char)len;
        hdr_len = 2;
    }
    else {
        hdr[1] = 126;
        hdr[2] = (unsigned char)(len >> 8);
        hdr[3] = (unsigned char)len;
        hdr_len = 4;
    }
    return ws_write_exact(fd, hdr, hdr_len) && ws_write_exact(fd, text, len);
}

static bool
ws_upgrade(int fd)
{
    char req[4096];
    if (!ws_read_http_headers(fd, req, sizeof(req))) return false;
    char key[128] = { 0 };
    char *h = req;
    while ((h = strstr(h, "Sec-WebSocket-Key:")) != NULL) {
        if (sscanf(h, "Sec-WebSocket-Key: %127s", key) == 1) break;
        ++h;
    }
    if (!*key) return false;
    char input[256];
    int in_len = snprintf(input, sizeof(input),
                          "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    if (in_len < 0
        || EVP_Digest(input, (size_t)in_len, digest, &digest_len, EVP_sha1(),
                      NULL)
               != 1)
        return false;
    unsigned char accept_key[64];
    int accept_len = EVP_EncodeBlock(accept_key, digest, (int)digest_len);
    char resp[512];
    int resp_len = snprintf(
        resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
        "Connection: Upgrade\r\nSec-WebSocket-Accept: %.*s\r\n\r\n",
        accept_len, accept_key);
    return resp_len > 0 && ws_write_exact(fd, resp, (size_t)resp_len);
}

static void *
gw_events_server_thread(void *arg)
{
    struct gw_events_server *srv = arg;
    struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
    bool ok = true;

    for (int i = 0; i < 3 && ok; ++i) {
        int fd = accept(srv->fd, NULL, NULL);
        if (fd < 0) {
            ok = false;
            break;
        }
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        char req[4096];
        const char *body = i == 0 ? srv->discovery_json
                           : i == 1 ? srv->user_json
                                    : srv->gateway_bot_json;
        ok = ws_read_http_headers(fd, req, sizeof(req))
             && ws_send_http(fd, "200 OK", body);
        close(fd);
    }

    int ws_fd = ok ? accept(srv->fd, NULL, NULL) : -1;
    if (ws_fd < 0) {
        srv->failed = 1;
        close(srv->fd);
        return NULL;
    }
    setsockopt(ws_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char *hello = NULL, *ready = NULL;
    ok = ws_upgrade(ws_fd)
         && load_file("test/fixtures/fluxer-gateway-hello.json", &hello) > 0
         && load_file("test/fixtures/fluxer-gateway-ready.json", &ready) > 0;

    unsigned opcode = 0;
    if (ok) ok = ws_send_frame(ws_fd, 1, hello);
    if (ok)
        ok = ws_read_frame(ws_fd, srv->identify_frame,
                           sizeof(srv->identify_frame), &opcode)
             && opcode == 1;
    if (ok) ok = ws_send_frame(ws_fd, 1, ready);
    /* READY triggers heartbeat + 4 client commands in on_ready */
    if (ok)
        ok = ws_read_frame(ws_fd, srv->ready_hb_frame,
                           sizeof(srv->ready_hb_frame), &opcode)
             && opcode == 1;
    if (ok)
        ok = ws_read_frame(ws_fd, srv->presence_frame,
                           sizeof(srv->presence_frame), &opcode)
             && opcode == 1;
    if (ok)
        ok = ws_read_frame(ws_fd, srv->voice_join_frame,
                           sizeof(srv->voice_join_frame), &opcode)
             && opcode == 1;
    if (ok)
        ok = ws_read_frame(ws_fd, srv->voice_leave_frame,
                           sizeof(srv->voice_leave_frame), &opcode)
             && opcode == 1;
    if (ok)
        ok = ws_read_frame(ws_fd, srv->req_members_frame,
                           sizeof(srv->req_members_frame), &opcode)
             && opcode == 1;
    if (ok) ok = ws_send_frame(ws_fd, 1, "{\"op\":11}");

    /* Dispatch Fluxer Gateway v1 events */
    if (ok)
        ok = ws_send_frame(
            ws_fd, 1,
            "{\"op\":0,\"s\":2,\"t\":\"GUILD_CREATE\","
            "\"d\":{\"id\":\"9223372036854775840\",\"name\":\"FluxerGuild\"}}");
    if (ok)
        ok = ws_send_frame(
            ws_fd, 1,
            "{\"op\":0,\"s\":3,\"t\":\"TYPING_START\","
            "\"d\":{\"channel_id\":\"9223372036854775809\","
            "\"guild_id\":\"9223372036854775840\","
            "\"user_id\":\"9223372036854775842\",\"timestamp\":1700000123}}");
    if (ok)
        ok = ws_send_frame(
            ws_fd, 1,
            "{\"op\":0,\"s\":4,\"t\":\"GUILD_MEMBERS_CHUNK\","
            "\"d\":{\"guild_id\":\"9223372036854775840\",\"members\":[],"
            "\"chunk_index\":0,\"chunk_count\":1,\"nonce\":\"chunk-1\"}}");
    if (ok)
        ok = ws_send_frame(
            ws_fd, 1,
            "{\"op\":0,\"s\":5,\"t\":\"VOICE_STATE_UPDATE\","
            "\"d\":{\"guild_id\":\"9223372036854775840\","
            "\"channel_id\":\"9223372036854775812\","
            "\"user_id\":\"9223372036854775842\",\"session_id\":\"vs1\"}}");
    if (ok)
        ok = ws_send_frame(
            ws_fd, 1,
            "{\"op\":0,\"s\":6,\"t\":\"WEBHOOKS_UPDATE\","
            "\"d\":{\"guild_id\":\"9223372036854775840\","
            "\"channel_id\":\"9223372036854775809\"}}");
    /* Non-command message for cache, followed by MESSAGE_DELETE_BULK */
    if (ok)
        ok = ws_send_frame(
            ws_fd, 1,
            "{\"op\":0,\"s\":7,\"t\":\"MESSAGE_CREATE\","
            "\"d\":{\"id\":\"9223372036854775850\","
            "\"channel_id\":\"9223372036854775809\","
            "\"guild_id\":\"9223372036854775840\","
            "\"content\":\"cached hello\","
            "\"author\":{\"id\":\"9223372036854775842\",\"username\":\"alice\",\"bot\":false}}}");
    if (ok)
        ok = ws_send_frame(
            ws_fd, 1,
            "{\"op\":0,\"s\":8,\"t\":\"MESSAGE_DELETE_BULK\","
            "\"d\":{\"ids\":[\"9223372036854775850\"],"
            "\"channel_id\":\"9223372036854775809\","
            "\"guild_id\":\"9223372036854775840\"}}");
    /* Bot !ping (must be ignored by human filter) */
    if (ok)
        ok = ws_send_frame(
            ws_fd, 1,
            "{\"op\":0,\"s\":9,\"t\":\"MESSAGE_CREATE\","
            "\"d\":{\"id\":\"9223372036854775851\","
            "\"channel_id\":\"9223372036854775809\","
            "\"content\":\"!ping\","
            "\"author\":{\"id\":\"9223372036854775843\",\"username\":\"otherbot\",\"bot\":true}}}");
    /* Webhook !ping (must be ignored by human filter) */
    if (ok)
        ok = ws_send_frame(
            ws_fd, 1,
            "{\"op\":0,\"s\":10,\"t\":\"MESSAGE_CREATE\","
            "\"d\":{\"id\":\"9223372036854775852\","
            "\"channel_id\":\"9223372036854775809\","
            "\"webhook_id\":\"9223372036854775844\","
            "\"content\":\"!ping\","
            "\"author\":{\"id\":\"9223372036854775844\",\"username\":\"hook\",\"bot\":false}}}");
    /* Human !ping with argument */
    if (ok)
        ok = ws_send_frame(
            ws_fd, 1,
            "{\"op\":0,\"s\":11,\"t\":\"MESSAGE_CREATE\","
            "\"d\":{\"id\":\"9223372036854775853\","
            "\"channel_id\":\"9223372036854775809\","
            "\"content\":\"!ping   fluxer\","
            "\"author\":{\"id\":\"9223372036854775842\",\"username\":\"alice\",\"bot\":false}}}");

    if (ok) {
        char close_buf[128];
        ok = ws_read_frame(ws_fd, close_buf, sizeof(close_buf), &opcode)
             && opcode == 8;
        if (ok) {
            const unsigned char close_reply[] = { 0x88, 0x02, 0x03, 0xe8 };
            ok = ws_write_exact(ws_fd, close_reply, sizeof(close_reply));
        }
    }

    free(hello);
    free(ready);
    close(ws_fd);
    close(srv->fd);
    srv->failed = !ok;
    return NULL;
}

static void
ev_on_ready(struct discord *client, const struct discord_ready *event)
{
    (void)event;
    ++g_ev_state.ready_calls;

    struct discord_presence_update presence = {
        .status = "dnd",
        .since = UINT64_C(1700000000000),
        .afk = false,
    };
    struct discord_activity act = {
        .name = "Fluxer",
        .type = DISCORD_ACTIVITY_GAME,
    };
    discord_presence_add_activity(&presence, &act);
    discord_update_presence(client, &presence);
    free(presence.activities->array);
    free(presence.activities);

    struct discord_update_voice_state join_vs = {
        .guild_id = UINT64_C(9223372036854775840),
        .channel_id = UINT64_C(9223372036854775812),
        .self_mute = true,
        .self_deaf = false,
    };
    discord_update_voice_state(client, &join_vs);

    struct discord_update_voice_state leave_vs = {
        .guild_id = UINT64_C(9223372036854775840),
        .channel_id = 0,
        .self_mute = false,
        .self_deaf = false,
    };
    discord_update_voice_state(client, &leave_vs);

    struct discord_request_guild_members req = {
        .guild_id = UINT64_C(9223372036854775840),
        .query = "",
        .limit = 0,
        .presences = true,
        .nonce = "chunk-1",
    };
    discord_request_guild_members(client, &req);
}

static void
ev_on_guild_create(struct discord *client, const struct discord_guild *event)
{
    ++g_ev_state.guild_create_calls;
    const struct discord_guild *cached =
        discord_cache_get_guild(client, event->id);
    if (cached) {
        g_ev_state.cached_guild_id = cached->id;
        discord_unclaim(client, cached);
    }
}

static void
ev_on_typing(struct discord *client, const struct discord_typing_start *event)
{
    (void)client;
    ++g_ev_state.typing_calls;
    g_ev_state.typing_timestamp_ms = event->timestamp;
}

static void
ev_on_chunk(struct discord *client,
            const struct discord_guild_members_chunk *event)
{
    (void)client;
    if (event->guild_id == UINT64_C(9223372036854775840) && event->nonce
        && strcmp(event->nonce, "chunk-1") == 0)
    {
        ++g_ev_state.chunk_calls;
    }
}

static void
ev_on_voice_state(struct discord *client,
                  const struct discord_voice_state *event)
{
    (void)client;
    if (event->channel_id == UINT64_C(9223372036854775812))
        ++g_ev_state.voice_state_calls;
}

static void
ev_on_webhooks(struct discord *client,
               const struct discord_webhooks_update *event)
{
    (void)client;
    if (event->channel_id == UINT64_C(9223372036854775809))
        ++g_ev_state.webhooks_calls;
}

static void
ev_on_message_create(struct discord *client,
                     const struct discord_message *event)
{
    const struct discord_message *cached = discord_cache_get_channel_message(
        client, event->channel_id, event->id);
    if (cached && cached->content
        && strcmp(cached->content, "cached hello") == 0)
    {
        g_ev_state.cache_hit_before_bulk_delete = 1;
        discord_unclaim(client, cached);
    }
}

static void
ev_on_message_delete_bulk(struct discord *client,
                          const struct discord_message_delete_bulk *event)
{
    const struct discord_message *cached = discord_cache_get_channel_message(
        client, event->channel_id, UINT64_C(9223372036854775850));
    if (cached == NULL) {
        g_ev_state.cache_miss_after_bulk_delete = 1;
    }
    else {
        discord_unclaim(client, cached);
    }
}

static void
ev_on_ping_command(struct discord *client, const struct discord_message *event)
{
    if (!event || event->webhook_id != 0 || !event->author
        || event->author->bot)
    {
        ++g_ev_state.ignored_bot_or_webhook_pings;
        return;
    }
    ++g_ev_state.human_ping_calls;
    snprintf(g_ev_state.ping_arg, sizeof(g_ev_state.ping_arg), "%s",
             event->content ? event->content : "");
    discord_shutdown(client);
}

static time_t g_ev_deadline;
static void
ev_on_idle_watchdog(struct discord *client)
{
    if (time(NULL) >= g_ev_deadline) discord_shutdown(client);
}

TEST
gateway_opcodes_dispatch_prefix_commands_and_cache_work_end_to_end(void)
{
    char *user_json = NULL;
    ASSERT(load_file("test/fixtures/fluxer-user.json", &user_json) > 0);

    struct gw_events_server srv = { 0 };
    struct sockaddr_in addr = { .sin_family = AF_INET,
                                .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
                                .sin_port = 0 };
    socklen_t addr_len = sizeof(addr);
    srv.fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT(srv.fd >= 0);
    ASSERT_EQ(0, bind(srv.fd, (struct sockaddr *)&addr, sizeof(addr)));
    ASSERT_EQ(0, listen(srv.fd, 8));
    ASSERT_EQ(0, getsockname(srv.fd, (struct sockaddr *)&addr, &addr_len));
    srv.port = ntohs(addr.sin_port);
    struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(srv.fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char origin[64];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", srv.port);
    snprintf(srv.discovery_json, sizeof(srv.discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://127.0.0.1:%u/gateway\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin, srv.port);
    snprintf(srv.gateway_bot_json, sizeof(srv.gateway_bot_json),
             "{\"url\":\"ws://127.0.0.1:%u/gateway\",\"shards\":1,"
             "\"session_start_limit\":{\"total\":1000,\"remaining\":999,"
             "\"reset_after\":14400000,\"max_concurrency\":1}}",
             srv.port);
    srv.user_json = user_json;

    memset(&g_ev_state, 0, sizeof(g_ev_state));
    pthread_t srv_tid;
    ASSERT_EQ(0, pthread_create(&srv_tid, NULL, gw_events_server_thread, &srv));

    struct discord_config cfg = {
        .token = (char *)TEST_TOKEN,
        .instance_origin = origin,
        .log = { .quiet = true, .level = LOGMOD_LEVEL_FATAL },
    };
    struct discord *client = discord_from_config(&cfg);
    ASSERT(client != NULL);

    discord_cache_enable(client, DISCORD_CACHE_GUILDS | DISCORD_CACHE_MESSAGES);
    discord_add_intents(client, DISCORD_GATEWAY_MESSAGE_CONTENT);
    discord_set_prefix(client, "!");
    const char *cmds[] = { "ping", "p" };
    discord_set_on_commands(client, cmds, 2, &ev_on_ping_command);

    discord_set_on_ready(client, &ev_on_ready);
    discord_set_on_guild_create(client, &ev_on_guild_create);
    discord_set_on_typing_start(client, &ev_on_typing);
    discord_set_on_guild_members_chunk(client, &ev_on_chunk);
    discord_set_on_voice_state_update(client, &ev_on_voice_state);
    discord_set_on_webhooks_update(client, &ev_on_webhooks);
    discord_set_on_message_create(client, &ev_on_message_create);
    discord_set_on_message_delete_bulk(client, &ev_on_message_delete_bulk);
    discord_set_on_idle(client, &ev_on_idle_watchdog);
    client->gw.session->retry.limit = 1;
    g_ev_deadline = time(NULL) + 5;

    CCORDcode code = discord_run(client);
    ASSERT_EQ(0, pthread_join(srv_tid, NULL));
    ASSERT_EQ(0, srv.failed);
    ASSERT(code == CCORD_OK || code == CCORD_DISCORD_CONNECTION);

    /* Verify IDENTIFY omitted intents despite discord_add_intents() */
    ASSERT(strstr(srv.identify_frame, "\"op\":2") != NULL);
    ASSERT(strstr(srv.identify_frame, "\"intents\"") == NULL);

    /* Verify Opcode 3 PresenceUpdate */
    ASSERT_STR_EQ(
        "{\"op\":3,\"d\":{\"since\":1700000000000,"
        "\"activities\":[{\"name\":\"Fluxer\",\"type\":0}],"
        "\"status\":\"dnd\",\"afk\":false}}",
        srv.presence_frame);

    /* Verify Opcode 4 VoiceStateUpdate (join + disconnect null) */
    ASSERT_STR_EQ(
        "{\"op\":4,\"d\":{\"guild_id\":\"9223372036854775840\","
        "\"channel_id\":\"9223372036854775812\","
        "\"self_mute\":true,\"self_deaf\":false}}",
        srv.voice_join_frame);
    ASSERT_STR_EQ(
        "{\"op\":4,\"d\":{\"guild_id\":\"9223372036854775840\","
        "\"channel_id\":null,\"self_mute\":false,\"self_deaf\":false}}",
        srv.voice_leave_frame);

    /* Verify Opcode 8 RequestGuildMembers */
    ASSERT_STR_EQ(
        "{\"op\":8,\"d\":{\"guild_id\":\"9223372036854775840\","
        "\"query\":\"\",\"limit\":0,\"presences\":true,\"nonce\":\"chunk-1\"}}",
        srv.req_members_frame);

    /* Verify dispatched events, cache, and prefix command filtering */
    ASSERT_EQ(1, g_ev_state.ready_calls);
    ASSERT_EQ(1, g_ev_state.guild_create_calls);
    ASSERT_EQ(UINT64_C(9223372036854775840), g_ev_state.cached_guild_id);
    ASSERT_EQ(1, g_ev_state.typing_calls);
    ASSERT_EQ(UINT64_C(1700000123000), g_ev_state.typing_timestamp_ms);
    ASSERT_EQ(1, g_ev_state.chunk_calls);
    ASSERT_EQ(1, g_ev_state.voice_state_calls);
    ASSERT_EQ(1, g_ev_state.webhooks_calls);
    ASSERT_EQ(1, g_ev_state.cache_hit_before_bulk_delete);
    ASSERT_EQ(1, g_ev_state.cache_miss_after_bulk_delete);
    ASSERT_EQ(2, g_ev_state.ignored_bot_or_webhook_pings);
    ASSERT_EQ(1, g_ev_state.human_ping_calls);
    ASSERT_STR_EQ("fluxer", g_ev_state.ping_arg);

    discord_cleanup(client);
    free(user_json);
    PASS();
}

TEST
fluxer_docs_and_example_have_no_hardcoded_tokens_or_false_slash_claims(void)
{
    char *example_src = NULL, *fluxer_doc = NULL;
    ASSERT(load_file("examples/fluxer-ping.c", &example_src) > 0
           || load_file("../examples/fluxer-ping.c", &example_src) > 0);
    ASSERT(load_file("docs/FLUXER.md", &fluxer_doc) > 0
           || load_file("../docs/FLUXER.md", &fluxer_doc) > 0);

    ASSERT(strstr(example_src, "FLUXER_TOKEN") != NULL);
    ASSERT(strstr(example_src, "FLUXER_INSTANCE_ORIGIN") != NULL);
    ASSERT(strstr(example_src, "webhook_id") != NULL);
    ASSERT(strstr(example_src, "author->bot") != NULL);
    ASSERT(strstr(example_src, "discord_add_intents") == NULL);
    ASSERT(strstr(example_src, "application_command") == NULL);

    ASSERT(strstr(fluxer_doc, "/.well-known/fluxer") != NULL);
    ASSERT(strstr(fluxer_doc, "discord_set_instance_origin") != NULL);
    ASSERT(strstr(fluxer_doc, "CCORD_UNSUPPORTED") != NULL);
    ASSERT(strstr(fluxer_doc, "FLUXER_COMPATIBILITY.md") != NULL);

    free(example_src);
    free(fluxer_doc);
    PASS();
}

GREATEST_MAIN_DEFS();

int
main(int argc, char *argv[])
{
    GREATEST_MAIN_BEGIN();

    RUN_TEST(
        all_application_command_and_interaction_apis_return_unsupported_without_http);
    RUN_TEST(oauth2_and_gateway_rest_routes_match_fluxer_v1);
    RUN_TEST(instance_origin_setter_and_client_helpers_work_before_gateway_run);
    RUN_TEST(gateway_opcodes_dispatch_prefix_commands_and_cache_work_end_to_end);
    RUN_TEST(
        fluxer_docs_and_example_have_no_hardcoded_tokens_or_false_slash_claims);

    GREATEST_MAIN_END();
}
