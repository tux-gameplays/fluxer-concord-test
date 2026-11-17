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

#define MOCK_MAX_REQUESTS 32
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
    struct unsupported_context *context = response->data;
    (void)client;
    ++context->calls;
    context->code = response->code;
    context->data = response->data;
    context->keep = response->keep;
}

struct http_mock {
    int fd;
    unsigned short port;
    size_t expected_requests;
    size_t actual_requests;
    const char *statuses[MOCK_MAX_REQUESTS];
    const char *bodies[MOCK_MAX_REQUESTS];
    char requests[MOCK_MAX_REQUESTS][MOCK_BUFFER_SIZE];
};

static int
mock_open(struct http_mock *mock, size_t expected_requests)
{
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port = 0,
    };
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
        int connection = accept(mock->fd, NULL, NULL);
        if (connection < 0) break;

        ++mock->actual_requests;
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
        const char *status =
            mock->statuses[i] ? mock->statuses[i] : "200 OK";
        char response[MOCK_BUFFER_SIZE];
        int response_size =
            snprintf(response, sizeof(response),
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

#define ASSERT_UNSUPPORTED_CALL(expr, ctx)                                    \
    do {                                                                      \
        ASSERT_EQ(CCORD_UNSUPPORTED, (expr));                                 \
        ASSERT_EQ(1, (ctx).calls);                                            \
        ASSERT_EQ(CCORD_UNSUPPORTED, (ctx).code);                             \
        ASSERT_EQ(&(ctx), (ctx).data);                                        \
        ASSERT_EQ(&(ctx), (ctx).keep);                                        \
    } while (0)

TEST
unsupported_guild_operations_fail_without_http_requests(void)
{
    static const char user_json[] =
        "{\"id\":\"9223372036854775810\",\"username\":\"guildbot\","
        "\"discriminator\":\"0001\",\"bot\":true}";
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
    mock.bodies[1] = user_json;

    char config_path[] = "/tmp/concord-fluxer-guild-unsup-XXXXXX";
    ASSERT(write_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    const u64snowflake gid = UINT64_C(9223372036854775820);
    const u64snowflake uid = UINT64_C(9223372036854775821);
    const u64snowflake iid = UINT64_C(9223372036854775822);

    struct unsupported_context ctx;

    ctx = (struct unsupported_context){ 0 };
    struct discord_ret_guild ret_guild = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(discord_create_guild(client, NULL, &ret_guild),
                            ctx);
    ASSERT_EQ(CCORD_UNSUPPORTED, discord_create_guild(client, NULL, NULL));

    ctx = (struct unsupported_context){ 0 };
    struct discord_ret_guild_preview ret_preview = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(discord_get_guild_preview(client, gid, &ret_preview),
                            ctx);

    ctx = (struct unsupported_context){ 0 };
    struct discord_ret ret_blank = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(discord_delete_guild(client, gid, &ret_blank),
                            ctx);

    ctx = (struct unsupported_context){ 0 };
    struct discord_ret_guild_member ret_member = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_add_guild_member(client, gid, uid, NULL, &ret_member), ctx);

    ctx = (struct unsupported_context){ 0 };
    struct discord_ret_ban ret_ban = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(discord_get_guild_ban(client, gid, uid, &ret_ban),
                            ctx);

    ctx = (struct unsupported_context){ 0 };
    struct discord_ret_prune_count ret_prune = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_get_guild_prune_count(client, gid, NULL, &ret_prune), ctx);

    ctx = (struct unsupported_context){ 0 };
    ret_blank = (struct discord_ret){
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_begin_guild_prune(client, gid, NULL, &ret_blank), ctx);

    ctx = (struct unsupported_context){ 0 };
    struct discord_ret_voice_regions ret_regions = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_get_guild_voice_regions(client, gid, &ret_regions), ctx);

    ctx = (struct unsupported_context){ 0 };
    struct discord_ret_integrations ret_integrations = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_get_guild_integrations(client, gid, &ret_integrations), ctx);

    ctx = (struct unsupported_context){ 0 };
    ret_blank = (struct discord_ret){
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_delete_guild_integrations(client, gid, iid, NULL, &ret_blank),
        ctx);

    ctx = (struct unsupported_context){ 0 };
    struct discord_ret_guild_widget_settings ret_widget_settings = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_get_guild_widget_settings(client, gid, &ret_widget_settings),
        ctx);

    ctx = (struct unsupported_context){ 0 };
    ret_widget_settings = (struct discord_ret_guild_widget_settings){
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_modify_guild_widget(client, gid, NULL, &ret_widget_settings),
        ctx);

    ctx = (struct unsupported_context){ 0 };
    struct discord_ret_guild_widget ret_widget = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(discord_get_guild_widget(client, gid, &ret_widget),
                            ctx);

    ctx = (struct unsupported_context){ 0 };
    struct discord_ret_welcome_screen ret_welcome = {
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_get_guild_welcome_screen(client, gid, &ret_welcome), ctx);

    ctx = (struct unsupported_context){ 0 };
    ret_welcome = (struct discord_ret_welcome_screen){
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_modify_guild_welcome_screen(client, gid, NULL, &ret_welcome),
        ctx);

    ctx = (struct unsupported_context){ 0 };
    ret_blank = (struct discord_ret){
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_modify_current_user_voice_state(client, gid, NULL, &ret_blank),
        ctx);

    ctx = (struct unsupported_context){ 0 };
    ret_blank = (struct discord_ret){
        .data = &ctx, .keep = &ctx, .fail = &on_unsupported
    };
    ASSERT_UNSUPPORTED_CALL(
        discord_modify_user_voice_state(client, gid, uid, NULL, &ret_blank),
        ctx);

    ASSERT_EQ(0, pthread_join(server_thread, NULL));
    ASSERT_EQ(2, mock.actual_requests);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

TEST
guild_and_channel_endpoints_use_fluxer_routes_and_large_snowflakes(void)
{
    static const char user_json[] =
        "{\"id\":\"9223372036854775810\",\"username\":\"guildbot\","
        "\"discriminator\":\"0001\",\"bot\":true}";
    static const char guild_json[] =
        "{\"id\":\"9223372036854775820\",\"name\":\"Tidepool\","
        "\"owner_id\":\"9223372036854775821\",\"afk_timeout\":300,"
        "\"system_channel_flags\":3,\"member_count\":4182,"
        "\"vanity_url_code\":\"tidepool\"}";
    static const char modified_guild_json[] =
        "{\"id\":\"9223372036854775820\",\"name\":\"Updated Tidepool\","
        "\"owner_id\":\"9223372036854775821\",\"afk_timeout\":600,"
        "\"system_channel_flags\":3,\"member_count\":4182}";
    static const char channels_json[] =
        "[{\"id\":\"9223372036854775822\",\"guild_id\":\"9223372036854775820\","
        "\"name\":\"general\",\"type\":0}]";
    static const char created_channel_json[] =
        "{\"id\":\"9223372036854775824\",\"guild_id\":\"9223372036854775820\","
        "\"parent_id\":\"9223372036854775823\",\"name\":\"ops\",\"type\":0}";
    static const char invites_json[] =
        "[{\"code\":\"aB3dEf7h\",\"type\":0,"
        "\"guild\":{\"id\":\"9223372036854775820\",\"name\":\"Tidepool\"},"
        "\"channel\":{\"id\":\"9223372036854775822\",\"name\":\"general\",\"type\":0},"
        "\"inviter\":{\"id\":\"9223372036854775821\",\"username\":\"owner\"},"
        "\"member_count\":412,\"presence_count\":87}]";
    static const char vanity_json[] =
        "{\"code\":\"tidepool\",\"uses\":1337}";

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
    mock.bodies[1] = user_json;
    mock.bodies[2] = guild_json;
    mock.bodies[3] = modified_guild_json;
    mock.bodies[4] = channels_json;
    mock.statuses[5] = "201 Created";
    mock.bodies[5] = created_channel_json;
    mock.statuses[6] = "204 No Content";
    mock.bodies[6] = "";
    mock.bodies[7] = invites_json;
    mock.bodies[8] = vanity_json;

    char config_path[] = "/tmp/concord-fluxer-guild-core-XXXXXX";
    ASSERT(write_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    const u64snowflake guild_id = UINT64_C(9223372036854775820);
    const u64snowflake owner_id = UINT64_C(9223372036854775821);
    const u64snowflake channel_id = UINT64_C(9223372036854775822);
    const u64snowflake parent_id = UINT64_C(9223372036854775823);
    const u64snowflake created_id = UINT64_C(9223372036854775824);

    struct discord_guild guild = { 0 };
    struct discord_ret_guild guild_ret = { .sync = &guild };
    ASSERT_EQ(CCORD_OK, discord_get_guild(client, guild_id, &guild_ret));
    ASSERT_EQ(guild_id, guild.id);
    ASSERT_EQ(owner_id, guild.owner_id);
    ASSERT_STR_EQ("Tidepool", guild.name);
    ASSERT_EQ(UINT64_C(3), guild.system_channel_flags);
    ASSERT_EQ(4182, guild.member_count);
    discord_guild_cleanup(&guild);

    struct discord_modify_guild mod_params = {
        .reason = "rename guild",
        .name = "Updated Tidepool",
        .afk_timeout = 600,
        .system_channel_flags = 3,
    };
    guild = (struct discord_guild){ 0 };
    guild_ret = (struct discord_ret_guild){ .sync = &guild };
    ASSERT_EQ(CCORD_OK,
              discord_modify_guild(client, guild_id, &mod_params, &guild_ret));
    ASSERT_EQ(guild_id, guild.id);
    ASSERT_STR_EQ("Updated Tidepool", guild.name);
    ASSERT_EQ(600, guild.afk_timeout);
    discord_guild_cleanup(&guild);

    struct discord_channels channels = { 0 };
    struct discord_ret_channels channels_ret = { .sync = &channels };
    ASSERT_EQ(CCORD_OK,
              discord_get_guild_channels(client, guild_id, &channels_ret));
    ASSERT_EQ(1, channels.size);
    ASSERT_EQ(channel_id, channels.array[0].id);
    ASSERT_STR_EQ("general", channels.array[0].name);
    discord_channels_cleanup(&channels);

    struct discord_create_guild_channel create_chan = {
        .reason = "ops channel",
        .name = "ops",
        .type = DISCORD_CHANNEL_GUILD_TEXT,
        .parent_id = parent_id,
    };
    struct discord_channel channel = { 0 };
    struct discord_ret_channel channel_ret = { .sync = &channel };
    ASSERT_EQ(CCORD_OK, discord_create_guild_channel(
                            client, guild_id, &create_chan, &channel_ret));
    ASSERT_EQ(created_id, channel.id);
    ASSERT_EQ(parent_id, channel.parent_id);
    ASSERT_STR_EQ("ops", channel.name);
    discord_channel_cleanup(&channel);

    struct discord_modify_guild_channel_position pos_entry = {
        .id = created_id,
        .position = 2,
        .lock_category = true,
        .parent_id = parent_id,
    };
    struct discord_modify_guild_channel_positions pos_params = {
        .size = 1,
        .array = &pos_entry,
    };
    struct discord_ret blank_ret = { .sync = true };
    ASSERT_EQ(CCORD_OK, discord_modify_guild_channel_positions(
                            client, guild_id, &pos_params, &blank_ret));

    struct discord_invites invites = { 0 };
    struct discord_ret_invites invites_ret = { .sync = &invites };
    ASSERT_EQ(CCORD_OK,
              discord_get_guild_invites(client, guild_id, &invites_ret));
    ASSERT_EQ(1, invites.size);
    ASSERT_STR_EQ("aB3dEf7h", invites.array[0].code);
    ASSERT_EQ(guild_id, invites.array[0].guild->id);
    ASSERT_EQ(channel_id, invites.array[0].channel->id);
    ASSERT_EQ(owner_id, invites.array[0].inviter->id);
    ASSERT_EQ(412, invites.array[0].approximate_member_count);
    ASSERT_EQ(87, invites.array[0].approximate_presence_count);
    discord_invites_cleanup(&invites);

    struct discord_invite vanity = { 0 };
    struct discord_ret_invite vanity_ret = { .sync = &vanity };
    ASSERT_EQ(CCORD_OK,
              discord_get_guild_vanity_url(client, guild_id, &vanity_ret));
    ASSERT_STR_EQ("tidepool", vanity.code);
    discord_invite_cleanup(&vanity);

    ASSERT_EQ(0, pthread_join(server_thread, NULL));

    ASSERT(strstr(mock.requests[2],
                  "GET /api-root/v1/guilds/9223372036854775820 ")
           != NULL);
    ASSERT(strstr(mock.requests[3],
                  "PATCH /api-root/v1/guilds/9223372036854775820 ")
           != NULL);
    ASSERT(strstr(mock.requests[3], "X-Audit-Log-Reason: rename guild\r\n")
           != NULL);
    ASSERT(strstr(mock.requests[3], "\"name\":\"Updated Tidepool\"") != NULL);
    ASSERT(strstr(mock.requests[3], "\"afk_timeout\":600") != NULL);
    ASSERT(strstr(mock.requests[3], "\"system_channel_flags\":3") != NULL);
    ASSERT(strstr(mock.requests[3], "\"icon\":\"\"") == NULL);
    ASSERT(strstr(mock.requests[3], "\"afk_channel_id\":\"0\"") == NULL);

    ASSERT(strstr(mock.requests[4],
                  "GET /api-root/v1/guilds/9223372036854775820/channels ")
           != NULL);
    ASSERT(strstr(mock.requests[5],
                  "POST /api-root/v1/guilds/9223372036854775820/channels ")
           != NULL);
    ASSERT(strstr(mock.requests[5], "\"name\":\"ops\"") != NULL);
    ASSERT(strstr(mock.requests[5], "\"parent_id\":\"9223372036854775823\"")
           != NULL);
    ASSERT(strstr(mock.requests[5], "\"topic\":\"\"") == NULL);
    ASSERT(strstr(mock.requests[5], "\"position\":0") == NULL);

    ASSERT(strstr(mock.requests[6],
                  "PATCH /api-root/v1/guilds/9223372036854775820/channels ")
           != NULL);
    ASSERT(strstr(mock.requests[6], "\"id\":\"9223372036854775824\"") != NULL);
    ASSERT(strstr(mock.requests[6], "\"position\":2") != NULL);
    ASSERT(strstr(mock.requests[6], "\"lock_permissions\":true") != NULL);
    ASSERT(strstr(mock.requests[6], "\"lock_category\"") == NULL);

    ASSERT(strstr(mock.requests[7],
                  "GET /api-root/v1/guilds/9223372036854775820/invites ")
           != NULL);
    ASSERT(strstr(mock.requests[8],
                  "GET /api-root/v1/guilds/9223372036854775820/vanity-url ")
           != NULL);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

TEST
member_role_and_ban_endpoints_use_fluxer_routes_and_codecs(void)
{
    static const char user_json[] =
        "{\"id\":\"9223372036854775810\",\"username\":\"guildbot\","
        "\"discriminator\":\"0001\",\"bot\":true}";
    static const char member_json[] =
        "{\"user\":{\"id\":\"9223372036854775831\",\"username\":\"ada\","
        "\"discriminator\":\"0042\"},\"nick\":\"CoolAda\","
        "\"roles\":[\"9223372036854775832\"],"
        "\"joined_at\":\"2026-05-05T20:30:05.018Z\","
        "\"mute\":true,\"deaf\":true}";
    static const char members_list_json[] =
        "[{\"user\":{\"id\":\"9223372036854775831\",\"username\":\"ada\"},"
        "\"nick\":\"CoolAda\",\"roles\":[\"9223372036854775832\"],"
        "\"joined_at\":\"2026-05-05T20:30:05.018Z\",\"mute\":false,\"deaf\":false}]";
    static const char search_response_json[] =
        "{\"guild_id\":\"9223372036854775830\","
        "\"members\":[{\"id\":\"9223372036854775830_9223372036854775831\","
        "\"guild_id\":\"9223372036854775830\","
        "\"user_id\":\"9223372036854775831\","
        "\"username\":\"ada\",\"discriminator\":\"0042\","
        "\"global_name\":\"Ada\",\"nickname\":\"CoolAda\","
        "\"role_ids\":[\"9223372036854775832\"],"
        "\"joined_at\":1700000000,\"is_bot\":false}],"
        "\"page_result_count\":1,\"total_result_count\":1,\"indexing\":false}";
    static const char bans_json[] =
        "[{\"user\":{\"id\":\"9223372036854775831\",\"username\":\"spammer\","
        "\"discriminator\":\"0042\"},\"reason\":\"Advertising\","
        "\"moderator_id\":\"9223372036854775810\","
        "\"banned_at\":\"2026-08-14T09:12:44.000Z\",\"expires_at\":null}]";
    static const char role_json[] =
        "{\"id\":\"9223372036854775832\",\"name\":\"Moderator\","
        "\"color\":5793266,\"hoist\":true,\"position\":2,"
        "\"permissions\":\"274877906944\",\"managed\":false,"
        "\"mentionable\":false,\"hoist_position\":1}";
    static const char roles_json[] =
        "[{\"id\":\"9223372036854775832\",\"name\":\"Moderator\","
        "\"color\":5793266,\"hoist\":true,\"position\":2,"
        "\"permissions\":\"274877906944\",\"managed\":false,"
        "\"mentionable\":false}]";

    struct http_mock mock;
    ASSERT_EQ(0, mock_open(&mock, 18));

    char origin[64], discovery_json[512];
    snprintf(origin, sizeof(origin), "http://127.0.0.1:%u", mock.port);
    snprintf(discovery_json, sizeof(discovery_json),
             "{\"endpoints\":{\"api_public\":\"%s/api-root\","
             "\"gateway\":\"ws://gateway.selfhost.test\","
             "\"media\":\"http://media.selfhost.test\"}}",
             origin);
    mock.bodies[0] = discovery_json;
    mock.bodies[1] = user_json;
    mock.bodies[2] = member_json;
    mock.bodies[3] = members_list_json;
    mock.bodies[4] = search_response_json;
    mock.bodies[5] = member_json;
    mock.bodies[6] = member_json;
    mock.statuses[7] = "204 No Content";
    mock.statuses[8] = "204 No Content";
    mock.statuses[9] = "204 No Content";
    mock.bodies[10] = bans_json;
    mock.statuses[11] = "204 No Content";
    mock.statuses[12] = "204 No Content";
    mock.bodies[13] = roles_json;
    mock.bodies[14] = role_json;
    mock.bodies[15] = roles_json;
    mock.bodies[16] = role_json;
    mock.statuses[17] = "204 No Content";

    char config_path[] = "/tmp/concord-fluxer-guild-members-XXXXXX";
    ASSERT(write_config(origin, config_path));
    pthread_t server_thread;
    ASSERT_EQ(0, pthread_create(&server_thread, NULL, mock_serve, &mock));

    struct discord *client = discord_from_json(config_path);
    ASSERT(client != NULL);

    const u64snowflake guild_id = UINT64_C(9223372036854775830);
    const u64snowflake user_id = UINT64_C(9223372036854775831);
    const u64snowflake role_id = UINT64_C(9223372036854775832);

    struct discord_guild_member member = { 0 };
    struct discord_ret_guild_member member_ret = { .sync = &member };
    ASSERT_EQ(CCORD_OK,
              discord_get_guild_member(client, guild_id, user_id, &member_ret));
    ASSERT(member.user != NULL);
    ASSERT_EQ(user_id, member.user->id);
    ASSERT_STR_EQ("CoolAda", member.nick);
    ASSERT(member.roles != NULL);
    ASSERT_EQ(1, member.roles->size);
    ASSERT_EQ(role_id, member.roles->array[0]);
    ASSERT_EQ(true, member.muted);
    ASSERT_EQ(true, member.deaf);
    discord_guild_member_cleanup(&member);

    struct discord_list_guild_members list_params = {
        .limit = 25,
        .after = user_id,
    };
    struct discord_guild_members members = { 0 };
    struct discord_ret_guild_members members_ret = { .sync = &members };
    ASSERT_EQ(CCORD_OK, discord_list_guild_members(
                            client, guild_id, &list_params, &members_ret));
    ASSERT_EQ(1, members.size);
    ASSERT_EQ(user_id, members.array[0].user->id);
    discord_guild_members_cleanup(&members);

    struct discord_search_guild_members search_params = {
        .query = "ada",
        .limit = 10,
    };
    members = (struct discord_guild_members){ 0 };
    members_ret = (struct discord_ret_guild_members){ .sync = &members };
    ASSERT_EQ(CCORD_OK, discord_search_guild_members(
                            client, guild_id, &search_params, &members_ret));
    ASSERT_EQ(1, members.size);
    ASSERT_EQ(guild_id, members.array[0].guild_id);
    ASSERT(members.array[0].user != NULL);
    ASSERT_EQ(user_id, members.array[0].user->id);
    ASSERT_STR_EQ("ada", members.array[0].user->username);
    ASSERT_STR_EQ("0042", members.array[0].user->discriminator);
    ASSERT_STR_EQ("CoolAda", members.array[0].nick);
    ASSERT(members.array[0].roles != NULL);
    ASSERT_EQ(1, members.array[0].roles->size);
    ASSERT_EQ(role_id, members.array[0].roles->array[0]);
    ASSERT_EQ(UINT64_C(1700000000000), members.array[0].joined_at);
    discord_guild_members_cleanup(&members);

    struct discord_modify_guild_member mod_member = {
        .reason = "mute member",
        .nick = "ModAda",
        .mute = true,
    };
    member = (struct discord_guild_member){ 0 };
    member_ret = (struct discord_ret_guild_member){ .sync = &member };
    ASSERT_EQ(CCORD_OK, discord_modify_guild_member(
                            client, guild_id, user_id, &mod_member,
                            &member_ret));
    discord_guild_member_cleanup(&member);

    struct discord_modify_current_member mod_me = {
        .reason = "bot nick",
        .nick = "BotAda",
    };
    member = (struct discord_guild_member){ 0 };
    member_ret = (struct discord_ret_guild_member){ .sync = &member };
    ASSERT_EQ(CCORD_OK, discord_modify_current_member(
                            client, guild_id, &mod_me, &member_ret));
    discord_guild_member_cleanup(&member);

    struct discord_ret blank_ret = { .sync = true };
    struct discord_add_guild_member_role add_role_params = {
        .reason = "grant mod"
    };
    ASSERT_EQ(CCORD_OK, discord_add_guild_member_role(
                            client, guild_id, user_id, role_id,
                            &add_role_params, &blank_ret));

    struct discord_remove_guild_member_role rem_role_params = {
        .reason = "revoke mod"
    };
    ASSERT_EQ(CCORD_OK, discord_remove_guild_member_role(
                            client, guild_id, user_id, role_id,
                            &rem_role_params, &blank_ret));

    struct discord_remove_guild_member kick_params = { .reason = "kick" };
    ASSERT_EQ(CCORD_OK, discord_remove_guild_member(
                            client, guild_id, user_id, &kick_params,
                            &blank_ret));

    struct discord_bans bans = { 0 };
    struct discord_ret_bans bans_ret = { .sync = &bans };
    ASSERT_EQ(CCORD_OK, discord_get_guild_bans(client, guild_id, &bans_ret));
    ASSERT_EQ(1, bans.size);
    ASSERT_EQ(user_id, bans.array[0].user->id);
    ASSERT_STR_EQ("Advertising", bans.array[0].reason);
    discord_bans_cleanup(&bans);

    struct discord_create_guild_ban ban_params = {
        .reason = "Advertising",
        .delete_message_days = 2,
    };
    ASSERT_EQ(CCORD_OK, discord_create_guild_ban(
                            client, guild_id, user_id, &ban_params,
                            &blank_ret));

    struct discord_remove_guild_ban unban_params = { .reason = "appeal" };
    ASSERT_EQ(CCORD_OK, discord_remove_guild_ban(
                            client, guild_id, user_id, &unban_params,
                            &blank_ret));

    struct discord_roles roles = { 0 };
    struct discord_ret_roles roles_ret = { .sync = &roles };
    ASSERT_EQ(CCORD_OK, discord_get_guild_roles(client, guild_id, &roles_ret));
    ASSERT_EQ(1, roles.size);
    ASSERT_EQ(role_id, roles.array[0].id);
    ASSERT_EQ(UINT64_C(274877906944), roles.array[0].permissions);
    discord_roles_cleanup(&roles);

    struct discord_create_guild_role create_role = {
        .reason = "create mod",
        .name = "Moderator",
        .permissions = UINT64_C(274877906944),
        .color = 0x5865f2,
        .hoist = true,
    };
    struct discord_role role = { 0 };
    struct discord_ret_role role_ret = { .sync = &role };
    ASSERT_EQ(CCORD_OK, discord_create_guild_role(
                            client, guild_id, &create_role, &role_ret));
    ASSERT_EQ(role_id, role.id);
    ASSERT_STR_EQ("Moderator", role.name);
    discord_role_cleanup(&role);

    struct discord_modify_guild_role_position role_pos_entry = {
        .id = role_id,
        .position = 2,
    };
    struct discord_modify_guild_role_positions role_pos_params = {
        .size = 1,
        .array = &role_pos_entry,
    };
    roles = (struct discord_roles){ 0 };
    roles_ret = (struct discord_ret_roles){ .sync = &roles };
    ASSERT_EQ(CCORD_OK, discord_modify_guild_role_positions(
                            client, guild_id, &role_pos_params, &roles_ret));
    ASSERT_EQ(1, roles.size);
    discord_roles_cleanup(&roles);

    struct discord_modify_guild_role mod_role = {
        .reason = "rename role",
        .name = "Senior Mod",
    };
    role = (struct discord_role){ 0 };
    role_ret = (struct discord_ret_role){ .sync = &role };
    ASSERT_EQ(CCORD_OK, discord_modify_guild_role(
                            client, guild_id, role_id, &mod_role, &role_ret));
    ASSERT_EQ(role_id, role.id);
    discord_role_cleanup(&role);

    struct discord_delete_guild_role del_role = { .reason = "cleanup" };
    ASSERT_EQ(CCORD_OK, discord_delete_guild_role(
                            client, guild_id, role_id, &del_role, &blank_ret));

    ASSERT_EQ(0, pthread_join(server_thread, NULL));

    ASSERT(strstr(mock.requests[2],
                  "GET /api-root/v1/guilds/9223372036854775830/members/9223372036854775831 ")
           != NULL);
    ASSERT(strstr(mock.requests[3],
                  "GET /api-root/v1/guilds/9223372036854775830/members?limit=25&after=9223372036854775831 ")
           != NULL);
    ASSERT(strstr(mock.requests[4],
                  "POST /api-root/v1/guilds/9223372036854775830/members-search ")
           != NULL);
    ASSERT(strstr(mock.requests[4], "\"query\":\"ada\"") != NULL);
    ASSERT(strstr(mock.requests[4], "\"limit\":10") != NULL);

    ASSERT(strstr(mock.requests[5],
                  "PATCH /api-root/v1/guilds/9223372036854775830/members/9223372036854775831 ")
           != NULL);
    ASSERT(strstr(mock.requests[5], "\"nick\":\"ModAda\"") != NULL);
    ASSERT(strstr(mock.requests[5], "\"mute\":true") != NULL);

    ASSERT(strstr(mock.requests[6],
                  "PATCH /api-root/v1/guilds/9223372036854775830/members/@me ")
           != NULL);
    ASSERT(strstr(mock.requests[6], "\"nick\":\"BotAda\"") != NULL);

    ASSERT(strstr(mock.requests[7],
                  "PUT /api-root/v1/guilds/9223372036854775830/members/9223372036854775831/roles/9223372036854775832 ")
           != NULL);
    ASSERT(strstr(mock.requests[8],
                  "DELETE /api-root/v1/guilds/9223372036854775830/members/9223372036854775831/roles/9223372036854775832 ")
           != NULL);
    ASSERT(strstr(mock.requests[9],
                  "DELETE /api-root/v1/guilds/9223372036854775830/members/9223372036854775831 ")
           != NULL);

    ASSERT(strstr(mock.requests[10],
                  "GET /api-root/v1/guilds/9223372036854775830/bans ")
           != NULL);
    ASSERT(strstr(mock.requests[11],
                  "PUT /api-root/v1/guilds/9223372036854775830/bans/9223372036854775831 ")
           != NULL);
    ASSERT(strstr(mock.requests[11], "\"delete_message_days\":2") != NULL);
    ASSERT(strstr(mock.requests[12],
                  "DELETE /api-root/v1/guilds/9223372036854775830/bans/9223372036854775831 ")
           != NULL);

    ASSERT(strstr(mock.requests[13],
                  "GET /api-root/v1/guilds/9223372036854775830/roles ")
           != NULL);
    ASSERT(strstr(mock.requests[14],
                  "POST /api-root/v1/guilds/9223372036854775830/roles ")
           != NULL);
    ASSERT(strstr(mock.requests[14], "\"name\":\"Moderator\"") != NULL);
    ASSERT(strstr(mock.requests[14], "\"permissions\":\"274877906944\"")
           != NULL);
    ASSERT(strstr(mock.requests[14], "\"icon\":\"\"") == NULL);

    ASSERT(strstr(mock.requests[15],
                  "PATCH /api-root/v1/guilds/9223372036854775830/roles ")
           != NULL);
    ASSERT(strstr(mock.requests[15], "\"id\":\"9223372036854775832\"")
           != NULL);
    ASSERT(strstr(mock.requests[15], "\"position\":2") != NULL);

    ASSERT(strstr(mock.requests[16],
                  "PATCH /api-root/v1/guilds/9223372036854775830/roles/9223372036854775832 ")
           != NULL);
    ASSERT(strstr(mock.requests[16], "\"name\":\"Senior Mod\"") != NULL);
    ASSERT(strstr(mock.requests[16], "\"icon\":\"\"") == NULL);

    ASSERT(strstr(mock.requests[17],
                  "DELETE /api-root/v1/guilds/9223372036854775830/roles/9223372036854775832 ")
           != NULL);

    discord_cleanup(client);
    unlink(config_path);
    PASS();
}

GREATEST_MAIN_DEFS();

int
main(int argc, char *argv[])
{
    GREATEST_MAIN_BEGIN();

    RUN_TEST(unsupported_guild_operations_fail_without_http_requests);
    RUN_TEST(guild_and_channel_endpoints_use_fluxer_routes_and_large_snowflakes);
    RUN_TEST(member_role_and_ban_endpoints_use_fluxer_routes_and_codecs);

    GREATEST_MAIN_END();
}
