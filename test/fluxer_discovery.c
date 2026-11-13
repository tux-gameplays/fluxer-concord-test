#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "fluxer-discovery.h"
#include "discord.h"
#include "greatest.h"

struct fixture_server {
    int fd;
    unsigned short port;
    const char *status;
    const char *body;
    const char *second_status;
    const char *second_body;
    unsigned request_count;
    char requests[2][4096];
};

static int
fixture_server_open(struct fixture_server *server,
                    const char *status,
                    const char *body)
{
    struct sockaddr_in address = { .sin_family = AF_INET,
                                   .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
                                   .sin_port = 0 };
    socklen_t address_size = sizeof(address);
    server->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->fd < 0) return -1;
    if (bind(server->fd, (struct sockaddr *)&address, sizeof(address)) < 0
        || listen(server->fd, 1) < 0
        || getsockname(server->fd, (struct sockaddr *)&address,
                       &address_size) < 0)
    {
        close(server->fd);
        return -1;
    }
    server->port = ntohs(address.sin_port);
    server->status = status;
    server->body = body;
    server->second_status = "500 Internal Server Error";
    server->second_body = "{}";
    server->request_count = 1;
    server->requests[0][0] = '\0';
    server->requests[1][0] = '\0';
    return 0;
}

static void *
fixture_server_respond(void *data)
{
    struct fixture_server *server = data;
    for (unsigned i = 0; i < server->request_count; ++i) {
        struct pollfd listener = { .fd = server->fd, .events = POLLIN };
        if (poll(&listener, 1, 1000) <= 0) break;
        int connection = accept(server->fd, NULL, NULL);
        if (connection < 0) break;

        char *request = server->requests[i];
        size_t used = 0;
        while (used + 1 < sizeof(server->requests[i])) {
            ssize_t count = recv(connection, request + used,
                                 sizeof(server->requests[i]) - used - 1, 0);
            if (count <= 0) break;
            used += (size_t)count;
            request[used] = '\0';
            if (strstr(request, "\r\n\r\n")) break;
        }

        const char *status = i == 0 ? server->status : server->second_status;
        const char *body = i == 0 ? server->body : server->second_body;
        char response[8192];
        int length = snprintf(response, sizeof(response),
                              "HTTP/1.1 %s\r\nContent-Type: application/json\r\n"
                              "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
                              status, strlen(body), body);
        if (length > 0 && (size_t)length < sizeof(response))
            (void)send(connection, response, (size_t)length, 0);
        close(connection);
    }
    close(server->fd);
    return NULL;
}

static size_t
load_fixture(const char *path, char **contents)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) return 0;

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
    if (buffer == NULL) {
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

TEST
valid_discovery_keeps_advertised_service_urls(void)
{
    char *json = NULL;
    size_t size = load_fixture("test/fixtures/fluxer-discovery.json", &json);
    ASSERT(size > 0);

    struct fluxer_discovery discovery = { 0 };
    ASSERT_EQ(CCORD_OK, fluxer_discovery_parse(json, size, &discovery));
    ASSERT_STR_EQ("https://api.selfhost.test/prefix/v1", discovery.api_base);
    ASSERT_STR_EQ("wss://gateway.selfhost.test/socket", discovery.gateway_url);
    ASSERT_STR_EQ("https://media.selfhost.test", discovery.media_url);

    fluxer_discovery_cleanup(&discovery);
    free(json);
    PASS();
}

TEST
invalid_discovery_is_rejected(void)
{
    struct fluxer_discovery discovery = { 0 };
    ASSERT_EQ(CCORD_BAD_DECODE,
              fluxer_discovery_parse("{", 1, &discovery));

    char *json = NULL;
    size_t size = load_fixture("test/fixtures/fluxer-discovery-invalid.json",
                               &json);
    ASSERT(size > 0);
    ASSERT_EQ(CCORD_BAD_DECODE,
              fluxer_discovery_parse(json, size, &discovery));
    free(json);

    static const char invalid_gateway[] =
        "{\"endpoints\":{" 
        "\"api_public\":\"https://api.example\","
        "\"gateway\":\"https://gateway.example\","
        "\"media\":\"https://media.example\"}}";
    ASSERT_EQ(CCORD_BAD_DECODE,
              fluxer_discovery_parse(invalid_gateway,
                                     sizeof(invalid_gateway) - 1,
                                     &discovery));
    static const char credentials_in_endpoint[] =
        "{\"endpoints\":{"
        "\"api_public\":\"https://user:secret@api.example\","
        "\"gateway\":\"wss://gateway.example\","
        "\"media\":\"https://media.example\"}}";
    ASSERT_EQ(CCORD_BAD_DECODE,
              fluxer_discovery_parse(credentials_in_endpoint,
                                     sizeof(credentials_in_endpoint) - 1,
                                     &discovery));

    PASS();
}

TEST
fetch_uses_discovery_path_without_authorization(void)
{
    char *json = NULL;
    size_t size = load_fixture("test/fixtures/fluxer-discovery.json", &json);
    ASSERT(size > 0);

    struct fixture_server server;
    ASSERT_EQ(0, fixture_server_open(&server, "200 OK", json));
    pthread_t thread;
    ASSERT_EQ(0, pthread_create(&thread, NULL, fixture_server_respond, &server));

    char origin[64];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", server.port);
    struct fluxer_discovery discovery = { 0 };
    ASSERT_EQ(CCORD_OK, fluxer_discovery_fetch(origin, &discovery));
    ASSERT_EQ(0, pthread_join(thread, NULL));
    ASSERT(strstr(server.requests[0], "GET /.well-known/fluxer ") != NULL);
    ASSERT(strstr(server.requests[0], "Authorization:") == NULL);
    ASSERT_STR_EQ("https://api.selfhost.test/prefix/v1", discovery.api_base);

    fluxer_discovery_cleanup(&discovery);
    free(json);
    PASS();
}

TEST
json_config_origin_is_used_before_rest_setup(void)
{
    char body[512] = { 0 };
    struct fixture_server server;
    ASSERT_EQ(0, fixture_server_open(&server, "200 OK", body));
    char origin[64];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", server.port);
    snprintf(body, sizeof(body),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);
    server.request_count = 2;
    server.second_status = "401 Unauthorized";
    server.second_body = "{\"message\":\"Unauthorized\",\"code\":0}";

    char config_json[256];
    int config_length = snprintf(
        config_json, sizeof(config_json),
        "{\"token\":\"123456789012345678.abcdefghijklmnopqrstuvwxyz0123456789ABCD\","
        "\"instance_origin\":\"%s\",\"log\":{\"quiet\":true}}",
        origin);
    ASSERT(config_length > 0 && (size_t)config_length < sizeof(config_json));
    char config_path[] = "/tmp/concord-fluxer-config-XXXXXX";
    int config_fd = mkstemp(config_path);
    ASSERT(config_fd >= 0);
    ASSERT_EQ(config_length,
              (int)write(config_fd, config_json, (size_t)config_length));
    close(config_fd);

    pthread_t thread;
    ASSERT_EQ(0, pthread_create(&thread, NULL, fixture_server_respond, &server));
    struct discord *client = discord_from_json(config_path);
    ASSERT(client == NULL);
    ASSERT_EQ(0, pthread_join(thread, NULL));
    ASSERT(strstr(server.requests[0], "GET /.well-known/fluxer ") != NULL);
    ASSERT(strstr(server.requests[1], "GET /api-root/v1/users/@me ") != NULL);
    ASSERT(strstr(server.requests[1],
                  "Authorization: Bot 123456789012345678.abcdefghijklmnopqrstuvwxyz0123456789ABCD")
           != NULL);
    ASSERT(strstr(server.requests[0], "Authorization:") == NULL);
    ASSERT(strstr(server.requests[1], "Authorization: Bot ") != NULL);
    unlink(config_path);
    PASS();
}

TEST
http_failure_is_reported(void)
{
    struct fixture_server server;
    ASSERT_EQ(0, fixture_server_open(&server, "503 Service Unavailable", "{}"));
    pthread_t thread;
    ASSERT_EQ(0, pthread_create(&thread, NULL, fixture_server_respond, &server));

    char origin[64];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", server.port);
    struct fluxer_discovery discovery = { 0 };
    ASSERT_EQ(CCORD_HTTP_CODE, fluxer_discovery_fetch(origin, &discovery));
    ASSERT_EQ(0, pthread_join(thread, NULL));
    fluxer_discovery_cleanup(&discovery);
    PASS();
}

TEST
invalid_origin_scheme_is_rejected(void)
{
    struct fluxer_discovery discovery = { 0 };
    ASSERT_EQ(CCORD_BAD_PARAMETER,
              fluxer_discovery_fetch("ftp://fluxer.example", &discovery));
    PASS();
}

GREATEST_MAIN_DEFS();

int
main(int argc, char *argv[])
{
    GREATEST_MAIN_BEGIN();

    RUN_TEST(valid_discovery_keeps_advertised_service_urls);
    RUN_TEST(invalid_discovery_is_rejected);
    RUN_TEST(fetch_uses_discovery_path_without_authorization);
    RUN_TEST(json_config_origin_is_used_before_rest_setup);
    RUN_TEST(http_failure_is_reported);
    RUN_TEST(invalid_origin_scheme_is_rejected);

    GREATEST_MAIN_END();
}
