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
#include "discord-worker.h"
#include "greatest.h"

#define MOCK_MAX_REQUESTS 24
#define MOCK_BUFFER_SIZE  16384

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

struct done_json_context {
    int calls;
    CCORDcode code;
    char body[1024];
};

static void
on_execute_done(struct discord *client, struct discord_response *response)
{
    (void)client;
    struct done_json_context *ctx = response->data;
    ++ctx->calls;
    ctx->code = response->code;
    size_t len = response->json.size < sizeof(ctx->body) - 1
                     ? response->json.size
                     : sizeof(ctx->body) - 1;
    if (response->json.start && len > 0)
        memcpy(ctx->body, response->json.start, len);
    ctx->body[len] = '\0';
}

struct http_mock {
    int fd;
    unsigned short port;
    size_t expected_requests;
    size_t received_requests;
    size_t unexpected_requests;
    const char *statuses[MOCK_MAX_REQUESTS];
    const char *bodies[MOCK_MAX_REQUESTS];
    char requests[MOCK_MAX_REQUESTS][MOCK_BUFFER_SIZE];
};

static int
mock_open(struct http_mock *mock, size_t expected_requests)
{
    struct sockaddr_in address = { .sin_family = AF_INET,
                                   .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
                                   .sin_port = 0 };
    socklen_t length = sizeof(address);
    memset(mock, 0, sizeof(*mock));
    mock->expected_requests = expected_requests;
    mock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (mock->fd < 0) return -1;
    if (bind(mock->fd, (struct sockaddr *)&address, sizeof(address)) < 0
        || listen(mock->fd, MOCK_MAX_REQUESTS) < 0
        || getsockname(mock->fd, (struct sockaddr *)&address, &length) < 0)
    {
        close(mock->fd);
        return -1;
    }
    mock->port = ntohs(address.sin_port);
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
        struct pollfd pfd = { .fd = mock->fd, .events = POLLIN };
        if (poll(&pfd, 1, 3000) <= 0) break;
        int connection = accept(mock->fd, NULL, NULL);
        if (connection < 0) break;
        ++mock->received_requests;

        char *request = mock->requests[i];
        size_t used = 0, header_size = 0, expected_size = 0;
        while (used + 1 < MOCK_BUFFER_SIZE) {
            struct pollfd rpfd = { .fd = connection, .events = POLLIN };
            if (poll(&rpfd, 1, 1000) <= 0) break;
            ssize_t count =
                recv(connection, request + used, MOCK_BUFFER_SIZE - used - 1, 0);
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

    /* Check briefly that no extra unexpected requests were made */
    struct pollfd extra_pfd = { .fd = mock->fd, .events = POLLIN };
    while (poll(&extra_pfd, 1, 50) > 0) {
        int connection = accept(mock->fd, NULL, NULL);
        if (connection < 0) break;
        ++mock->unexpected_requests;
        close(connection);
    }

    close(mock->fd);
    return NULL;
}

static bool
write_bot_config(const char origin[], char path[])
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

static bool
write_tokenless_config(const char origin[], char path[])
{
    char json[512];
    int length = snprintf(json, sizeof(json),
                          "{\"discord\":{},\"logging\":{\"quiet\":true},"
                          "\"instance_origin\":\"%s\"}",
                          origin);
    if (length < 0 || (size_t)length >= sizeof(json)) return false;
    int fd = mkstemp(path);
    if (fd < 0) return false;
    ssize_t written = write(fd, json, (size_t)length);
    close(fd);
    return written == length;
}

static const char k_user_json[] =
    "{\"id\":\"9223372036854775815\",\"username\":\"fluxer-bot\","
    "\"discriminator\":\"0001\",\"bot\":true}";

static const char k_webhook_json[] =
    "{\"id\":\"9223372036854775811\",\"type\":1,"
    "\"guild_id\":\"9223372036854775812\","
    "\"channel_id\":\"9223372036854775809\","
    "\"name\":\"Deploy Hook\",\"avatar\":\"a_fluxer_avatar\","
    "\"token\":\"wh_secret_token_64chars\","
    "\"user\":{\"id\":\"9223372036854775815\",\"username\":\"fluxer-bot\"}}";

static const char k_webhook_list_json[] =
    "[{\"id\":\"9223372036854775811\",\"type\":2,"
    "\"guild_id\":\"9223372036854775812\","
    "\"channel_id\":\"9223372036854775809\","
    "\"name\":\"Deploy Hook\",\"avatar\":\"a_fluxer_avatar\","
    "\"source_guild\":{\"id\":\"9223372036854775850\",\"name\":\"Src Guild\"},"
    "\"source_channel\":{\"id\":\"9223372036854775851\",\"name\":\"news\"}}]";

static const char k_token_webhook_json[] =
    "{\"id\":\"9223372036854775811\",\"type\":1,"
    "\"guild_id\":\"9223372036854775812\","
    "\"channel_id\":\"9223372036854775809\","
    "\"name\":\"Deploy Hook\",\"avatar\":\"a_fluxer_avatar\","
    "\"token\":\"wh_secret_token_64chars\"}";

static const char k_webhook_message_json[] =
    "{\"id\":\"9223372036854775830\","
    "\"channel_id\":\"9223372036854775809\","
    "\"webhook_id\":\"9223372036854775811\","
    "\"content\":\"webhook hello\","
    "\"author\":{\"id\":\"9223372036854775811\",\"username\":\"Deploy Hook\","
    "\"bot\":true}}";

TEST
bot_webhook_crud_uses_fluxer_routes_auth_and_large_snowflakes(void)
{
    struct http_mock mock;
    ASSERT_EQ(0, mock_open(&mock, 9));

    char origin[64], discovery_json[512];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", mock.port);
    snprintf(discovery_json, sizeof(discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);

    mock.bodies[0] = discovery_json;
    mock.bodies[1] = k_user_json;
    mock.bodies[2] = k_webhook_json;      /* POST /channels/{id}/webhooks */
    mock.bodies[3] = k_webhook_list_json; /* GET /channels/{id}/webhooks */
    mock.bodies[4] = k_webhook_list_json; /* GET /guilds/{id}/webhooks */
    mock.bodies[5] = k_webhook_json;      /* GET /webhooks/{id} */
    mock.bodies[6] = k_webhook_json;      /* PATCH /webhooks/{id} (name) */
    mock.bodies[7] = k_webhook_json;      /* PATCH /webhooks/{id} (channel) */
    mock.statuses[8] = "204 No Content";  /* DELETE /webhooks/{id} */
    mock.bodies[8] = "";

    char config_path[] = "/tmp/concord-fluxer-wh-bot-XXXXXX";
    ASSERT(write_bot_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    const u64snowflake channel_id = UINT64_C(9223372036854775809);
    const u64snowflake guild_id = UINT64_C(9223372036854775812);
    const u64snowflake webhook_id = UINT64_C(9223372036854775811);
    const u64snowflake moved_channel_id = UINT64_C(9223372036854775820);

    struct discord_webhook wh = { 0 };
    struct discord_ret_webhook wh_ret = { .sync = &wh };
    struct discord_create_webhook create_params = {
        .reason = "setup hook",
        .name = "Deploy Hook",
        .avatar = "data:image/png;base64,AAAA",
    };
    ASSERT_EQ(CCORD_OK,
              discord_create_webhook(client, channel_id, &create_params,
                                     &wh_ret));
    ASSERT_EQ(webhook_id, wh.id);
    ASSERT_EQ(guild_id, wh.guild_id);
    ASSERT_EQ(channel_id, wh.channel_id);
    ASSERT_STR_EQ("Deploy Hook", wh.name);
    ASSERT_STR_EQ("a_fluxer_avatar", wh.avatar);
    ASSERT_STR_EQ("wh_secret_token_64chars", wh.token);
    ASSERT(wh.user != NULL);
    ASSERT_EQ(UINT64_C(9223372036854775815), wh.user->id);
    discord_webhook_cleanup(&wh);

    struct discord_webhooks list = { 0 };
    struct discord_ret_webhooks list_ret = { .sync = &list };
    ASSERT_EQ(CCORD_OK,
              discord_get_channel_webhooks(client, channel_id, &list_ret));
    ASSERT_EQ(1, list.size);
    ASSERT_EQ(webhook_id, list.array[0].id);
    ASSERT(list.array[0].source_guild != NULL);
    ASSERT_EQ(UINT64_C(9223372036854775850), list.array[0].source_guild->id);
    ASSERT(list.array[0].source_channel != NULL);
    ASSERT_EQ(UINT64_C(9223372036854775851), list.array[0].source_channel->id);
    discord_webhooks_cleanup(&list);

    memset(&list, 0, sizeof(list));
    list_ret = (struct discord_ret_webhooks){ .sync = &list };
    ASSERT_EQ(CCORD_OK,
              discord_get_guild_webhooks(client, guild_id, &list_ret));
    ASSERT_EQ(1, list.size);
    ASSERT_EQ(guild_id, list.array[0].guild_id);
    discord_webhooks_cleanup(&list);

    memset(&wh, 0, sizeof(wh));
    wh_ret = (struct discord_ret_webhook){ .sync = &wh };
    ASSERT_EQ(CCORD_OK, discord_get_webhook(client, webhook_id, &wh_ret));
    ASSERT_EQ(webhook_id, wh.id);
    discord_webhook_cleanup(&wh);

    memset(&wh, 0, sizeof(wh));
    wh_ret = (struct discord_ret_webhook){ .sync = &wh };
    struct discord_modify_webhook mod_name = {
        .reason = "rename hook",
        .name = "Renamed Hook",
    };
    ASSERT_EQ(CCORD_OK,
              discord_modify_webhook(client, webhook_id, &mod_name, &wh_ret));
    discord_webhook_cleanup(&wh);

    memset(&wh, 0, sizeof(wh));
    wh_ret = (struct discord_ret_webhook){ .sync = &wh };
    struct discord_modify_webhook mod_chan = {
        .channel_id = moved_channel_id,
    };
    ASSERT_EQ(CCORD_OK,
              discord_modify_webhook(client, webhook_id, &mod_chan, &wh_ret));
    discord_webhook_cleanup(&wh);

    struct discord_delete_webhook del_params = { .reason = "cleanup hook" };
    struct discord_ret del_ret = { .sync = true };
    ASSERT_EQ(CCORD_OK,
              discord_delete_webhook(client, webhook_id, &del_params,
                                     &del_ret));

    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    ASSERT_EQ(9, mock.received_requests);
    ASSERT_EQ(0, mock.unexpected_requests);

    ASSERT(strstr(mock.requests[2],
                  "POST /api-root/v1/channels/9223372036854775809/webhooks ")
           != NULL);
    ASSERT(strstr(mock.requests[2], "Authorization: Bot ") != NULL);
    ASSERT(strstr(mock.requests[2], "X-Audit-Log-Reason: setup hook") != NULL);
    ASSERT(strstr(mock.requests[2], "\"name\":\"Deploy Hook\"") != NULL);

    ASSERT(strstr(mock.requests[3],
                  "GET /api-root/v1/channels/9223372036854775809/webhooks ")
           != NULL);
    ASSERT(strstr(mock.requests[3], "Authorization: Bot ") != NULL);

    ASSERT(strstr(mock.requests[4],
                  "GET /api-root/v1/guilds/9223372036854775812/webhooks ")
           != NULL);
    ASSERT(strstr(mock.requests[4], "Authorization: Bot ") != NULL);

    ASSERT(strstr(mock.requests[5],
                  "GET /api-root/v1/webhooks/9223372036854775811 ") != NULL);
    ASSERT(strstr(mock.requests[5], "Authorization: Bot ") != NULL);

    ASSERT(strstr(mock.requests[6],
                  "PATCH /api-root/v1/webhooks/9223372036854775811 ") != NULL);
    ASSERT(strstr(mock.requests[6], "Authorization: Bot ") != NULL);
    ASSERT(strstr(mock.requests[6], "X-Audit-Log-Reason: rename hook") != NULL);
    ASSERT(strstr(mock.requests[6], "\"name\":\"Renamed Hook\"") != NULL);
    ASSERT(strstr(mock.requests[6], "\"channel_id\"") == NULL);

    ASSERT(strstr(mock.requests[7],
                  "PATCH /api-root/v1/webhooks/9223372036854775811 ") != NULL);
    ASSERT(strstr(mock.requests[7],
                  "\"channel_id\":\"9223372036854775820\"") != NULL);
    ASSERT(strstr(mock.requests[7], "\"name\"") == NULL);

    ASSERT(strstr(mock.requests[8],
                  "DELETE /api-root/v1/webhooks/9223372036854775811 ") != NULL);
    ASSERT(strstr(mock.requests[8], "Authorization: Bot ") != NULL);
    ASSERT(strstr(mock.requests[8], "X-Audit-Log-Reason: cleanup hook")
           != NULL);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

TEST
token_webhook_routes_omit_authorization_and_support_tokenless_client(void)
{
    struct http_mock mock;
    /* 1 discovery + 8 token webhook requests (no /users/@me on tokenless!) */
    ASSERT_EQ(0, mock_open(&mock, 9));

    char origin[64], discovery_json[512];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", mock.port);
    snprintf(discovery_json, sizeof(discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);

    mock.bodies[0] = discovery_json;
    mock.bodies[1] = k_token_webhook_json;   /* GET /webhooks/{id}/{token} */
    mock.bodies[2] = k_token_webhook_json;   /* PATCH /webhooks/{id}/{token} */
    mock.bodies[3] = k_webhook_message_json; /* POST execute ?wait=true&thread_id=... */
    mock.statuses[4] = "204 No Content";     /* POST execute multipart */
    mock.bodies[4] = "";
    mock.bodies[5] = k_webhook_message_json; /* GET /webhooks/{id}/{tok}/messages/{mid} */
    mock.bodies[6] = k_webhook_message_json; /* PATCH /webhooks/{id}/{tok}/messages/{mid} */
    mock.statuses[7] = "204 No Content";     /* DELETE /webhooks/{id}/{tok}/messages/{mid} */
    mock.bodies[7] = "";
    mock.statuses[8] = "204 No Content";     /* DELETE /webhooks/{id}/{tok} */
    mock.bodies[8] = "";

    char config_path[] = "/tmp/concord-fluxer-wh-tok-XXXXXX";
    ASSERT(write_tokenless_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    const u64snowflake webhook_id = UINT64_C(9223372036854775811);
    const u64snowflake thread_id = UINT64_C(9223372036854775825);
    const u64snowflake message_id = UINT64_C(9223372036854775830);
    const char *wh_token = "wh_secret_token_64chars";

    /* Bot-only route on tokenless client must fail cleanly with zero network */
    struct unsupported_context auth_ctx = { 0 };
    struct discord_ret_webhook bad_auth_ret = {
        .data = &auth_ctx,
        .keep = &auth_ctx,
        .fail = &on_unsupported,
    };
    ASSERT_EQ(CCORD_DISCORD_BAD_AUTH,
              discord_get_webhook(client, webhook_id, &bad_auth_ret));
    ASSERT_EQ(1, auth_ctx.calls);
    ASSERT_EQ(CCORD_DISCORD_BAD_AUTH, auth_ctx.code);

    struct discord_webhook wh = { 0 };
    struct discord_ret_webhook wh_ret = { .sync = &wh };
    ASSERT_EQ(CCORD_OK,
              discord_get_webhook_with_token(client, webhook_id, wh_token,
                                             &wh_ret));
    ASSERT_EQ(webhook_id, wh.id);
    discord_webhook_cleanup(&wh);

    memset(&wh, 0, sizeof(wh));
    wh_ret = (struct discord_ret_webhook){ .sync = &wh };
    struct discord_modify_webhook_with_token mod_tok = {
        .name = "Token Renamed",
    };
    ASSERT_EQ(CCORD_OK,
              discord_modify_webhook_with_token(client, webhook_id, wh_token,
                                                &mod_tok, &wh_ret));
    discord_webhook_cleanup(&wh);

    struct done_json_context exec_ctx = { 0 };
    struct discord_ret exec_ret = {
        .sync = true,
        .data = &exec_ctx,
        .done = &on_execute_done,
    };
    struct discord_execute_webhook exec_params = {
        .wait = true,
        .thread_id = thread_id,
        .content = "webhook hello",
    };
    ASSERT_EQ(CCORD_OK,
              discord_execute_webhook(client, webhook_id, wh_token,
                                      &exec_params, &exec_ret));
    ASSERT_EQ(1, exec_ctx.calls);
    ASSERT(strstr(exec_ctx.body, "9223372036854775830") != NULL);

    struct discord_attachment file_part = {
        .content = "hello attachment",
        .size = sizeof("hello attachment") - 1,
        .filename = "note.txt",
        .content_type = "text/plain",
    };
    struct discord_attachments attachments = {
        .size = 1,
        .array = &file_part,
    };
    struct discord_execute_webhook exec_mime = {
        .content = "with file",
        .attachments = &attachments,
    };
    struct discord_ret mime_ret = { .sync = true };
    ASSERT_EQ(CCORD_OK,
              discord_execute_webhook(client, webhook_id, wh_token, &exec_mime,
                                      &mime_ret));

    struct discord_message msg = { 0 };
    struct discord_ret_message msg_ret = { .sync = &msg };
    ASSERT_EQ(CCORD_OK,
              discord_get_webhook_message(client, webhook_id, wh_token,
                                          message_id, &msg_ret));
    ASSERT_EQ(message_id, msg.id);
    ASSERT_EQ(webhook_id, msg.webhook_id);
    discord_message_cleanup(&msg);

    memset(&msg, 0, sizeof(msg));
    msg_ret = (struct discord_ret_message){ .sync = &msg };
    struct discord_edit_webhook_message edit_params = {
        .thread_id = thread_id,
        .content = "webhook edited",
    };
    ASSERT_EQ(CCORD_OK,
              discord_edit_webhook_message(client, webhook_id, wh_token,
                                           message_id, &edit_params, &msg_ret));
    ASSERT_EQ(message_id, msg.id);
    discord_message_cleanup(&msg);

    struct discord_ret del_msg_ret = { .sync = true };
    ASSERT_EQ(CCORD_OK,
              discord_delete_webhook_message(client, webhook_id, wh_token,
                                             message_id, &del_msg_ret));

    struct discord_ret del_wh_ret = { .sync = true };
    ASSERT_EQ(CCORD_OK,
              discord_delete_webhook_with_token(client, webhook_id, wh_token,
                                                &del_wh_ret));

    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    ASSERT_EQ(9, mock.received_requests);
    ASSERT_EQ(0, mock.unexpected_requests);

    for (size_t i = 0; i < 9; ++i) {
        ASSERT(strstr(mock.requests[i], "Authorization:") == NULL);
    }

    ASSERT(strstr(mock.requests[1],
                  "GET /api-root/v1/webhooks/9223372036854775811/wh_secret_token_64chars ")
           != NULL);
    ASSERT(strstr(mock.requests[2],
                  "PATCH /api-root/v1/webhooks/9223372036854775811/wh_secret_token_64chars ")
           != NULL);
    ASSERT(strstr(mock.requests[2], "\"name\":\"Token Renamed\"") != NULL);
    ASSERT(strstr(mock.requests[2], "\"avatar\"") == NULL);

    ASSERT(strstr(mock.requests[3],
                  "POST /api-root/v1/webhooks/9223372036854775811/wh_secret_token_64chars?wait=true&thread_id=9223372036854775825 ")
           != NULL);
    ASSERT(strstr(mock.requests[3], "\"content\":\"webhook hello\"") != NULL);
    ASSERT(strstr(mock.requests[3], "\"username\"") == NULL);
    ASSERT(strstr(mock.requests[3], "\"avatar_url\"") == NULL);

    ASSERT(strstr(mock.requests[4],
                  "POST /api-root/v1/webhooks/9223372036854775811/wh_secret_token_64chars ")
           != NULL);
    ASSERT(strstr(mock.requests[4], "multipart/form-data") != NULL);
    ASSERT(strstr(mock.requests[4], "payload_json") != NULL);
    ASSERT(strstr(mock.requests[4], "hello attachment") != NULL);

    ASSERT(strstr(mock.requests[5],
                  "GET /api-root/v1/webhooks/9223372036854775811/wh_secret_token_64chars/messages/9223372036854775830 ")
           != NULL);
    ASSERT(strstr(mock.requests[6],
                  "PATCH /api-root/v1/webhooks/9223372036854775811/wh_secret_token_64chars/messages/9223372036854775830?thread_id=9223372036854775825 ")
           != NULL);
    ASSERT(strstr(mock.requests[6], "\"content\":\"webhook edited\"") != NULL);
    ASSERT(strstr(mock.requests[7],
                  "DELETE /api-root/v1/webhooks/9223372036854775811/wh_secret_token_64chars/messages/9223372036854775830 ")
           != NULL);
    ASSERT(strstr(mock.requests[8],
                  "DELETE /api-root/v1/webhooks/9223372036854775811/wh_secret_token_64chars ")
           != NULL);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

TEST
bot_client_also_omits_authorization_on_token_webhook_routes(void)
{
    struct http_mock mock;
    ASSERT_EQ(0, mock_open(&mock, 3));

    char origin[64], discovery_json[512];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", mock.port);
    snprintf(discovery_json, sizeof(discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);

    mock.bodies[0] = discovery_json;
    mock.bodies[1] = k_user_json;
    mock.statuses[2] = "204 No Content";
    mock.bodies[2] = "";

    char config_path[] = "/tmp/concord-fluxer-wh-mixed-XXXXXX";
    ASSERT(write_bot_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    struct discord_execute_webhook exec_params = { .content = "ping via hook" };
    struct discord_ret exec_ret = { .sync = true };
    ASSERT_EQ(CCORD_OK,
              discord_execute_webhook(client, UINT64_C(9223372036854775811),
                                      "wh_secret_token_64chars", &exec_params,
                                      &exec_ret));

    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    ASSERT_EQ(3, mock.received_requests);
    ASSERT(strstr(mock.requests[1], "Authorization: Bot ") != NULL);
    ASSERT(strstr(mock.requests[2],
                  "POST /api-root/v1/webhooks/9223372036854775811/wh_secret_token_64chars ")
           != NULL);
    ASSERT(strstr(mock.requests[2], "Authorization:") == NULL);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

TEST
voice_stage_and_scheduled_event_apis_return_unsupported_without_network(void)
{
    struct http_mock mock;
    ASSERT_EQ(0, mock_open(&mock, 2));

    char origin[64], discovery_json[512];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", mock.port);
    snprintf(discovery_json, sizeof(discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);

    mock.bodies[0] = discovery_json;
    mock.bodies[1] = k_user_json;

    char config_path[] = "/tmp/concord-fluxer-unsup-wv-XXXXXX";
    ASSERT(write_bot_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    const u64snowflake guild_id = UINT64_C(9223372036854775812);
    const u64snowflake channel_id = UINT64_C(9223372036854775809);
    const u64snowflake event_id = UINT64_C(9223372036854775840);

    /* 1. Voice: discord_list_voice_regions */
    {
        struct unsupported_context ctx = { 0 };
        struct discord_ret_voice_regions ret = {
            .data = &ctx, .keep = &ctx, .fail = &on_unsupported
        };
        ASSERT_EQ(CCORD_UNSUPPORTED, discord_list_voice_regions(client, &ret));
        ASSERT_EQ(1, ctx.calls);
        ASSERT_EQ(CCORD_UNSUPPORTED, ctx.code);
        ASSERT_EQ(&ctx, ctx.data);
        ASSERT_EQ(&ctx, ctx.keep);
        ASSERT_EQ(CCORD_UNSUPPORTED, discord_list_voice_regions(client, NULL));
    }

    /* 2. Stage Instances (4 functions) */
    {
        struct unsupported_context ctx = { 0 };
        struct discord_ret_stage_instance sret = {
            .data = &ctx, .keep = &ctx, .fail = &on_unsupported
        };
        struct discord_create_stage_instance cparams = {
            .channel_id = channel_id, .topic = "Stage"
        };
        ASSERT_EQ(CCORD_UNSUPPORTED,
                  discord_create_stage_instance(client, &cparams, &sret));
        ASSERT_EQ(1, ctx.calls);
        ASSERT_EQ(CCORD_UNSUPPORTED, ctx.code);

        ASSERT_EQ(CCORD_UNSUPPORTED,
                  discord_get_stage_instance(client, channel_id, &sret));
        ASSERT_EQ(2, ctx.calls);

        struct discord_modify_stage_instance mparams = { .topic = "Updated" };
        ASSERT_EQ(CCORD_UNSUPPORTED,
                  discord_modify_stage_instance(client, channel_id, &mparams,
                                                &sret));
        ASSERT_EQ(3, ctx.calls);

        struct discord_ret dret = {
            .data = &ctx, .keep = &ctx, .fail = &on_unsupported
        };
        ASSERT_EQ(CCORD_UNSUPPORTED,
                  discord_delete_stage_instance(client, channel_id, NULL,
                                                &dret));
        ASSERT_EQ(4, ctx.calls);
    }

    /* 3. Guild Scheduled Events (6 functions) */
    {
        struct unsupported_context ctx = { 0 };
        struct discord_ret_guild_scheduled_events lret = {
            .data = &ctx, .keep = &ctx, .fail = &on_unsupported
        };
        ASSERT_EQ(CCORD_UNSUPPORTED,
                  discord_list_guild_scheduled_events(client, guild_id, NULL,
                                                      &lret));
        ASSERT_EQ(1, ctx.calls);
        ASSERT_EQ(CCORD_UNSUPPORTED, ctx.code);

        struct discord_ret_guild_scheduled_event eret = {
            .data = &ctx, .keep = &ctx, .fail = &on_unsupported
        };
        ASSERT_EQ(CCORD_UNSUPPORTED,
                  discord_create_guild_scheduled_event(client, guild_id, NULL,
                                                       &eret));
        ASSERT_EQ(2, ctx.calls);

        ASSERT_EQ(CCORD_UNSUPPORTED,
                  discord_get_guild_scheduled_event(client, guild_id, event_id,
                                                    NULL, &eret));
        ASSERT_EQ(3, ctx.calls);

        ASSERT_EQ(CCORD_UNSUPPORTED,
                  discord_modify_guild_scheduled_event(client, guild_id,
                                                       event_id, NULL, &eret));
        ASSERT_EQ(4, ctx.calls);

        struct discord_ret dret = {
            .data = &ctx, .keep = &ctx, .fail = &on_unsupported
        };
        ASSERT_EQ(CCORD_UNSUPPORTED,
                  discord_delete_guild_scheduled_event(client, guild_id,
                                                       event_id, &dret));
        ASSERT_EQ(5, ctx.calls);

        struct discord_ret_guild_scheduled_event_users uret = {
            .data = &ctx, .keep = &ctx, .fail = &on_unsupported
        };
        ASSERT_EQ(CCORD_UNSUPPORTED,
                  discord_get_guild_scheduled_event_users(client, guild_id,
                                                          event_id, NULL,
                                                          &uret));
        ASSERT_EQ(6, ctx.calls);
    }

    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    ASSERT_EQ(2, mock.received_requests);
    ASSERT_EQ(0, mock.unexpected_requests);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

struct async_webhook_context {
    int calls;
    CCORDcode code;
    u64snowflake id;
};

static void
on_async_webhook_done(struct discord *client,
                      struct discord_response *resp,
                      const struct discord_webhook *wh)
{
    (void)client;
    struct async_webhook_context *ctx = resp->data;
    ++ctx->calls;
    ctx->code = resp->code;
    ctx->id = wh ? wh->id : 0;
}

TEST
token_webhook_async_ratelimit_sync_flag_and_validation(void)
{
    struct http_mock mock;
    /* 1 discovery + 1 SYNC_FLAG + 2 (429 + retry) + 1 async = 5 requests */
    ASSERT_EQ(0, mock_open(&mock, 5));

    char origin[64], discovery_json[512];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", mock.port);
    snprintf(discovery_json, sizeof(discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);

    mock.bodies[0] = discovery_json;
    mock.bodies[1] = k_token_webhook_json;
    mock.statuses[2] = "429 Too Many Requests";
    mock.bodies[2] =
        "{\"code\":\"RATE_LIMITED\",\"message\":\"slow down\","
        "\"retry_after\":0.001}";
    mock.bodies[3] = k_token_webhook_json;
    mock.bodies[4] = k_token_webhook_json;

    char config_path[] = "/tmp/concord-fluxer-wh-async-XXXXXX";
    ASSERT(write_tokenless_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    const u64snowflake webhook_id = UINT64_C(9223372036854775811);
    const char *wh_token = "wh_secret_token_64chars";

    /* Parameter & path-injection validation (zero network traffic) */
    ASSERT_EQ(CCORD_BAD_PARAMETER,
              discord_get_webhook_with_token(client, 0, wh_token, NULL));
    ASSERT_EQ(CCORD_BAD_PARAMETER,
              discord_get_webhook_with_token(client, webhook_id, "", NULL));
    ASSERT_EQ(CCORD_BAD_PARAMETER,
              discord_get_webhook_with_token(client, webhook_id,
                                             "../users/@me", NULL));
    ASSERT_EQ(CCORD_BAD_PARAMETER,
              discord_execute_webhook(client, webhook_id, "tok?inject=1",
                                      NULL, NULL));

    /* DISCORD_SYNC_FLAG support (as used by examples/webhook.c) */
    struct discord_ret_webhook sync_flag_ret = { .sync = DISCORD_SYNC_FLAG };
    ASSERT_EQ(CCORD_OK,
              discord_get_webhook_with_token(client, webhook_id, wh_token,
                                             &sync_flag_ret));

    /* 429 retry_after handling */
    struct discord_webhook retried = { 0 };
    struct discord_ret_webhook retry_ret = { .sync = &retried };
    ASSERT_EQ(CCORD_OK,
              discord_get_webhook_with_token(client, webhook_id, wh_token,
                                             &retry_ret));
    ASSERT_EQ(webhook_id, retried.id);
    discord_webhook_cleanup(&retried);

    /* Async completion dispatched through requestor finished queue */
    struct async_webhook_context async_ctx = { 0 };
    struct discord_ret_webhook async_ret = {
        .data = &async_ctx,
        .done = &on_async_webhook_done,
    };
    ASSERT_EQ(CCORD_PENDING,
              discord_get_webhook_with_token(client, webhook_id, wh_token,
                                             &async_ret));
    discord_worker_join(client);
    discord_requestor_dispatch_responses(&client->rest.requestor);
    ASSERT_EQ(1, async_ctx.calls);
    ASSERT_EQ(CCORD_OK, async_ctx.code);
    ASSERT_EQ(webhook_id, async_ctx.id);

    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    ASSERT_EQ(5, mock.received_requests);
    ASSERT_EQ(0, mock.unexpected_requests);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

GREATEST_MAIN_DEFS();

int
main(int argc, char *argv[])
{
    GREATEST_MAIN_BEGIN();

    RUN_TEST(bot_webhook_crud_uses_fluxer_routes_auth_and_large_snowflakes);
    RUN_TEST(token_webhook_routes_omit_authorization_and_support_tokenless_client);
    RUN_TEST(bot_client_also_omits_authorization_on_token_webhook_routes);
    RUN_TEST(token_webhook_async_ratelimit_sync_flag_and_validation);
    RUN_TEST(voice_stage_and_scheduled_event_apis_return_unsupported_without_network);

    GREATEST_MAIN_END();
}
