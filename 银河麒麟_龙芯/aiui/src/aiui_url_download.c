#include <string.h>
#include "aiui_wrappers_os.h"
#include "aiui_http_api.h"
#include <stdio.h>
#include "aiui_url_download.h"

typedef struct aiui_url_download_s
{
    aiui_url_download_recv_cb cb;
    void *cb_param;
    AIUI_HTTP_HANDLE http_handle;
} aiui_url_download_t;

static int _aiui_url_download_recv_cb(const void *param,
                                      AIUI_HTTP_EVENT_TYPE type,
                                      void *buf,
                                      const int len)
{
    if (NULL == param) return -1;
    aiui_url_download_t *handle = (aiui_url_download_t *)param;
    switch (type) {
        case AIUI_HTTP_EVENT_DISCONNECTED:
            handle->cb(handle->cb_param, AIUI_URL_DOWNLOAD_END, NULL, 0);
            break;
        case AIUI_HTTP_EVENT_CONNECTED:
            handle->cb(handle->cb_param, AIUI_URL_DOWNLOAD_BEGIN, NULL, 0);
            break;
        case AIUI_HTTP_EVENT_MESSAGE:
            handle->cb(handle->cb_param, AIUI_URL_DOWNLOAD_DATA, buf, len);
            break;
        default:
            break;
    }
    return 0;
}

AIUI_URL_DOWNLOAD_HANDLE aiui_url_download_start(const char *url,
                                                 aiui_url_download_recv_cb cb,
                                                 int offset,
                                                 void *priv)
{
    aiui_url_download_t *handle =
        (aiui_url_download_t *)aiui_hal_malloc(sizeof(aiui_url_download_t));
    memset(handle, 0, sizeof(aiui_url_download_t));
    handle->cb = cb;
    handle->cb_param = priv;
    handle->http_handle = aiui_http_create_request(
        url, "GET", _aiui_url_download_recv_cb, handle, "aiui_url_download");
    if (NULL == handle->http_handle) {
        aiui_hal_free(handle);
        return NULL;
    }
    aiui_http_add_header(handle->http_handle, "Accept-Encoding: none");
    aiui_http_add_header(handle->http_handle, "Cache-Control: no-cache");
    char range[32] = {0};
    snprintf(range, 32, "Range: bytes=%d-", offset);
    aiui_http_add_header(handle->http_handle, range);
    aiui_http_add_header(handle->http_handle, "Connection: close");
    aiui_http_send_request(handle->http_handle, NULL);
    return (void *)handle;
}

void aiui_url_download_stop(AIUI_URL_DOWNLOAD_HANDLE handle)
{
    if (NULL != handle) {
        aiui_url_download_t *self = (aiui_url_download_t *)handle;
        aiui_http_destroy_request(self->http_handle);
        aiui_hal_free(handle);
    }
}