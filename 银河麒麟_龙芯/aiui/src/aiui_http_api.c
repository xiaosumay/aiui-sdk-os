#include <stdlib.h>
#include <string.h>
#include "aiui_wrappers_os.h"
#include "aiui_log.h"
#include "aiui_socket_api.h"
#include "aiui_http_api.h"

#define AIUI_TAG "aiui_http"

typedef struct aiui_http_response_s
{
    int is_header_completed;
    int is_chunked;
    int contentLength;
    int is_over;
    int expectedChunkSize;
    int removecrlf;
    int recv_body_len;
    // 主要是用来解析chunk模式下的body数据
    int cur_body_len;
    char *cur_body;

} aiui_http_response_t;

typedef struct aiui_http_info_s
{
    aiui_http_recv_cb cb;
    void *cb_param;
    AIUI_SOCKET_HANDLE sock;
    int stop;
    aiui_http_response_t response;
    char *request_header;
} aiui_http_info_t;

static int _aiui_http_parse_url(
    const char *url, char **address, char **port, char **resource, bool *is_https)
{
    size_t address_len = 0;
    size_t port_len = 0;
    size_t resource_len = 0;
    char *cur_pos = NULL;
    char *address_end = NULL;
    char *port_end = NULL;
    if (NULL == url || 0 == strlen(url)) return -1;

    if (strstr((char *)url, "https") != NULL || strstr((char *)url, "wss") != NULL) {
        *is_https = true;
    } else {
        *is_https = false;
    }
    cur_pos = strstr((char *)url, "://");
    if (NULL != cur_pos) {
        cur_pos += 3;
    } else {
        cur_pos = (char *)url;
    }
    port_end = strchr(cur_pos, '/');
    if (NULL == port_end) {
        return -1;
    }
    address_end = strchr(cur_pos, ':');
    address_end = (NULL == address_end ? port_end : address_end);
    // 解析address
    address_len = (size_t)(address_end - cur_pos);
    *address = (char *)aiui_hal_malloc(address_len + 1);
    memcpy(*address, cur_pos, address_len);
    (*address)[address_len] = '\0';
    // 解析port
    if (address_end != port_end) {
        port_len = (size_t)(port_end - address_end - 1);
        *port = (char *)aiui_hal_malloc(port_len + 1);
        memcpy(*port, address_end + 1, port_len);
        (*port)[port_len] = '\0';
    } else {
        *port = NULL;
    }
    //解析resource
    resource_len = strlen(port_end);
    if (resource_len <= 0) return -1;
    *resource = (char *)aiui_hal_malloc(resource_len + 1);
    memcpy(*resource, port_end, resource_len);
    (*resource)[resource_len] = '\0';
    return 0;
}

static int _aiui_http_parse_header(aiui_http_info_t *handle, const char *header_info, int size)
{
    char const *tagStart = NULL;
    char const *tagEnd = NULL;

    if (NULL == header_info || size <= 0) return -1;
    char *header_end_pos = strstr((char *)header_info, "\r\n\r\n");
    if (NULL == header_end_pos) {
        LOGE(AIUI_TAG, "can not find http end flag");
        return -1;
    }
    tagStart = strstr(header_info, "Content-Length: ");
    if (tagStart != NULL) {
        char content_len[16] = {0};
        tagStart += strlen("Content-Length: ");
        tagEnd = strstr(tagStart, "\r\n");
        if (NULL == tagEnd) {
            return -1;
        }
        memcpy(content_len, tagStart, (size_t)(tagEnd - tagStart));
        content_len[tagEnd - tagStart] = 0;
        handle->response.contentLength = atoi(content_len);
        LOGD(AIUI_TAG, "contentLength: %d", handle->response.contentLength);
    }

    tagStart = strstr(header_info, "Transfer-Encoding: chunked");
    if (tagStart != NULL) {
        handle->response.is_chunked = 1;
        LOGD(AIUI_TAG, "is_chunked");
    }
    handle->response.is_header_completed = 1;
    return size - ((header_end_pos - header_info) + 4);
}

static int _aiui_http_parse_body(aiui_http_info_t *handle, char *buf, int len)
{
    if (NULL == handle || len <= 0) return -1;
    if (1 == handle->response.is_chunked) {
        if (NULL == handle->response.cur_body) {
            handle->response.cur_body = (char *)aiui_hal_malloc(len);
            handle->response.cur_body_len = len;
            memcpy(handle->response.cur_body, buf, len);
        } else {
            int tmp_len = handle->response.cur_body_len + len;
            char *tmp_body = (char *)aiui_hal_malloc(tmp_len);
            memcpy(tmp_body, handle->response.cur_body, handle->response.cur_body_len);
            memcpy(tmp_body + handle->response.cur_body_len, buf, len);
            aiui_hal_free(handle->response.cur_body);
            handle->response.cur_body = tmp_body;
            handle->response.cur_body_len = tmp_len;
        }
        int left = handle->response.cur_body_len;
        int passed = 0;
        int buf_offset = 0;
        memset(buf, 0, len);
        while (left > 0 && passed < len) {
            if (handle->response.expectedChunkSize > 0) {
                int save_size = left > handle->response.expectedChunkSize
                                    ? handle->response.expectedChunkSize
                                    : left;
                memcpy(buf + buf_offset, handle->response.cur_body + passed, save_size);
                handle->response.expectedChunkSize -= save_size;
                buf_offset += save_size;
                left -= save_size;
                passed += save_size;
                if (0 == handle->response.expectedChunkSize) handle->response.removecrlf = 1;
            } else {
                if (1 == handle->response.removecrlf) {
                    if (left < 2)
                        break;
                    else {
                        left -= 2;
                        passed += 2;
                        handle->response.removecrlf = 0;
                        if (left <= 0) break;
                    }
                }
                char *flag_pos = strstr(handle->response.cur_body + passed, "\r\n");
                if (NULL == flag_pos) {
                    LOGE(AIUI_TAG, "can not find flag");
                    break;
                }
                sscanf(handle->response.cur_body + passed,
                       "%x[^\r\n]",
                       &handle->response.expectedChunkSize);
                left -= (int)(flag_pos + 2 - (handle->response.cur_body + passed));
                passed += (int)(flag_pos + 2 - (handle->response.cur_body + passed));
                //结束
                if (0 == handle->response.expectedChunkSize) {
                    handle->response.is_over = 1;
                    break;
                }
            }
        }
        if (left > 0 && handle->response.is_over != 1) {
            char *tmp_body = (char *)aiui_hal_malloc(left);
            memcpy(tmp_body, handle->response.cur_body + passed, left);
            aiui_hal_free(handle->response.cur_body);
            handle->response.cur_body = tmp_body;
            handle->response.cur_body_len = left;
        } else {
            aiui_hal_free(handle->response.cur_body);
            handle->response.cur_body = NULL;
            handle->response.cur_body_len = 0;
        }
        return buf_offset;
    } else {
        handle->response.recv_body_len += len;
        if (handle->response.contentLength != 0 &&
            handle->response.recv_body_len >= handle->response.contentLength)
            handle->response.is_over = 1;
        return len;
    }
}

static int _aiui_http_recv(aiui_http_info_t *handle, char *buf, int len)
{
    if (0 == handle->response.is_header_completed) {
        int header_size = 0;
        char *header_info = (char *)aiui_hal_malloc(1024);
        memset(header_info, 0, 1024);
        int header_capacity = 1024;

        do {
            if (header_size >= header_capacity) {
                char *tmp = (char *)aiui_hal_malloc(header_size + 1024);
                memcpy(tmp, header_info, header_size);
                header_capacity += 1024;
                aiui_hal_free(header_info);
                header_info = tmp;
            }
            int recv_size = aiui_socket_recv(handle->sock, header_info + header_size, 1024);
            if (recv_size <= 0) break;
            header_size += recv_size;
        } while (strstr(header_info, "\r\n\r\n") == NULL);
        int left = _aiui_http_parse_header(handle, header_info, header_size);
        if (left < 0) {
            aiui_hal_free(header_info);
            return -1;
        };
        if (handle->cb != NULL) handle->cb(handle->cb_param, AIUI_HTTP_EVENT_CONNECTED, NULL, 0);
        if (left == 0) {
            memset(buf, 0, len);
            int size = aiui_socket_recv(handle->sock, buf, len);
            aiui_hal_free(header_info);
            return _aiui_http_parse_body(handle, buf, size);
        } else {
            memcpy(buf, header_info + (header_size - left), left);
            aiui_hal_free(header_info);
            return _aiui_http_parse_body(handle, buf, left);
        }
    } else {
        if (1 == handle->response.is_over) return 0;
        int size = aiui_socket_recv(handle->sock, buf, len);
        return _aiui_http_parse_body(handle, buf, size);
    }
}
static void *_aiui_http_recv_thread(void *param)
{
    if (NULL == param) return NULL;
    aiui_http_info_t *self = (aiui_http_info_t *)param;
    LOGD(AIUI_TAG, "http thread start %d", self->stop);
    while (0 == self->stop) {
        if (aiui_connect_is_ok(self->sock) != 0) {
            LOGD(AIUI_TAG, "http closed");
            break;
        }
        char buf[1024] = {0};
        int size = _aiui_http_recv(self, buf, 1024);
        if (size > 0) self->cb(self->cb_param, AIUI_HTTP_EVENT_MESSAGE, buf, size);
    }
    // 主动关闭时,才需要发送断开消息
    self->response.is_over = 1;
    if (0 == self->stop) {
        self->cb(self->cb_param, AIUI_HTTP_EVENT_DISCONNECTED, NULL, 0);
    }
    LOGD(AIUI_TAG, "http thread end");
    return NULL;
}

AIUI_HTTP_HANDLE aiui_http_create_request(
    const char *url, const char *type, aiui_http_recv_cb cb, void *param, const char *name)
{
    LOGD(AIUI_TAG, "create http request");
    if (NULL == type || NULL == url) {
        LOGE(AIUI_TAG, "type or url is empty");
        return NULL;
    }
    char *host = NULL;
    char *port = NULL;
    char *resource = NULL;
    bool is_https = false;
    LOGD(AIUI_TAG, "type = %s, url = %s", type, url);
    if (_aiui_http_parse_url(url, &host, &port, &resource, &is_https) != 0) {
        LOGE(AIUI_TAG, "url is invalid %s", url);
        return NULL;
    }
    aiui_http_info_t *handle = (aiui_http_info_t *)aiui_hal_malloc(sizeof(aiui_http_info_t));
    memset(handle, 0, sizeof(aiui_http_info_t));
    if (is_https) {
        const char *tmp_port = (NULL == port ? "443" : port);
        handle->sock = aiui_socket_connect(host, tmp_port, AIUI_SOCKET_SSL);

    } else {
        const char *tmp_port = (NULL == port ? "80" : port);
        handle->sock = aiui_socket_connect(host, tmp_port, AIUI_SOCKET_RAW);
    }
    if (NULL == handle->sock) {
        aiui_hal_free(host);
        if (port != NULL) aiui_hal_free(port);
        aiui_hal_free(resource);
        aiui_hal_free(handle);
        LOGE(AIUI_TAG, "http connect failed");
        return NULL;
    }
    // 认为额外添加的其他头信息不会超过1024字节
    int request_header_len = strlen("%s %s HTTP/1.1\r\nHost:%s\r\n") + strlen(type) +
                             strlen(resource) + strlen(host) + 1024;
    handle->request_header = (char *)aiui_hal_malloc(request_header_len);
    memset(handle->request_header, 0, request_header_len);
    snprintf(handle->request_header,
             request_header_len,
             "%s %s HTTP/1.1\r\nHost:%s\r\n",
             type,
             resource,
             host);
    handle->cb = cb;
    if (NULL != handle->cb) {
        handle->cb_param = param;
        void *tid = NULL;
        aiui_thread_param_t thread_param;
        thread_param.name = name;
        thread_param.deatch = 1;
        aiui_hal_thread_create(&tid, &thread_param, _aiui_http_recv_thread, handle);
    }
    aiui_hal_free(host);
    if (port != NULL) aiui_hal_free(port);
    aiui_hal_free(resource);

    return handle;
}

int aiui_http_recv(AIUI_HTTP_HANDLE handle, char *buf, int len)
{
    if (NULL == handle || NULL == buf || len <= 0) {
        LOGE(AIUI_TAG, "param error %p %p %d", handle, buf, len);
        return -1;
    }
    aiui_http_info_t *self = (aiui_http_info_t *)handle;
    if (NULL != self->cb) {
        LOGE(AIUI_TAG, "alread set callback, can not use this function");
        return -2;
    }
    return _aiui_http_recv(self, buf, len);
}

int aiui_http_add_header(AIUI_HTTP_HANDLE handle, const char *header_info)
{
    if (NULL == handle) {
        LOGE(AIUI_TAG, "http does not init");
        return -1;
    }
    aiui_http_info_t *self = (aiui_http_info_t *)handle;
    memcpy(self->request_header + strlen(self->request_header), header_info, strlen(header_info));
    memcpy(self->request_header + strlen(self->request_header), "\r\n", 2);
    return 0;
}

int aiui_http_send_request(AIUI_HTTP_HANDLE handle, const char *body)
{
    if (NULL == handle) {
        LOGE(AIUI_TAG, "http does not init");
        return -1;
    }
    aiui_http_info_t *self = (aiui_http_info_t *)handle;
    int ret = 0;
    if (NULL == body) {
        memcpy(self->request_header + strlen(self->request_header), "\r\n", 2);
        ret = aiui_socket_send(self->sock, self->request_header, strlen(self->request_header));
    } else {
        int body_len = strlen(body);
        char buf[32] = {0};
        snprintf(buf, 32, "Content-Length: %d\r\n\r\n", body_len);
        memcpy(self->request_header + strlen(self->request_header), buf, strlen(buf));
        int send_len = strlen(self->request_header) + body_len + 1;
        char *send_buf = (char *)aiui_hal_malloc(send_len);
        snprintf(send_buf, send_len, "%s%s", self->request_header, body);
        LOGD(AIUI_TAG, "send data = %s", send_buf);
        ret = aiui_socket_send(self->sock, send_buf, send_len);
        aiui_hal_free(send_buf);
    }
    return (ret > 0) ? 0 : -1;
}

int aiui_http_send_msg(AIUI_HTTP_HANDLE handle, const char *msg, const int len)
{
    if (NULL == handle) {
        LOGE(AIUI_TAG, "http does not init");
        return -1;
    }
    aiui_http_info_t *self = (aiui_http_info_t *)handle;
    return aiui_socket_send(self->sock, msg, len);
}

void aiui_http_destroy_request(AIUI_HTTP_HANDLE handle)
{
    LOGD(AIUI_TAG, "destroy http request start");
    if (NULL == handle) return;
    aiui_http_info_t *self = (aiui_http_info_t *)handle;
    self->stop = 1;
    if (NULL != self->sock) {
        aiui_socket_close(self->sock);
    }
    while (!self->response.is_over && self->cb != NULL) {
        aiui_hal_sleep(10);
    }
    aiui_socket_destroy(self->sock);
    self->sock = NULL;
    if (self->request_header != NULL) {
        aiui_hal_free(self->request_header);
        self->request_header = NULL;
    }
    if (self->response.cur_body != NULL) {
        aiui_hal_free(self->response.cur_body);
        self->response.cur_body = NULL;
    }
    aiui_hal_free(self);
    LOGD(AIUI_TAG, "destroy http request end");
}
