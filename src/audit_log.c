#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "discord.h"
#include "discord-internal.h"
#include "discord-request.h"

static CCORDcode
_audit_query_append(char *query,
                    size_t query_cap,
                    size_t *offset,
                    const char *key,
                    const char *fmt,
                    ...)
{
    va_list args;
    char sep = (*offset == 0) ? '?' : '&';
    int written =
        snprintf(query + *offset, query_cap - *offset, "%c%s=", sep, key);
    if (written < 0 || (size_t)written >= query_cap - *offset)
        return CCORD_MALFORMED_PAYLOAD;
    *offset += (size_t)written;

    va_start(args, fmt);
    written = vsnprintf(query + *offset, query_cap - *offset, fmt, args);
    va_end(args);
    if (written < 0 || (size_t)written >= query_cap - *offset)
        return CCORD_MALFORMED_PAYLOAD;
    *offset += (size_t)written;
    return CCORD_OK;
}

CCORDcode
discord_get_guild_audit_log(struct discord *client,
                            u64snowflake guild_id,
                            struct discord_get_guild_audit_log *params,
                            struct discord_ret_audit_log *ret)
{
    struct discord_attributes attr = { 0 };
    char query[1024] = "";
    size_t offset = 0;

    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    if (params) {
        CCORDcode code;
        if (params->user_id) {
            code = _audit_query_append(query, sizeof(query), &offset,
                                       "user_id", "%" PRIu64, params->user_id);
            CCORD_EXPECT(client, code == CCORD_OK, code,
                         "Out of bounds write attempt");
        }
        if (params->action_type) {
            code = _audit_query_append(query, sizeof(query), &offset,
                                       "action_type", "%d",
                                       params->action_type);
            CCORD_EXPECT(client, code == CCORD_OK, code,
                         "Out of bounds write attempt");
        }
        if (params->before) {
            code = _audit_query_append(query, sizeof(query), &offset, "before",
                                       "%" PRIu64, params->before);
            CCORD_EXPECT(client, code == CCORD_OK, code,
                         "Out of bounds write attempt");
        }
        if (params->after) {
            code = _audit_query_append(query, sizeof(query), &offset, "after",
                                       "%" PRIu64, params->after);
            CCORD_EXPECT(client, code == CCORD_OK, code,
                         "Out of bounds write attempt");
        }
        if (params->limit) {
            code = _audit_query_append(query, sizeof(query), &offset, "limit",
                                       "%d", params->limit);
            CCORD_EXPECT(client, code == CCORD_OK, code,
                         "Out of bounds write attempt");
        }
    }
    DISCORD_ATTR_INIT(attr, discord_audit_log, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/guilds/%" PRIu64 "/audit-logs%s", guild_id,
                            query);
}
