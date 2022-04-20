#ifndef __AIUI_URL_DOWNLOAD_H__
#define __AIUI_URL_DOWNLOAD_H__

typedef enum {
    AIUI_URL_DOWNLOAD_BEGIN = 0,
    AIUI_URL_DOWNLOAD_END,
    AIUI_URL_DOWNLOAD_DATA,

    AIUI_URL_DOWNLOAD_MAX
} AIUI_URL_DOWNLOAD_TYPE;

typedef void (*aiui_url_download_recv_cb)(void *param,
                                          const AIUI_URL_DOWNLOAD_TYPE type,
                                          void *buf,
                                          const int len);

typedef void *AIUI_URL_DOWNLOAD_HANDLE;

AIUI_URL_DOWNLOAD_HANDLE aiui_url_download_start(const char *url,
                                                 aiui_url_download_recv_cb cb,
                                                 int offset,
                                                 void *priv);

void aiui_url_download_stop(AIUI_URL_DOWNLOAD_HANDLE handle);

#endif