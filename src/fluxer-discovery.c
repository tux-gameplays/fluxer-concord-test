#include <curl/curl.h>
#include <stdbool.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fluxer-discovery.h"
#include "cog-utils.h"
#define JSMN_HEADER
#include "jsmn.h"
#include "jsmn-find.h"
#undef JSMN_HEADER

#define FLUXER_DISCOVERY_MAX_BODY (64 * 1024)
#define FLUXER_DISCOVERY_MAX_URL 2048

struct fluxer_http_body {
    char *data;
    size_t size;
    bool too_large;
};

static bool
_url_has_scheme_and_host(const char url[], const char *const schemes[])
{
    CURLU *handle = curl_url();
    char *scheme = NULL, *host = NULL, *user = NULL, *password = NULL;
    bool valid = false;

    if (!handle) return false;
    if (curl_url_set(handle, CURLUPART_URL, url, 0) != CURLUE_OK
        || curl_url_get(handle, CURLUPART_SCHEME, &scheme, 0) != CURLUE_OK
        || curl_url_get(handle, CURLUPART_HOST, &host, 0) != CURLUE_OK)
    {
        goto done;
    }
    if (curl_url_get(handle, CURLUPART_USER, &user, 0) == CURLUE_OK
        || curl_url_get(handle, CURLUPART_PASSWORD, &password, 0) == CURLUE_OK)
    {
        goto done;
    }

    for (size_t i = 0; schemes[i]; ++i) {
        if (0 == strcasecmp(scheme, schemes[i])) {
            valid = true;
            break;
        }
    }

done:
    if (scheme) curl_free(scheme);
    if (host) curl_free(host);
    if (user) curl_free(user);
    if (password) curl_free(password);
    curl_url_cleanup(handle);
    return valid;
}

static bool
_valid_instance_origin(const char origin[])
{
    static const char *const schemes[] = { "http", "https", NULL };
    CURLU *handle = curl_url();
    char *scheme = NULL, *host = NULL, *path = NULL;
    bool valid = false;

    if (!handle) return false;
    if (curl_url_set(handle, CURLUPART_URL, origin, 0) != CURLUE_OK
        || curl_url_get(handle, CURLUPART_SCHEME, &scheme, 0) != CURLUE_OK
        || curl_url_get(handle, CURLUPART_HOST, &host, 0) != CURLUE_OK)
    {
        goto done;
    }

    for (size_t i = 0; schemes[i]; ++i) {
        if (0 == strcasecmp(scheme, schemes[i])) {
            valid = true;
            break;
        }
    }
    if (!valid) goto done;

    if (curl_url_get(handle, CURLUPART_PATH, &path, 0) == CURLUE_OK
        && path[0] && strcmp(path, "/") != 0) {
        valid = false;
    }
    if (path) curl_free(path);
    path = NULL;
    if (curl_url_get(handle, CURLUPART_USER, &path, 0) == CURLUE_OK) {
        curl_free(path);
        path = NULL;
        valid = false;
    }
    if (curl_url_get(handle, CURLUPART_QUERY, &path, 0) == CURLUE_OK) {
        curl_free(path);
        path = NULL;
        valid = false;
    }
    if (curl_url_get(handle, CURLUPART_FRAGMENT, &path, 0) == CURLUE_OK) {
        curl_free(path);
        path = NULL;
        valid = false;
    }

done:
    if (scheme) curl_free(scheme);
    if (host) curl_free(host);
    if (path) curl_free(path);
    curl_url_cleanup(handle);
    return valid;
}

static char *
_discovery_string(const char json[], size_t size, const jsmnf_loader *loader,
                  char *const path[], unsigned depth)
{
    const jsmnf_pair *field = jsmnf_find_path(loader->root, path, depth);
    if (!field || !field->v || field->v->type != JSMN_STRING
        || field->v->start < 0 || field->v->end < field->v->start
        || (size_t)field->v->end > size)
    {
        return NULL;
    }

    char *value = NULL;
    size_t length = (size_t)(field->v->end - field->v->start);
    if (!length || length > FLUXER_DISCOVERY_MAX_URL) return NULL;
    if (cog_strndup(json + field->v->start, length, &value) != length)
        return NULL;
    return value;
}

void
fluxer_discovery_cleanup(struct fluxer_discovery *discovery)
{
    if (!discovery) return;
    free(discovery->api_base);
    free(discovery->gateway_url);
    free(discovery->media_url);
    memset(discovery, 0, sizeof(*discovery));
}

CCORDcode
fluxer_discovery_parse(const char json[], size_t size,
                       struct fluxer_discovery *discovery)
{
    static const char *const api_schemes[] = { "http", "https", NULL };
    static const char *const gateway_schemes[] = { "ws", "wss", NULL };
    jsmnf_loader loader;
    struct fluxer_discovery parsed = { 0 };
    jsmnf_table *table;
    long result;
    CCORDcode code = CCORD_BAD_DECODE;

    if (!json || !discovery || !size || size > FLUXER_DISCOVERY_MAX_BODY)
        return CCORD_BAD_PARAMETER;
    memset(discovery, 0, sizeof(*discovery));
    table = calloc(size + 1, sizeof(*table));
    if (!table) return CCORD_OUT_OF_MEMORY;

    jsmnf_init(&loader);
    result = jsmnf_load(&loader, json, size, table, size + 1);
    if (result <= 0) goto done;

    parsed.api_base = _discovery_string(
        json, size, &loader, (char *[2]){ "endpoints", "api_public" }, 2);
    parsed.gateway_url = _discovery_string(
        json, size, &loader, (char *[2]){ "endpoints", "gateway" }, 2);
    parsed.media_url = _discovery_string(
        json, size, &loader, (char *[2]){ "endpoints", "media" }, 2);
    if (!parsed.api_base || !parsed.gateway_url || !parsed.media_url
        || !_url_has_scheme_and_host(parsed.api_base, api_schemes)
        || !_url_has_scheme_and_host(parsed.gateway_url, gateway_schemes)
        || !_url_has_scheme_and_host(parsed.media_url, api_schemes))
    {
        goto done;
    }

    size_t api_length = strlen(parsed.api_base);
    while (api_length && parsed.api_base[api_length - 1] == '/')
        parsed.api_base[--api_length] = '\0';
    char *versioned_api = malloc(api_length + sizeof("/v1"));
    if (!versioned_api) {
        code = CCORD_OUT_OF_MEMORY;
        goto done;
    }
    memcpy(versioned_api, parsed.api_base, api_length);
    memcpy(versioned_api + api_length, "/v1", sizeof("/v1"));
    free(parsed.api_base);
    parsed.api_base = versioned_api;

    *discovery = parsed;
    memset(&parsed, 0, sizeof(parsed));
    code = CCORD_OK;

done:
    free(table);
    fluxer_discovery_cleanup(&parsed);
    return code;
}

static size_t
_fluxer_http_write(char *data, size_t size, size_t count, void *user_data)
{
    struct fluxer_http_body *body = user_data;
    size_t length = size * count;
    if (length > FLUXER_DISCOVERY_MAX_BODY - body->size) {
        body->too_large = true;
        return 0;
    }

    char *next = realloc(body->data, body->size + length + 1);
    if (!next) return 0;
    body->data = next;
    memcpy(body->data + body->size, data, length);
    body->size += length;
    body->data[body->size] = '\0';
    return length;
}

CCORDcode
fluxer_discovery_fetch(const char origin[], struct fluxer_discovery *discovery)
{
    struct fluxer_http_body body = { 0 };
    CURL *curl = NULL;
    CURLcode result;
    long http_status = 0;
    char url[FLUXER_DISCOVERY_MAX_URL + 64];
    size_t origin_length;
    CCORDcode code;

    if (!discovery) return CCORD_BAD_PARAMETER;
    memset(discovery, 0, sizeof(*discovery));
    if (!origin || !_valid_instance_origin(origin)) return CCORD_BAD_PARAMETER;

    origin_length = strlen(origin);
    while (origin_length && origin[origin_length - 1] == '/') --origin_length;
    if (origin_length > sizeof(url) - sizeof("/.well-known/fluxer"))
        return CCORD_BAD_PARAMETER;
    snprintf(url, sizeof(url), "%.*s/.well-known/fluxer", (int)origin_length,
             origin);

    curl = curl_easy_init();
    if (!curl) return CCORD_OUT_OF_MEMORY;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _fluxer_http_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        code = body.too_large ? CCORD_BAD_DECODE : CCORD_CURL_NO_RESPONSE;
        goto done;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
    if (http_status != 200) {
        code = CCORD_HTTP_CODE;
        goto done;
    }
    code = fluxer_discovery_parse(body.data, body.size, discovery);

done:
    curl_easy_cleanup(curl);
    free(body.data);
    return code;
}
