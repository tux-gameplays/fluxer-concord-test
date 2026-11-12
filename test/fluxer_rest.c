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

#define MOCK_REQUESTS 14
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

TEST
unsupported_error_is_delivered_to_async_failure_callback(void)
{
    struct unsupported_context context = { 0 };
    CCORDcode result = discord_rest_unsupported(
        NULL, &on_unsupported, &context, &context);
    ASSERT_EQ(CCORD_UNSUPPORTED, result);
    ASSERT_EQ(1, context.calls);
    ASSERT_EQ(CCORD_UNSUPPORTED, context.code);
    ASSERT_EQ(&context, context.data);
    ASSERT_EQ(&context, context.keep);
    PASS();
}

struct http_mock {
    int fd;
    unsigned short port;
    const char *statuses[MOCK_REQUESTS];
    const char *bodies[MOCK_REQUESTS];
    char requests[MOCK_REQUESTS][MOCK_BUFFER_SIZE];
};

static size_t
load_file(const char *path, char **contents)
{
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    char *buffer = malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return 0;
    }
    if (fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        return 0;
    }
    buffer[size] = '\0';
    fclose(file);
    *contents = buffer;
    return (size_t)size;
}

static int
mock_open(struct http_mock *mock)
{
    struct sockaddr_in address = { .sin_family = AF_INET,
                                   .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
                                   .sin_port = 0 };
    socklen_t length = sizeof(address);
    mock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (mock->fd < 0) return -1;
    if (bind(mock->fd, (struct sockaddr *)&address, sizeof(address)) < 0
        || listen(mock->fd, MOCK_REQUESTS) < 0
        || getsockname(mock->fd, (struct sockaddr *)&address, &length) < 0)
    {
        close(mock->fd);
        return -1;
    }
    mock->port = ntohs(address.sin_port);
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
    for (size_t i = 0; i < MOCK_REQUESTS; ++i) {
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
    }
    close(mock->fd);
    return NULL;
}

static bool
write_config(const char origin[], char path[])
{
    static const char token[] =
        "abcdefghijklmnopqrst.abcde.abcdefghijklmnopqrstuvwxy";
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

TEST
message_create_uses_fluxer_rest_and_decodes_large_snowflakes(void)
{
    char *user_json = NULL, *message_json = NULL;
    ASSERT(load_file("test/fixtures/fluxer-user.json", &user_json) > 0);
    ASSERT(load_file("test/fixtures/fluxer-message.json", &message_json) > 0);

    struct http_mock mock = { 0 };
    ASSERT_EQ(0, mock_open(&mock));
    char origin[64], discovery_json[512];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", mock.port);
    snprintf(discovery_json, sizeof(discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);
    for (size_t i = 0; i < MOCK_REQUESTS; ++i)
        mock.statuses[i] = "200 OK";
    char message_list[4096];
    snprintf(message_list, sizeof(message_list), "[%s]", message_json);
    static const char channel_json[] =
        "{\"id\":\"9223372036854775813\",\"type\":1}";
    mock.bodies[0] = discovery_json;
    mock.bodies[1] = user_json;
    mock.bodies[2] = message_json;
    mock.bodies[3] = user_json;
    mock.bodies[4] = user_json;
    mock.bodies[5] = "[]";
    mock.statuses[6] = "204 No Content";
    mock.bodies[6] = "";
    mock.bodies[7] = channel_json;
    mock.bodies[8] = message_list;
    mock.bodies[9] = message_json;
    mock.bodies[10] = message_json;
    mock.statuses[11] = "204 No Content";
    mock.bodies[11] = "";
    mock.statuses[12] = "429 Too Many Requests";
    mock.bodies[12] =
        "{\"code\":\"RATE_LIMITED\",\"message\":\"slow down\","
        "\"retry_after\":0.001,\"global\":false}";
    mock.bodies[13] = user_json;

    char config_path[] = "/tmp/concord-fluxer-rest-XXXXXX";
    ASSERT(write_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);
    struct discord_create_message params = { .content = "ping" };
    struct discord_message message = { 0 };
    struct discord_ret_message ret = { .sync = &message };
    const u64snowflake channel_id = UINT64_C(9223372036854775809);
    ASSERT_EQ(CCORD_OK,
              discord_create_message(client, channel_id, &params, &ret));
    ASSERT_EQ(UINT64_C(9223372036854775810), message.id);
    ASSERT_EQ(channel_id, message.channel_id);
    ASSERT_EQ(UINT64_C(9223372036854775811), message.webhook_id);
    ASSERT_STR_EQ("pong", message.content);

    struct discord_user user = { 0 };
    struct discord_ret_user user_ret = { .sync = &user };
    ASSERT_EQ(CCORD_OK, discord_get_current_user(client, &user_ret));
    discord_user_cleanup(&user);
    user_ret = (struct discord_ret_user){ .sync = &user };
    ASSERT_EQ(CCORD_OK,
              discord_get_user(client, UINT64_C(123456789012345678),
                               &user_ret));
    discord_user_cleanup(&user);

    struct discord_guilds guilds = { 0 };
    struct discord_ret_guilds guilds_ret = { .sync = &guilds };
    ASSERT_EQ(CCORD_OK, discord_get_current_user_guilds(client, &guilds_ret));
    discord_guilds_cleanup(&guilds);
    struct discord_ret leave_ret = { .sync = true };
    ASSERT_EQ(CCORD_OK, discord_leave_guild(
                            client, UINT64_C(123456789012345679), &leave_ret));

    struct discord_create_dm dm_params = {
        .recipient_id = UINT64_C(123456789012345680)
    };
    struct discord_channel dm = { 0 };
    struct discord_ret_channel dm_ret = { .sync = &dm };
    ASSERT_EQ(CCORD_OK, discord_create_dm(client, &dm_params, &dm_ret));
    discord_channel_cleanup(&dm);

    struct discord_messages messages = { 0 };
    struct discord_get_channel_messages get_messages = { 0 };
    struct discord_ret_messages messages_ret = { .sync = &messages };
    ASSERT_EQ(CCORD_OK, discord_get_channel_messages(
                            client, channel_id, &get_messages, &messages_ret));
    discord_messages_cleanup(&messages);
    struct discord_message fetched = { 0 };
    struct discord_ret_message fetched_ret = { .sync = &fetched };
    const u64snowflake message_id = UINT64_C(9223372036854775810);
    ASSERT_EQ(CCORD_OK, discord_get_channel_message(
                            client, channel_id, message_id, &fetched_ret));
    ASSERT_EQ(message_id, fetched.id);
    discord_message_cleanup(&fetched);
    struct discord_edit_message edit = { .content = "edited" };
    struct discord_message edited = { 0 };
    struct discord_ret_message edited_ret = { .sync = &edited };
    ASSERT_EQ(CCORD_OK, discord_edit_message(client, channel_id, message_id,
                                              &edit, &edited_ret));
    ASSERT_EQ(message_id, edited.id);
    discord_message_cleanup(&edited);
    struct discord_ret delete_ret = { .sync = true };
    ASSERT_EQ(CCORD_OK, discord_delete_message(client, channel_id, message_id,
                                                NULL, &delete_ret));

    struct discord_user retry_user = { 0 };
    struct discord_ret_user retry_user_ret = { .sync = &retry_user };
    ASSERT_EQ(CCORD_OK, discord_get_user(
                            client, UINT64_C(123456789012345678),
                            &retry_user_ret));
    ASSERT_EQ(UINT64_C(123456789012345678), retry_user.id);
    discord_user_cleanup(&retry_user);

    struct unsupported_context connection_context = { 0 };
    struct discord_ret_connections connections_ret = {
        .data = &connection_context,
        .keep = &connection_context,
        .fail = &on_unsupported,
    };
    ASSERT_EQ(CCORD_UNSUPPORTED,
              discord_get_user_connections(client, &connections_ret));
    ASSERT_EQ(1, connection_context.calls);
    ASSERT_EQ(CCORD_UNSUPPORTED, connection_context.code);

    struct unsupported_context profile_context = { 0 };
    struct discord_ret_user profile_ret = {
        .data = &profile_context,
        .keep = &profile_context,
        .fail = &on_unsupported,
    };
    ASSERT_EQ(CCORD_UNSUPPORTED,
              discord_modify_current_user(client, NULL, &profile_ret));
    ASSERT_EQ(1, profile_context.calls);
    ASSERT_EQ(CCORD_UNSUPPORTED, profile_context.code);

    struct unsupported_context group_context = { 0 };
    struct discord_ret_channel group_ret = {
        .data = &group_context,
        .keep = &group_context,
        .fail = &on_unsupported,
    };
    ASSERT_EQ(CCORD_UNSUPPORTED,
              discord_create_group_dm(client, NULL, &group_ret));
    ASSERT_EQ(1, group_context.calls);
    ASSERT_EQ(CCORD_UNSUPPORTED, group_context.code);

    ASSERT_EQ(0, pthread_join(server_thread, NULL));

    ASSERT(strstr(mock.requests[0], "GET /.well-known/fluxer ") != NULL);
    ASSERT(strstr(mock.requests[0], "Authorization:") == NULL);
    ASSERT(strstr(mock.requests[1], "GET /api-root/v1/users/@me ") != NULL);
    ASSERT(strstr(mock.requests[1], "Authorization: Bot ") != NULL);
    ASSERT(strstr(mock.requests[2],
                  "POST /api-root/v1/channels/9223372036854775809/messages ")
           != NULL);
    ASSERT(strstr(mock.requests[2], "Authorization: Bot ") != NULL);
    ASSERT(strstr(mock.requests[2], "\"content\":\"ping\"") != NULL);
    ASSERT(strstr(mock.requests[3], "GET /api-root/v1/users/@me ") != NULL);
    ASSERT(strstr(mock.requests[4],
                  "GET /api-root/v1/users/123456789012345678 ") != NULL);
    ASSERT(strstr(mock.requests[5],
                  "GET /api-root/v1/users/@me/guilds ") != NULL);
    ASSERT(strstr(mock.requests[6],
                  "DELETE /api-root/v1/users/@me/guilds/123456789012345679 ")
           != NULL);
    ASSERT(strstr(mock.requests[7],
                  "POST /api-root/v1/users/@me/channels ") != NULL);
    ASSERT(strstr(mock.requests[8],
                  "GET /api-root/v1/channels/9223372036854775809/messages ")
           != NULL);
    ASSERT(strstr(mock.requests[9],
                  "GET /api-root/v1/channels/9223372036854775809/messages/9223372036854775810 ")
           != NULL);
    ASSERT(strstr(mock.requests[10],
                  "PATCH /api-root/v1/channels/9223372036854775809/messages/9223372036854775810 ")
           != NULL);
    ASSERT(strstr(mock.requests[11],
                  "DELETE /api-root/v1/channels/9223372036854775809/messages/9223372036854775810 ")
           != NULL);
    ASSERT(strstr(mock.requests[12],
                  "GET /api-root/v1/users/123456789012345678 ") != NULL);
    ASSERT(strstr(mock.requests[13],
                  "GET /api-root/v1/users/123456789012345678 ") != NULL);

    discord_message_cleanup(&message);
    discord_cleanup(client);
    unlink(config_path);
    free(user_json);
    free(message_json);
    PASS();
}

GREATEST_MAIN_DEFS();

int
main(int argc, char *argv[])
{
    GREATEST_MAIN_BEGIN();

    RUN_TEST(unsupported_error_is_delivered_to_async_failure_callback);
    RUN_TEST(message_create_uses_fluxer_rest_and_decodes_large_snowflakes);

    GREATEST_MAIN_END();
}
