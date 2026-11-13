#include <errno.h>
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

#define GATEWAY_BUFFER_SIZE 16384
#define HTTP_REQUESTS 3

struct gateway_server {
    int fd;
    unsigned short port;
    char discovery_json[512];
    char gateway_bot_json[512];
    const char *user_json;
    bool invalid_session;
    char http_requests[HTTP_REQUESTS][4096];
    char websocket_request[4096];
    char resume_websocket_request[4096];
    char identify[2048];
    char resume[2048];
    char resume_heartbeat[256];
    char pre_ready_heartbeat[256];
    char ready_heartbeat[256];
    int stage;
    int failed;
};

static volatile int message_received;
static volatile int ready_received;

static size_t
load_fixture(const char *path, char **contents)
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

static bool
read_exact(int fd, void *buffer, size_t length)
{
    size_t used = 0;
    while (used < length) {
        ssize_t count = recv(fd, (char *)buffer + used, length - used, 0);
        if (count <= 0) return false;
        used += (size_t)count;
    }
    return true;
}

static bool
write_exact(int fd, const void *buffer, size_t length)
{
    size_t used = 0;
    while (used < length) {
        ssize_t count = send(fd, (const char *)buffer + used, length - used, 0);
        if (count <= 0) return false;
        used += (size_t)count;
    }
    return true;
}

static bool
read_http_headers(int fd, char buffer[], size_t capacity)
{
    size_t used = 0;
    while (used + 1 < capacity) {
        ssize_t count = recv(fd, buffer + used, capacity - used - 1, 0);
        if (count <= 0) return false;
        used += (size_t)count;
        buffer[used] = '\0';
        if (strstr(buffer, "\r\n\r\n")) return true;
    }
    return false;
}

static bool
send_http(int fd, const char *status, const char *body)
{
    char response[8192];
    int length = snprintf(response, sizeof(response),
                          "HTTP/1.1 %s\r\nContent-Type: application/json\r\n"
                          "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
                          status, strlen(body), body);
    return length > 0 && (size_t)length < sizeof(response)
           && write_exact(fd, response, (size_t)length);
}

static bool
read_client_frame(int fd, char output[], size_t output_size, unsigned *opcode)
{
    unsigned char header[2];
    if (!read_exact(fd, header, sizeof(header))) return false;
    *opcode = header[0] & 0x0f;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t length = header[1] & 0x7f;
    if (length == 126) {
        unsigned char extended[2];
        if (!read_exact(fd, extended, sizeof(extended))) return false;
        length = ((uint64_t)extended[0] << 8) | extended[1];
    }
    else if (length == 127) {
        unsigned char extended[8];
        if (!read_exact(fd, extended, sizeof(extended))) return false;
        length = 0;
        for (size_t i = 0; i < sizeof(extended); ++i)
            length = (length << 8) | extended[i];
    }
    if (length + 1 > output_size) return false;

    unsigned char mask[4] = { 0 };
    if (masked && !read_exact(fd, mask, sizeof(mask))) return false;
    if (!read_exact(fd, output, (size_t)length)) return false;
    if (masked) {
        for (size_t i = 0; i < (size_t)length; ++i)
            output[i] ^= mask[i % sizeof(mask)];
    }
    output[length] = '\0';
    return true;
}

static bool
send_server_frame(int fd, unsigned opcode, const char *text)
{
    size_t length = strlen(text);
    unsigned char header[10];
    size_t header_size;
    header[0] = (unsigned char)(0x80 | (opcode & 0x0f));
    if (length < 126) {
        header[1] = (unsigned char)length;
        header_size = 2;
    }
    else if (length <= 65535) {
        header[1] = 126;
        header[2] = (unsigned char)(length >> 8);
        header[3] = (unsigned char)length;
        header_size = 4;
    }
    else {
        header[1] = 127;
        for (size_t i = 0; i < 8; ++i)
            header[2 + i] = (unsigned char)(length >> (56 - i * 8));
        header_size = 10;
    }
    return write_exact(fd, header, header_size)
           && write_exact(fd, text, length);
}

static bool
complete_websocket_upgrade(int fd, char request[], size_t request_size)
{
    if (!read_http_headers(fd, request, request_size)) return false;

    char key[128] = { 0 };
    char *header = request;
    while ((header = strstr(header, "Sec-WebSocket-Key:")) != NULL) {
        if (sscanf(header, "Sec-WebSocket-Key: %127s", key) == 1) break;
        ++header;
    }
    if (!*key) {
        return false;
    }

    char input[256];
    int input_size = snprintf(input, sizeof(input),
                              "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_size = 0;
    if (input_size < 0
        || EVP_Digest(input, (size_t)input_size, digest, &digest_size,
                      EVP_sha1(), NULL) != 1)
    {
        return false;
    }
    unsigned char accept[64];
    int accept_size = EVP_EncodeBlock(accept, digest, (int)digest_size);
    if (accept_size <= 0) return false;

    char response[512];
    int response_size = snprintf(
        response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
        "Connection: Upgrade\r\nSec-WebSocket-Accept: %.*s\r\n\r\n",
        accept_size, accept);
    if (response_size <= 0 || (size_t)response_size >= sizeof(response)) {
        return false;
    }
    bool sent = write_exact(fd, response, (size_t)response_size);
    return sent;
}

static bool
read_http_request(struct gateway_server *server, size_t index)
{
    int fd = accept(server->fd, NULL, NULL);
    if (fd < 0) return false;
    struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    bool result = read_http_headers(fd, server->http_requests[index],
                                    sizeof(server->http_requests[index]));
    const char *body = index == 0
                           ? server->discovery_json
                           : index == 1 ? server->user_json
                                        : server->gateway_bot_json;
    if (result) result = send_http(fd, "200 OK", body);
    close(fd);
    return result;
}

static bool
serve_gateway(struct gateway_server *server)
{
    int fd = accept(server->fd, NULL, NULL);
    if (fd < 0) return false;
    server->stage = 10;
    struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (!complete_websocket_upgrade(fd, server->websocket_request,
                                    sizeof(server->websocket_request))) {
        close(fd);
        return false;
    }
    server->stage = 11;

    char *hello = NULL, *ready = NULL, *message = NULL;
    size_t hello_size = load_fixture("test/fixtures/fluxer-gateway-hello.json",
                                     &hello);
    size_t ready_size = load_fixture("test/fixtures/fluxer-gateway-ready.json",
                                     &ready);
    size_t message_size = load_fixture("test/fixtures/fluxer-message.json",
                                       &message);
    if (!hello_size || !ready_size || !message_size)
    {
        free(hello);
        free(ready);
        free(message);
        close(fd);
        return false;
    }
    bool result = send_server_frame(fd, 1, hello);
    if (result) server->stage = 1;
    unsigned opcode = 0;
    if (result)
        result = read_client_frame(fd, server->identify,
                                  sizeof(server->identify), &opcode)
                 && opcode == 1;
    if (result) server->stage = 2;

    if (result) result = send_server_frame(fd, 1, "{\"op\":1,\"d\":null}");
    if (result)
        result = read_client_frame(fd, server->pre_ready_heartbeat,
                                   sizeof(server->pre_ready_heartbeat),
                                   &opcode)
                 && opcode == 1;
    if (result) server->stage = 7;
    if (result) result = send_server_frame(fd, 1, "{\"op\":11}");

    if (result) result = send_server_frame(fd, 1, ready);
    if (result) server->stage = 3;
    if (result)
        result = read_client_frame(fd, server->ready_heartbeat,
                                   sizeof(server->ready_heartbeat), &opcode)
                 && opcode == 1;
    if (result) server->stage = 4;
    if (result) result = send_server_frame(fd, 1, "{\"op\":11}");

    char event[8192];
    int event_size = snprintf(event, sizeof(event),
                              "{\"op\":0,\"s\":2,\"t\":\"MESSAGE_UPDATE\",\"d\":%s}",
                              message);
    if (result && event_size > 0 && (size_t)event_size < sizeof(event))
        result = send_server_frame(fd, 1, event);
    else
        result = false;
    if (result) server->stage = 5;

    if (result) {
        result = send_server_frame(
            fd, 1, server->invalid_session ? "{\"op\":9,\"d\":false}"
                                           : "{\"op\":7}");
        if (result) server->stage = 8;
    }
    if (result) {
        char close_frame[128];
        result = read_client_frame(fd, close_frame, sizeof(close_frame),
                                   &opcode)
                 && opcode == 8;
        if (result) server->stage = 9;
    }
    close(fd);

    fd = accept(server->fd, NULL, NULL);
    if (fd >= 0) setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                           sizeof(timeout));
    char retry_http[4096] = { 0 };
    if (result)
        result = fd >= 0
                 && read_http_headers(fd, retry_http, sizeof(retry_http))
                 && strstr(retry_http, "GET /api-root/v1/gateway/bot ")
                        != NULL
                 && send_http(fd, "200 OK", server->gateway_bot_json);
    if (fd >= 0) close(fd);

    fd = accept(server->fd, NULL, NULL);
    if (fd >= 0) server->stage = 10;
    if (fd >= 0) setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                            sizeof(timeout));
    if (result)
        result = fd >= 0
                 && complete_websocket_upgrade(
                     fd, server->resume_websocket_request,
                     sizeof(server->resume_websocket_request));
    if (result) server->stage = 11;
    if (result) result = send_server_frame(fd, 1, hello);
    if (result)
        result = read_client_frame(fd, server->resume, sizeof(server->resume),
                                   &opcode)
                 && opcode == 1;
    if (result && server->invalid_session) {
        result = strstr(server->resume, "\"op\":2") != NULL
                 && strstr(server->resume, "\"intents\"") == NULL;
        if (result) server->stage = 12;
        if (result) result = send_server_frame(fd, 1, ready);
    }
    else if (result) {
        result = strstr(server->resume, "\"op\":6") != NULL
                 && strstr(server->resume,
                           "\"session_id\":\"fluxer-session-1\"") != NULL
                 && strstr(server->resume, "\"seq\":2") != NULL;
        if (result) server->stage = 12;
        if (result)
            result = send_server_frame(
                fd, 1, "{\"op\":0,\"s\":3,\"t\":\"RESUMED\",\"d\":{}}");
    }
    if (result) server->stage = 13;
    if (result)
        result = read_client_frame(fd, server->resume_heartbeat,
                                   sizeof(server->resume_heartbeat), &opcode)
                 && opcode == 1
                 && strcmp(server->resume_heartbeat,
                           server->invalid_session ? "{\"op\":1,\"d\":1}"
                                                   : "{\"op\":1,\"d\":3}")
                        == 0;
    if (result) result = send_server_frame(fd, 1, "{\"op\":11}");
    event_size = snprintf(event, sizeof(event),
                          "{\"op\":0,\"s\":4,\"t\":\"MESSAGE_CREATE\",\"d\":%s}",
                          message);
    if (result && event_size > 0 && (size_t)event_size < sizeof(event))
        result = send_server_frame(fd, 1, event);
    else
        result = false;
    if (result) server->stage = 14;
    if (result) {
        char close_frame[128];
        result = read_client_frame(fd, close_frame, sizeof(close_frame),
                                   &opcode)
                 && opcode == 8;
    }
    if (result) server->stage = 6;
    if (result) {
        const unsigned char close_response[] = { 0x88, 0x02, 0x03, 0xe8 };
        result = write_exact(fd, close_response, sizeof(close_response));
    }
    if (fd >= 0) close(fd);
    free(hello);
    free(ready);
    free(message);
    return result;
}

static void *
gateway_server_thread(void *data)
{
    struct gateway_server *server = data;
    bool result = true;
    for (size_t i = 0; i < HTTP_REQUESTS && result; ++i) {
        result = read_http_request(server, i);
        if (result) server->stage = (int)i + 1;
    }
    if (result) result = serve_gateway(server);
    server->failed = !result;
    close(server->fd);
    return NULL;
}

static int
gateway_server_open(struct gateway_server *server)
{
    struct sockaddr_in address = { .sin_family = AF_INET,
                                   .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
                                   .sin_port = 0 };
    socklen_t size = sizeof(address);
    server->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->fd < 0) return -1;
    if (bind(server->fd, (struct sockaddr *)&address, sizeof(address)) < 0
        || listen(server->fd, 8) < 0
        || getsockname(server->fd, (struct sockaddr *)&address, &size) < 0)
    {
        close(server->fd);
        return -1;
    }
    struct timeval accept_timeout = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(server->fd, SOL_SOCKET, SO_RCVTIMEO, &accept_timeout,
               sizeof(accept_timeout));
    server->port = ntohs(address.sin_port);
    memset(server->http_requests, 0, sizeof(server->http_requests));
    memset(server->websocket_request, 0, sizeof(server->websocket_request));
    memset(server->resume_websocket_request, 0,
           sizeof(server->resume_websocket_request));
    memset(server->identify, 0, sizeof(server->identify));
    memset(server->resume, 0, sizeof(server->resume));
    memset(server->resume_heartbeat, 0, sizeof(server->resume_heartbeat));
    memset(server->pre_ready_heartbeat, 0,
           sizeof(server->pre_ready_heartbeat));
    memset(server->ready_heartbeat, 0, sizeof(server->ready_heartbeat));
    server->stage = 0;
    server->failed = 0;
    return 0;
}

static bool
write_config(const char origin[], char path[])
{
    static const char token[] =
        "123456789012345678.abcdefghijklmnopqrstuvwxyz0123456789ABCD";
    char json[512];
    int size = snprintf(json, sizeof(json),
                        "{\"token\":\"%s\",\"instance_origin\":\"%s\","
                        "\"log\":{\"quiet\":true,\"level\":\"FATAL\"}}",
                        token, origin);
    if (size < 0 || (size_t)size >= sizeof(json)) return false;
    int fd = mkstemp(path);
    if (fd < 0) return false;
    bool result = write(fd, json, (size_t)size) == size;
    close(fd);
    return result;
}

static void
on_ready(struct discord *client, const struct discord_ready *event)
{
    (void)client;
    (void)event;
    ready_received = 1;
}

static void
on_message(struct discord *client, const struct discord_message *event)
{
    if (event->content) {
        message_received = 1;
        discord_shutdown(client);
    }
}

static time_t idle_deadline;
static void
stop_if_gateway_stalls(struct discord *client)
{
    if (time(NULL) >= idle_deadline) discord_shutdown(client);
}

static enum greatest_test_res
run_gateway_scenario(bool invalid_session)
{
    char *user_json = NULL;
    ASSERT(load_fixture("test/fixtures/fluxer-user.json", &user_json) > 0);
    struct gateway_server server = { 0 };
    ASSERT_EQ(0, gateway_server_open(&server));

    char origin[64];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", server.port);
    int discovery_size = snprintf(
        server.discovery_json, sizeof(server.discovery_json),
        "{\"endpoints\":{\"api_public\":\"%s/api-root\","
        "\"gateway\":\"ws://127.0.0.1:%u/gateway\","
        "\"media\":\"http://media.selfhost.test\"}}",
        origin, server.port);
    int gateway_bot_size = snprintf(
        server.gateway_bot_json, sizeof(server.gateway_bot_json),
        "{\"url\":\"ws://127.0.0.1:%u/gateway\",\"shards\":1,"
        "\"session_start_limit\":{\"total\":1000,\"remaining\":999,"
        "\"reset_after\":14400000,\"max_concurrency\":1}}",
        server.port);
    ASSERT(discovery_size > 0
           && (size_t)discovery_size < sizeof(server.discovery_json));
    ASSERT(gateway_bot_size > 0
           && (size_t)gateway_bot_size < sizeof(server.gateway_bot_json));
    server.user_json = user_json;
    server.invalid_session = invalid_session;

    char config_path[] = "/tmp/concord-fluxer-gateway-XXXXXX";
    ASSERT(write_config(origin, config_path));
    pthread_t thread;
    ASSERT_EQ(0, pthread_create(&thread, NULL, gateway_server_thread, &server));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);
    discord_set_on_ready(client, &on_ready);
    discord_set_on_message_create(client, &on_message);
    discord_set_on_idle(client, &stop_if_gateway_stalls);
    client->gw.session->retry.limit = 2;
    message_received = 0;
    ready_received = 0;
    idle_deadline = time(NULL) + 5;
    CCORDcode code = discord_run(client);
    ASSERT_EQ(0, pthread_join(thread, NULL));
    ASSERT_EQ(6, server.stage);
    ASSERT_EQ(0, server.failed);
    ASSERT(code == CCORD_OK || code == CCORD_DISCORD_CONNECTION);
    ASSERT_EQ(1, message_received);
    ASSERT_EQ(1, ready_received);

    ASSERT(strstr(server.websocket_request,
                  "GET /gateway?v=1&encoding=json ") != NULL);
    ASSERT(strstr(server.resume_websocket_request,
                  "GET /gateway?v=1&encoding=json ") != NULL);
    ASSERT(strstr(server.identify, "\"op\":2") != NULL);
    ASSERT(strstr(server.identify,
                  "\"token\":\"123456789012345678.") != NULL);
    ASSERT(strstr(server.identify, "\"properties\"") != NULL);
    ASSERT(strstr(server.identify, "\"intents\"") == NULL);
    if (invalid_session) {
        ASSERT(strstr(server.resume, "\"op\":2") != NULL);
        ASSERT(strstr(server.resume, "\"intents\"") == NULL);
        ASSERT(strcmp(server.resume_heartbeat, "{\"op\":1,\"d\":1}")
               == 0);
    }
    else {
        ASSERT(strstr(server.resume, "\"op\":6") != NULL);
        ASSERT(strstr(server.resume, "\"seq\":2") != NULL);
        ASSERT(strcmp(server.resume_heartbeat, "{\"op\":1,\"d\":3}")
               == 0);
    }
    ASSERT(strcmp(server.pre_ready_heartbeat, "{\"op\":1,\"d\":null}")
           == 0);
    ASSERT(strcmp(server.ready_heartbeat, "{\"op\":1,\"d\":1}") == 0);
    ASSERT(strstr(server.http_requests[2], "GET /api-root/v1/gateway/bot ")
           != NULL);

    discord_cleanup(client);
    unlink(config_path);
    free(user_json);
    PASS();
}

TEST
fluxer_gateway_reconnect_resumes_last_dispatch_sequence(void)
{
    return run_gateway_scenario(false);
}

TEST
fluxer_gateway_invalid_session_false_reidentifies(void)
{
    return run_gateway_scenario(true);
}

GREATEST_MAIN_DEFS();

int
main(int argc, char *argv[])
{
    GREATEST_MAIN_BEGIN();
    RUN_TEST(fluxer_gateway_reconnect_resumes_last_dispatch_sequence);
    RUN_TEST(fluxer_gateway_invalid_session_false_reidentifies);
    GREATEST_MAIN_END();
}
