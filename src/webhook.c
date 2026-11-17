#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <curl/curl.h>

#include "discord.h"
#include "discord-internal.h"
#include "discord-request.h"

#define WEBHOOK_MAX_URL_LEN 2560

struct _webhook_http_buf {
    char *data;
    size_t size;
};

struct _webhook_async_task {
    struct discord *client;
    struct discord_attributes attr;
    struct ccord_szbuf body;
    enum http_method method;
    const char *custom_method;
    char endpoint[DISCORD_ENDPT_LEN];
};

static CCORDcode
_discord_webhook_bad_auth(
    struct discord *client,
    void (*fail)(struct discord *client, struct discord_response *response),
    void *data,
    const void *keep)
{
    if (fail) {
        struct discord_response response = {
            .data = data,
            .keep = keep,
            .code = CCORD_DISCORD_BAD_AUTH,
        };
        fail(client, &response);
    }
    return CCORD_DISCORD_BAD_AUTH;
}

static size_t
_webhook_http_write(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct _webhook_http_buf *buf = userdata;
    size_t chunk = size * nmemb;
    char *next;

    if (chunk == 0) return 0;
    next = realloc(buf->data, buf->size + chunk + 1);
    if (!next) return 0;
    buf->data = next;
    memcpy(buf->data + buf->size, ptr, chunk);
    buf->size += chunk;
    buf->data[buf->size] = '\0';
    return chunk;
}

static void
_webhook_attachments_dup(struct discord_attachments *dest,
                         const struct discord_attachments *src)
{
    int i;

    __carray_init(dest, (size_t)src->size, struct discord_attachment, , );
    for (i = 0; i < src->size; ++i) {
        carray_insert(dest, i, src->array[i]);
        dest->array[i].description = NULL;
        dest->array[i].url = NULL;
        dest->array[i].proxy_url = NULL;
        dest->array[i].waveform = NULL;
        if (src->array[i].content) {
            dest->array[i].size = src->array[i].size
                                      ? src->array[i].size
                                      : strlen(src->array[i].content) + 1;
            dest->array[i].content = malloc(dest->array[i].size);
            if (dest->array[i].content) {
                memcpy(dest->array[i].content, src->array[i].content,
                       dest->array[i].size);
            }
        }
        if (src->array[i].filename) {
            cog_strndup(src->array[i].filename, strlen(src->array[i].filename),
                        &dest->array[i].filename);
        }
        if (src->array[i].content_type) {
            cog_strndup(src->array[i].content_type,
                        strlen(src->array[i].content_type),
                        &dest->array[i].content_type);
        }
    }
    dest->size = i;
}

static curl_mime *
_webhook_build_mime(CURL *curl,
                    const struct ccord_szbuf *body,
                    const struct discord_attachments *attachments)
{
    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *part;
    char name[64];

    if (!mime) return NULL;

    if (body && body->start && body->size) {
        part = curl_mime_addpart(mime);
        curl_mime_data(part, body->start, body->size);
        curl_mime_type(part, "application/json");
        curl_mime_name(part, "payload_json");
    }

    if (attachments) {
        for (int i = 0; i < attachments->size; ++i) {
            if (snprintf(name, sizeof(name), "files[%" PRIu64 "]",
                         attachments->array[i].id)
                >= (int)sizeof(name))
            {
                continue;
            }
            if (attachments->array[i].content) {
                size_t data_sz = attachments->array[i].size
                                     ? attachments->array[i].size
                                     : CURL_ZERO_TERMINATED;
                part = curl_mime_addpart(mime);
                curl_mime_data(part, attachments->array[i].content, data_sz);
                curl_mime_filename(part, attachments->array[i].filename
                                             ? attachments->array[i].filename
                                             : "a.out");
                curl_mime_type(part, attachments->array[i].content_type
                                         ? attachments->array[i].content_type
                                         : "application/octet-stream");
                curl_mime_name(part, name);
            }
            else if (attachments->array[i].filename) {
                part = curl_mime_addpart(mime);
                if (curl_mime_filedata(part, attachments->array[i].filename)
                    != CURLE_OK)
                {
                    continue;
                }
                curl_mime_type(part, attachments->array[i].content_type
                                         ? attachments->array[i].content_type
                                         : "application/octet-stream");
                curl_mime_name(part, name);
            }
        }
    }

    return mime;
}

static long
_webhook_parse_retry_after_us(const char *json, size_t size)
{
    long wait_us = 1000000L;
    jsmnf_table table[16];
    jsmnf_loader loader;

    if (!json || !size) return wait_us;
    jsmnf_init(&loader);
    if (0 < jsmnf_load(&loader, json, size, table,
                       sizeof(table) / sizeof(*table)))
    {
        const jsmnf_pair *f = jsmnf_find(loader.root, "retry_after", 11);
        if (f && f->v && f->v->start >= 0) {
            double sec = strtod(json + f->v->start, NULL);
            if (sec > 0.0) wait_us = (long)(sec * 1000000.0);
        }
    }
    return wait_us;
}

static CCORDcode
_webhook_perform_http(struct discord *client,
                      const struct ccord_szbuf *body,
                      const struct discord_attachments *attachments,
                      enum http_method method,
                      const char *custom_method,
                      const char endpoint[],
                      struct _webhook_http_buf *out_buf)
{
    char url[WEBHOOK_MAX_URL_LEN];
    int max_retries = client->rest.requestor.retry_limit > 0
                          ? client->rest.requestor.retry_limit
                          : 3;
    CCORDcode code = CCORD_HTTP_CODE;

    if (!NOT_EMPTY_STR(client->api_base_url)) return CCORD_BAD_PARAMETER;
    if (snprintf(url, sizeof(url), "%s%s", client->api_base_url, endpoint)
        >= (int)sizeof(url))
    {
        return CCORD_ERRNO;
    }

    for (int attempt = 0; attempt <= max_retries; ++attempt) {
        CURL *curl = curl_easy_init();
        struct curl_slist *headers = NULL;
        curl_mime *mime = NULL;
        CURLcode res;
        long http_code = 0;

        free(out_buf->data);
        out_buf->data = NULL;
        out_buf->size = 0;

        if (!curl) return CCORD_OUT_OF_MEMORY;

        /* Deliberately send NO Authorization header on token-authenticated
         * Fluxer webhook routes (/v1/webhooks/{id}/{token}...). */
        headers = curl_slist_append(
            headers, "User-Agent: Cogmasters (https://github.com/Cogmasters)");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
        curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _webhook_http_write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_buf);

        switch (method) {
        case HTTP_GET:
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            break;
        case HTTP_DELETE:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
        case HTTP_MIMEPOST:
            mime = _webhook_build_mime(curl, body, attachments);
            if (!mime) {
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                return CCORD_OUT_OF_MEMORY;
            }
            curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
            if (custom_method) {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, custom_method);
            }
            break;
        case HTTP_POST:
            headers =
                curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                             (body && body->start) ? body->start : "");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
                             (curl_off_t)(body ? body->size : 0));
            break;
        case HTTP_PATCH:
            headers =
                curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                             (body && body->start) ? body->start : "");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
                             (curl_off_t)(body ? body->size : 0));
            break;
        case HTTP_PUT:
        case HTTP_INVALID:
        default:
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return CCORD_BAD_PARAMETER;
        }

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        }

        if (mime) curl_mime_free(mime);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            if (res == CURLE_READ_ERROR && attempt < max_retries) continue;
            return CCORD_CURLE_INTERNAL;
        }

        switch (http_code) {
        case HTTP_OK:
        case HTTP_CREATED:
        case HTTP_NO_CONTENT:
            return CCORD_OK;
        case HTTP_BAD_REQUEST:
        case HTTP_FORBIDDEN:
        case HTTP_NOT_FOUND:
        case HTTP_UNPROCESSABLE_ENTITY:
            return CCORD_DISCORD_JSON_CODE;
        case HTTP_UNAUTHORIZED:
            return CCORD_DISCORD_BAD_AUTH;
        case HTTP_TOO_MANY_REQUESTS:
            if (attempt < max_retries) {
                cog_sleep_us(
                    _webhook_parse_retry_after_us(out_buf->data, out_buf->size));
                continue;
            }
            return CCORD_DISCORD_RATELIMIT;
        default:
            if (http_code >= 500 && attempt < max_retries) continue;
            code = CCORD_HTTP_CODE;
            return code;
        }
    }

    return code;
}

static void *
_webhook_async_worker(void *userdata)
{
    struct _webhook_async_task *task = userdata;
    struct discord *client = task->client;
    struct discord_requestor *rqtor = &client->rest.requestor;
    struct _webhook_http_buf resp_buf = { 0 };
    struct discord_request *req = NULL;
    CCORDcode code;

    code = _webhook_perform_http(client, &task->body, &task->attr.attachments,
                                 task->method, task->custom_method,
                                 task->endpoint, &resp_buf);

    if (task->body.start && !task->body.is_static) free(task->body.start);
    discord_attachments_cleanup(&task->attr.attachments);

    pthread_mutex_lock(&rqtor->qlocks->recycling);
    if (!QUEUE_EMPTY(&rqtor->queues->recycling)) {
        QUEUE(struct discord_request) *qelem =
            QUEUE_HEAD(&rqtor->queues->recycling);
        QUEUE_REMOVE(qelem);
        req = QUEUE_DATA(qelem, struct discord_request, entry);
    }
    pthread_mutex_unlock(&rqtor->qlocks->recycling);

    if (!req) req = calloc(1, sizeof(*req));
    if (req) {
        memset(req, 0, sizeof(*req));
        QUEUE_INIT(&req->entry);
        req->dispatch = task->attr.dispatch;
        req->response = task->attr.response;
        req->body.is_static = true;
        req->code = code;
        req->json.start = resp_buf.data;
        req->json.size = resp_buf.size;

        if (code == CCORD_OK && req->dispatch.has_type) {
            req->response.data = calloc(1, req->response.size);
            if (req->response.data) {
                discord_refcounter_add_internal(
                    &client->refcounter, req->response.data,
                    req->response.cleanup, true);
                if (req->response.init) req->response.init(req->response.data);
                if (req->response.from_json && resp_buf.data) {
                    req->response.from_json(resp_buf.data, resp_buf.size,
                                            req->response.data);
                }
            }
        }

        pthread_mutex_lock(&rqtor->qlocks->finished);
        QUEUE_INSERT_TAIL(&rqtor->queues->finished, &req->entry);
        pthread_mutex_unlock(&rqtor->qlocks->finished);
        io_poller_wakeup(client->io_poller);
    }
    else {
        free(resp_buf.data);
    }

    free(task);

    pthread_mutex_lock(&client->workers->lock);
    --client->workers->count;
    pthread_cond_signal(&client->workers->cond);
    pthread_mutex_unlock(&client->workers->lock);

    return NULL;
}

static CCORDcode
_discord_webhook_token_run(struct discord *client,
                           const struct discord_attributes *attr,
                           struct ccord_szbuf *body,
                           enum http_method method,
                           const char *custom_method,
                           const char endpoint_fmt[],
                           ...)
{
    static const struct discord_attributes blank_attr = { 0 };
    char endpoint[DISCORD_ENDPT_LEN];
    va_list args;

    if (!attr) attr = &blank_attr;

    va_start(args, endpoint_fmt);
    if (vsnprintf(endpoint, sizeof(endpoint), endpoint_fmt, args)
        >= (int)sizeof(endpoint))
    {
        va_end(args);
        if (body && body->start && !body->is_static) free(body->start);
        return CCORD_ERRNO;
    }
    va_end(args);

    if (attr->dispatch.sync) {
        struct _webhook_http_buf resp_buf = { 0 };
        CCORDcode code = _webhook_perform_http(
            client, body, &attr->attachments, method, custom_method, endpoint,
            &resp_buf);

        if (body && body->start && !body->is_static) {
            free(body->start);
            body->start = NULL;
        }

        if (code == CCORD_OK && attr->dispatch.has_type
            && attr->dispatch.sync != DISCORD_SYNC_FLAG)
        {
            if (attr->response.init) attr->response.init(attr->dispatch.sync);
            if (attr->response.from_json && resp_buf.data) {
                attr->response.from_json(resp_buf.data, resp_buf.size,
                                         attr->dispatch.sync);
            }
        }

        {
            struct discord_response resp = {
                .data = attr->dispatch.data,
                .keep = attr->dispatch.keep,
                .code = code,
                .json = { resp_buf.data, resp_buf.size },
            };
            if (code != CCORD_OK) {
                if (attr->dispatch.fail) attr->dispatch.fail(client, &resp);
            }
            else if (attr->dispatch.done.typed) {
                if (!attr->dispatch.has_type) {
                    attr->dispatch.done.typeless(client, &resp);
                }
                else {
                    attr->dispatch.done.typed(
                        client, &resp,
                        attr->dispatch.sync != DISCORD_SYNC_FLAG
                            ? attr->dispatch.sync
                            : NULL);
                }
            }
        }

        free(resp_buf.data);
        return code;
    }
    else {
        struct _webhook_async_task *task;
        CCORDcode code;

        if (attr->dispatch.keep) {
            code = discord_refcounter_incr(&client->refcounter,
                                           (void *)attr->dispatch.keep);
            if (code < 0) {
                if (body && body->start && !body->is_static) free(body->start);
                return code;
            }
        }
        if (attr->dispatch.data
            && CCORD_RESOURCE_UNAVAILABLE
                   == discord_refcounter_incr(&client->refcounter,
                                              attr->dispatch.data))
        {
            discord_refcounter_add_client(&client->refcounter,
                                          attr->dispatch.data,
                                          attr->dispatch.cleanup, false);
        }

        task = calloc(1, sizeof(*task));
        if (!task) {
            if (body && body->start && !body->is_static) free(body->start);
            return CCORD_OUT_OF_MEMORY;
        }
        task->client = client;
        task->attr.dispatch = attr->dispatch;
        task->attr.response = attr->response;
        if (attr->attachments.size > 0) {
            _webhook_attachments_dup(&task->attr.attachments,
                                     &attr->attachments);
        }
        if (body) {
            task->body = *body;
            body->start = NULL;
        }
        else {
            task->body.is_static = true;
        }
        task->method = method;
        task->custom_method = custom_method;
        memcpy(task->endpoint, endpoint, sizeof(task->endpoint));

        {
            pthread_attr_t tattr;
            pthread_t tid;
            int rc;

            pthread_mutex_lock(&client->workers->lock);
            ++client->workers->count;
            pthread_mutex_unlock(&client->workers->lock);

            pthread_attr_init(&tattr);
            pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
            rc = pthread_create(&tid, &tattr, _webhook_async_worker, task);
            pthread_attr_destroy(&tattr);
            if (rc != 0) {
                pthread_mutex_lock(&client->workers->lock);
                --client->workers->count;
                pthread_cond_signal(&client->workers->cond);
                pthread_mutex_unlock(&client->workers->lock);
                if (task->body.start && !task->body.is_static)
                    free(task->body.start);
                discord_attachments_cleanup(&task->attr.attachments);
                free(task);
                return CCORD_ERRNO;
            }
        }
        return CCORD_PENDING;
    }
}

static bool
_webhook_token_is_safe(const char token[])
{
    if (!NOT_EMPTY_STR(token)) return false;
    for (const unsigned char *p = (const unsigned char *)token; *p; ++p) {
        if (*p <= 0x20 || *p >= 0x7f || *p == '/' || *p == '\\' || *p == '?'
            || *p == '#')
        {
            return false;
        }
    }
    return true;
}

static bool
_webhook_has_upload_attachments(const struct discord_attachments *attachments)
{
    if (!attachments || attachments->size <= 0) return false;
    for (int i = 0; i < attachments->size; ++i) {
        if (attachments->array[i].content != NULL
            || (attachments->array[i].id == 0
                && NOT_EMPTY_STR(attachments->array[i].filename)))
        {
            return true;
        }
    }
    return false;
}

CCORDcode
discord_create_webhook(struct discord *client,
                       u64snowflake channel_id,
                       struct discord_create_webhook *params,
                       struct discord_ret_webhook *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };

    if (!client) return CCORD_BAD_PARAMETER;
    if (!NOT_EMPTY_STR(client->config.token)) {
        return _discord_webhook_bad_auth(client, ret ? ret->fail : NULL,
                                         ret ? ret->data : NULL,
                                         ret ? ret->keep : NULL);
    }
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, NOT_EMPTY_STR(params->name), CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_create_webhook_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_webhook, ret, params->reason);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_POST,
                            "/channels/%" PRIu64 "/webhooks", channel_id);
}

CCORDcode
discord_get_channel_webhooks(struct discord *client,
                             u64snowflake channel_id,
                             struct discord_ret_webhooks *ret)
{
    struct discord_attributes attr = { 0 };

    if (!client) return CCORD_BAD_PARAMETER;
    if (!NOT_EMPTY_STR(client->config.token)) {
        return _discord_webhook_bad_auth(client, ret ? ret->fail : NULL,
                                         ret ? ret->data : NULL,
                                         ret ? ret->keep : NULL);
    }
    CCORD_EXPECT(client, channel_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_LIST_INIT(attr, discord_webhooks, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/channels/%" PRIu64 "/webhooks", channel_id);
}

CCORDcode
discord_get_guild_webhooks(struct discord *client,
                           u64snowflake guild_id,
                           struct discord_ret_webhooks *ret)
{
    struct discord_attributes attr = { 0 };

    if (!client) return CCORD_BAD_PARAMETER;
    if (!NOT_EMPTY_STR(client->config.token)) {
        return _discord_webhook_bad_auth(client, ret ? ret->fail : NULL,
                                         ret ? ret->data : NULL,
                                         ret ? ret->keep : NULL);
    }
    CCORD_EXPECT(client, guild_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_LIST_INIT(attr, discord_webhooks, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/guilds/%" PRIu64 "/webhooks", guild_id);
}

CCORDcode
discord_get_webhook(struct discord *client,
                    u64snowflake webhook_id,
                    struct discord_ret_webhook *ret)
{
    struct discord_attributes attr = { 0 };

    if (!client) return CCORD_BAD_PARAMETER;
    if (!NOT_EMPTY_STR(client->config.token)) {
        return _discord_webhook_bad_auth(client, ret ? ret->fail : NULL,
                                         ret ? ret->data : NULL,
                                         ret ? ret->keep : NULL);
    }
    CCORD_EXPECT(client, webhook_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_INIT(attr, discord_webhook, ret, NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_GET,
                            "/webhooks/%" PRIu64, webhook_id);
}

CCORDcode
discord_get_webhook_with_token(struct discord *client,
                               u64snowflake webhook_id,
                               const char webhook_token[],
                               struct discord_ret_webhook *ret)
{
    struct discord_attributes attr = { 0 };

    if (!client) return CCORD_BAD_PARAMETER;
    CCORD_EXPECT(client, webhook_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, _webhook_token_is_safe(webhook_token),
                 CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_INIT(attr, discord_webhook, ret, NULL);
    return _discord_webhook_token_run(client, &attr, NULL, HTTP_GET, NULL,
                                      "/webhooks/%" PRIu64 "/%s", webhook_id,
                                      webhook_token);
}

CCORDcode
discord_modify_webhook(struct discord *client,
                       u64snowflake webhook_id,
                       struct discord_modify_webhook *params,
                       struct discord_ret_webhook *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };

    if (!client) return CCORD_BAD_PARAMETER;
    if (!NOT_EMPTY_STR(client->config.token)) {
        return _discord_webhook_bad_auth(client, ret ? ret->fail : NULL,
                                         ret ? ret->data : NULL,
                                         ret ? ret->keep : NULL);
    }
    CCORD_EXPECT(client, webhook_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_modify_webhook_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_webhook, ret,
                      params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, &body, HTTP_PATCH,
                            "/webhooks/%" PRIu64, webhook_id);
}

CCORDcode
discord_modify_webhook_with_token(
    struct discord *client,
    u64snowflake webhook_id,
    const char webhook_token[],
    struct discord_modify_webhook_with_token *params,
    struct discord_ret_webhook *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };

    if (!client) return CCORD_BAD_PARAMETER;
    CCORD_EXPECT(client, webhook_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, _webhook_token_is_safe(webhook_token),
                 CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_modify_webhook_with_token_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_webhook, ret, NULL);
    return _discord_webhook_token_run(client, &attr, &body, HTTP_PATCH, NULL,
                                      "/webhooks/%" PRIu64 "/%s", webhook_id,
                                      webhook_token);
}

CCORDcode
discord_delete_webhook(struct discord *client,
                       u64snowflake webhook_id,
                       struct discord_delete_webhook *params,
                       struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };

    if (!client) return CCORD_BAD_PARAMETER;
    if (!NOT_EMPTY_STR(client->config.token)) {
        return _discord_webhook_bad_auth(client, ret ? ret->fail : NULL,
                                         ret ? ret->data : NULL,
                                         ret ? ret->keep : NULL);
    }
    CCORD_EXPECT(client, webhook_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, params ? params->reason : NULL);
    return discord_rest_run(&client->rest, &attr, NULL, HTTP_DELETE,
                            "/webhooks/%" PRIu64, webhook_id);
}

CCORDcode
discord_delete_webhook_with_token(struct discord *client,
                                  u64snowflake webhook_id,
                                  const char webhook_token[],
                                  struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };

    if (!client) return CCORD_BAD_PARAMETER;
    CCORD_EXPECT(client, webhook_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, _webhook_token_is_safe(webhook_token),
                 CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return _discord_webhook_token_run(client, &attr, NULL, HTTP_DELETE, NULL,
                                      "/webhooks/%" PRIu64 "/%s", webhook_id,
                                      webhook_token);
}

CCORDcode
discord_execute_webhook(struct discord *client,
                        u64snowflake webhook_id,
                        const char webhook_token[],
                        struct discord_execute_webhook *params,
                        struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    enum http_method method;
    char query[128] = "";

    if (!client) return CCORD_BAD_PARAMETER;
    CCORD_EXPECT(client, webhook_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, _webhook_token_is_safe(webhook_token),
                 CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_execute_webhook_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);

    if (params->wait && params->thread_id != 0) {
        snprintf(query, sizeof(query), "?wait=true&thread_id=%" PRIu64,
                 params->thread_id);
    }
    else if (params->wait) {
        snprintf(query, sizeof(query), "?wait=true");
    }
    else if (params->thread_id != 0) {
        snprintf(query, sizeof(query), "?thread_id=%" PRIu64,
                 params->thread_id);
    }

    if (params->attachments && params->attachments->size > 0) {
        method = HTTP_MIMEPOST;
        DISCORD_ATTACHMENTS_IDS_INIT(params->attachments);
        attr.attachments = *params->attachments;
    }
    else {
        method = HTTP_POST;
    }

    return _discord_webhook_token_run(client, &attr, &body, method, NULL,
                                      "/webhooks/%" PRIu64 "/%s%s", webhook_id,
                                      webhook_token, query);
}

CCORDcode
discord_get_webhook_message(struct discord *client,
                            u64snowflake webhook_id,
                            const char webhook_token[],
                            u64snowflake message_id,
                            struct discord_ret_message *ret)
{
    struct discord_attributes attr = { 0 };

    if (!client) return CCORD_BAD_PARAMETER;
    CCORD_EXPECT(client, webhook_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, _webhook_token_is_safe(webhook_token),
                 CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_INIT(attr, discord_message, ret, NULL);
    return _discord_webhook_token_run(
        client, &attr, NULL, HTTP_GET, NULL,
        "/webhooks/%" PRIu64 "/%s/messages/%" PRIu64, webhook_id,
        webhook_token, message_id);
}

CCORDcode
discord_edit_webhook_message(struct discord *client,
                             u64snowflake webhook_id,
                             const char webhook_token[],
                             u64snowflake message_id,
                             struct discord_edit_webhook_message *params,
                             struct discord_ret_message *ret)
{
    struct discord_attributes attr = { 0 };
    struct ccord_szbuf body = { 0 };
    enum http_method method;
    const char *custom_method = NULL;
    char query[64] = "";

    if (!client) return CCORD_BAD_PARAMETER;
    CCORD_EXPECT(client, webhook_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, _webhook_token_is_safe(webhook_token),
                 CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, params != NULL, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT_OK(client, discord_edit_webhook_message_to_json(
                                &body.start, &body.size, params));
    DISCORD_ATTR_INIT(attr, discord_message, ret, NULL);

    if (params->thread_id != 0) {
        snprintf(query, sizeof(query), "?thread_id=%" PRIu64,
                 params->thread_id);
    }

    if (_webhook_has_upload_attachments(params->attachments)) {
        method = HTTP_MIMEPOST;
        custom_method = "PATCH";
        DISCORD_ATTACHMENTS_IDS_INIT(params->attachments);
        attr.attachments = *params->attachments;
    }
    else {
        method = HTTP_PATCH;
    }

    return _discord_webhook_token_run(
        client, &attr, &body, method, custom_method,
        "/webhooks/%" PRIu64 "/%s/messages/%" PRIu64 "%s", webhook_id,
        webhook_token, message_id, query);
}

CCORDcode
discord_delete_webhook_message(struct discord *client,
                               u64snowflake webhook_id,
                               const char webhook_token[],
                               u64snowflake message_id,
                               struct discord_ret *ret)
{
    struct discord_attributes attr = { 0 };

    if (!client) return CCORD_BAD_PARAMETER;
    CCORD_EXPECT(client, webhook_id != 0, CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, _webhook_token_is_safe(webhook_token),
                 CCORD_BAD_PARAMETER, "");
    CCORD_EXPECT(client, message_id != 0, CCORD_BAD_PARAMETER, "");
    DISCORD_ATTR_BLANK_INIT(attr, ret, NULL);
    return _discord_webhook_token_run(
        client, &attr, NULL, HTTP_DELETE, NULL,
        "/webhooks/%" PRIu64 "/%s/messages/%" PRIu64, webhook_id,
        webhook_token, message_id);
}
