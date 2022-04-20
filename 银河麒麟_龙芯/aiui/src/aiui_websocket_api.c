#include <string.h>
#include <stdlib.h>
#include "base64.h"
#include "aiui_log.h"
#include "aiui_wrappers_os.h"
#include "aiui_http_api.h"
#include "aiui_websocket_api.h"

#define AIUI_TAG "aiui_websocket"

#define AIUI_FRAME_CHUNK_LENGTH 1024

// websocket根据data[0]判别数据包类型    比如0x81 = 0x80 | 0x1 为一个txt类型数据包
typedef enum {
    AIUI_WS_SEQ = 0x00,
    AIUI_WS_TXTDATA = 0x01,    // 0x1：标识一个txt类型数据包
    AIUI_WS_BINDATA = 0x02,    // 0x2：标识一个bin类型数据包
    AIUI_WS_DISCONN = 0x08,    // 0x8：标识一个断开连接类型数据包
    AIUI_WS_PING = 0x09,       // 0x8：标识一个断开连接类型数据包
    AIUI_WS_PONG = 0x0a,       // 0xA：表示一个pong类型数据包
    AIUI_WS_FIN = 0x80,        //fin
    AIUI_WS_END = 0x10,
    AIUI_WS_CLOSE_OK = 0xaa,
    AIUI_WS_INIT = 0xff,
} AIUI_WS_Type;

typedef struct aiui_websocket_frame_s
{
    int header_completed;
    unsigned int fin;
    unsigned int opcode;
    unsigned int header_len;
    unsigned int payload_len;
    char *rawdata;
    unsigned int rawdata_size;
    unsigned int capacity;
    struct aiui_websocket_frame_s *prev_frame;
} aiui_websocket_frame_t;

typedef struct aiui_websocket_info_s
{
    AIUI_HTTP_HANDLE http;
    aiui_websocket_cb_t cb;
    void *cb_param;
    aiui_websocket_frame_t *frame;
    char websocket_name[32];
} aiui_websocket_info_t;

static char *_aiui_websocket_build_shake_key()
{
    srand(time(NULL));
    unsigned char key_nonce[17] = {0};
    for (int i = 0; i < 16; i++) {
        key_nonce[i] = (unsigned char)(rand() & 0xff);
        if (0 == key_nonce[i]) key_nonce[i] = 128;    // 随机数不取0，可能会影响字符串长度判断
    }
    char *shake_key = (char *)aiui_hal_malloc((16 + 2) * 4 / 3 + 1);
    size_t write_len = 0;
    mbedtls_base64_encode((unsigned char *)shake_key,
                          128,
                          &write_len,
                          (unsigned char *)key_nonce,
                          strlen((const char *)key_nonce));
    return shake_key;
}

static void _aiui_cleanup_frames(aiui_websocket_info_t *websocket)
{
    if (NULL == websocket) return;
    aiui_websocket_frame_t *cur = websocket->frame;
    aiui_websocket_frame_t *prev = NULL;
    while (cur != NULL) {
        if (cur->rawdata != NULL) aiui_hal_free(cur->rawdata);
        prev = cur->prev_frame;
        aiui_hal_free(cur);
        cur = prev;
    }
    websocket->frame = NULL;
}

static int _aiui_is_complete_frame(aiui_websocket_frame_t *frame)
{
    if (frame->rawdata_size < 2) {
        return -1;
    }
    if (0 == frame->header_completed) {
        frame->fin = (frame->rawdata[0] & 0x80) == 0x80 ? 1 : 0;
        frame->opcode = frame->rawdata[0] & 0x0f;
        frame->header_len = 2;
        frame->payload_len = frame->rawdata[1] & 0x7f;
        switch (frame->payload_len) {
            case 126:
                if (frame->rawdata_size < 4) {
                    return -1;
                }
                for (int i = 0; i < 2; i++) {
                    memcpy((char *)&frame->payload_len + i, frame->rawdata + 3 - i, 1);
                }
                frame->header_len += 2;
                break;
            case 127:
                if (frame->rawdata_size < 10) {
                    return -1;
                }
                for (int i = 0; i < 8; i++) {
                    memcpy((char *)&frame->payload_len + i, frame->rawdata + 9 - i, 1);
                }
                frame->header_len += 8;
                break;
            default:
                break;
        }
        //LOGD(AIUI_TAG, "fin = %d, opcode = %d, header_len = %d, payload_len = %d", frame->fin, frame->opcode, frame->header_len, frame->payload_len);
        frame->header_completed = 1;
    }
    if (frame->rawdata_size < frame->header_len + frame->payload_len) {
        return -1;
    }
    return 0;
}

static void _aiui_dispatch_frame(aiui_websocket_info_t *websocket)
{
    aiui_websocket_frame_t *frame = websocket->frame;
    unsigned int msg_len = frame->payload_len;
    for (; frame->prev_frame != NULL; frame = frame->prev_frame) {
        msg_len += frame->payload_len;
        LOGD(AIUI_TAG, "msg_len=%d", msg_len);
    }
    frame = websocket->frame;
    char *msg = (char *)aiui_hal_malloc(msg_len + 1);    // 补"\0"
    memset(msg, 0, msg_len + 1);
    unsigned int offset = msg_len;
    for (; frame != NULL; frame = frame->prev_frame) {
        offset -= frame->payload_len;
        memcpy(msg + offset, frame->rawdata + frame->header_len, frame->payload_len);
    }

    websocket->cb.message(websocket->cb_param, msg, msg_len);
    aiui_hal_free(msg);
}

static void _aiui_websocket_parse_frame(aiui_websocket_info_t *websocket, char ch)
{
    if (NULL == websocket->frame) {
        websocket->frame =
            (aiui_websocket_frame_t *)aiui_hal_malloc(sizeof(aiui_websocket_frame_t));
        memset(websocket->frame, 0, sizeof(aiui_websocket_frame_t));
        websocket->frame->payload_len = -1;
        websocket->frame->capacity = AIUI_FRAME_CHUNK_LENGTH;
        websocket->frame->rawdata = (char *)aiui_hal_malloc(websocket->frame->capacity);
        memset(websocket->frame->rawdata, 0, websocket->frame->capacity);
    }
    // 数据超过 frame->capacity 重新分配最大容量
    if (websocket->frame->rawdata_size >= websocket->frame->capacity) {
        char *tmp_data =
            (char *)aiui_hal_malloc(websocket->frame->capacity + AIUI_FRAME_CHUNK_LENGTH);
        memcpy(tmp_data, websocket->frame->rawdata, websocket->frame->rawdata_size);
        aiui_hal_free(websocket->frame->rawdata);
        websocket->frame->rawdata = tmp_data;
        websocket->frame->capacity += AIUI_FRAME_CHUNK_LENGTH;
        memset(websocket->frame->rawdata + websocket->frame->rawdata_size,
               0,
               websocket->frame->capacity - websocket->frame->rawdata_size);
    }
    websocket->frame->rawdata[websocket->frame->rawdata_size++] = ch;
    if (_aiui_is_complete_frame(websocket->frame) == 0) {
        if (websocket->frame->fin == 1) {
            _aiui_dispatch_frame(websocket);
            _aiui_cleanup_frames(websocket);
        }
        // 一帧数据服务端分包发送
        else {
            aiui_websocket_frame_t *new_frame =
                (aiui_websocket_frame_t *)aiui_hal_malloc(sizeof(aiui_websocket_frame_t));
            memset(new_frame, 0, sizeof(aiui_websocket_frame_t));
            new_frame->payload_len = -1;
            new_frame->capacity = AIUI_FRAME_CHUNK_LENGTH;
            new_frame->rawdata = (char *)aiui_hal_malloc(new_frame->capacity);
            memset(new_frame->rawdata, 0, new_frame->capacity);
            new_frame->prev_frame = websocket->frame;
            websocket->frame = new_frame;
        }
    }
}

static int _aiui_websocket_recv_cb(const void *param,
                                   AIUI_HTTP_EVENT_TYPE type,
                                   void *buf,
                                   const int len)
{
    if (NULL == param) return -1;
    aiui_websocket_info_t *self = (aiui_websocket_info_t *)param;
    //LOGD(AIUI_TAG, "type = %d, len = %d, buf = %s", type, len, buf);
    switch (type) {
        case AIUI_HTTP_EVENT_DISCONNECTED:
            self->cb.disconnected(self->cb_param);
            break;
        case AIUI_HTTP_EVENT_CONNECTED:
            break;
        case AIUI_HTTP_EVENT_MESSAGE:
            for (int i = 0; i < len; i++) {
                _aiui_websocket_parse_frame(self, ((char *)buf)[i]);
            }
            break;
        default:
            break;
    }
    return 0;
}

static int _aiui_websocket_send(aiui_websocket_info_t *websocket,
                                const char *buf,
                                const unsigned int len,
                                AIUI_WS_Type type)
{
    unsigned char mask[4] = {0};
    unsigned int mask_int = 0;
    unsigned int payload_len_small = 0;
    unsigned int header_len = 6;
    unsigned int extend_size = 0;
    unsigned int frame_size = 0;

    srand(time(NULL));
    mask_int = rand();
    memcpy(mask, &mask_int, 4);
    if (len <= 125) {
        frame_size = 6 + len;
        payload_len_small = len;
        extend_size = 0;
    } else if (len <= 0xffff) {
        frame_size = 8 + len;
        payload_len_small = 126;
        header_len += 2;
        extend_size = 2;
    } else if (len <= 0xffffffffffffffffLL) {
        frame_size = 14 + len;
        payload_len_small = 127;
        header_len += 8;
        extend_size = 8;
    } else
        return -1;
    char *data = (char *)aiui_hal_malloc(frame_size);

    memset(data, 0, frame_size);
    *data = type | 0x80;
    *(data + 1) = payload_len_small | 0x80;    // payload length with mask bit on
    for (size_t i = 0; i < extend_size; i++) {
        *(data + 2 + i) = *((char *)&len + (extend_size - i - 1));    // big endian
    }
    for (int i = 0; i < 4; i++) {
        *(data + (header_len - 4) + i) = mask[i] & 0xff;
    }

    memcpy(data + header_len, buf, len);
    for (unsigned int i = 0; i < len; i++) {
        *(data + header_len + i) ^= mask[i % 4] & 0xff;
    }

    int ret = aiui_http_send_msg(websocket->http, data, frame_size);
    aiui_hal_free(data);
    if (ret > 0)
        return len;
    else
        return -1;
}

AIUI_WEBSOCKET_HANDLE aiui_websocket_create(aiui_websocket_cb_t cb, void *param)
{
    aiui_websocket_info_t *self =
        (aiui_websocket_info_t *)aiui_hal_malloc(sizeof(aiui_websocket_info_t));
    memset(self, 0, sizeof(aiui_websocket_info_t));
    self->cb = cb;
    self->cb_param = param;
    snprintf(self->websocket_name, 32, "aiui_websocket_%p", self);
    return (AIUI_WEBSOCKET_HANDLE)self;
}

int aiui_websocket_connect(AIUI_WEBSOCKET_HANDLE handle, const char *url, const char *origin_host)
{
    if (NULL == handle) return -1;
    aiui_websocket_info_t *self = (aiui_websocket_info_t *)handle;
    int http_url_len = strlen(url) + strlen(origin_host) + 1;
    char *http_url = (char *)aiui_hal_malloc(http_url_len);
    snprintf(http_url, http_url_len, "%s%s", origin_host, url);
    self->http = aiui_http_create_request(
        http_url, "GET", _aiui_websocket_recv_cb, handle, self->websocket_name);
    aiui_hal_free(http_url);
    if (NULL == self->http) return -2;
    aiui_http_add_header(self->http, "Upgrade: websocket");
    aiui_http_add_header(self->http, "Connection: Upgrade");

    int origin_len = strlen("Origin: ") + strlen(origin_host) + 1;
    char *origin = (char *)aiui_hal_malloc(origin_len);
    snprintf(origin, origin_len, "Origin: %s", origin_host);
    aiui_http_add_header(self->http, origin);
    aiui_hal_free(origin);

    char *shake_key = _aiui_websocket_build_shake_key();
    int sek_key_len = strlen("Sec-WebSocket-Key: ") + strlen(shake_key) + 1;
    char *sec_key = (char *)aiui_hal_malloc(sek_key_len);
    snprintf(sec_key, sek_key_len, "Sec-WebSocket-Key: %s", shake_key);
    aiui_http_add_header(self->http, sec_key);
    aiui_hal_free(sec_key);
    aiui_hal_free(shake_key);

    aiui_http_add_header(self->http, "Sec-WebSocket-Version: 13");
    aiui_http_send_request(self->http, NULL);
    return 0;
}

int aiui_websocket_disconect(AIUI_WEBSOCKET_HANDLE handle)
{
    if (NULL == handle) return -1;
    aiui_websocket_info_t *self = (aiui_websocket_info_t *)handle;
    aiui_http_destroy_request(self->http);
    _aiui_cleanup_frames(self);
    self->http = NULL;
    return 0;
}

int aiui_websocket_send_bin(AIUI_WEBSOCKET_HANDLE handle, const char *msg, int len)
{
    if (NULL == handle) return -1;
    aiui_websocket_info_t *self = (aiui_websocket_info_t *)handle;
    return _aiui_websocket_send(self, msg, len, AIUI_WS_BINDATA);
}

int aiui_websocket_send_text(AIUI_WEBSOCKET_HANDLE handle, const char *msg, int len)
{
    if (NULL == handle) return -1;
    aiui_websocket_info_t *self = (aiui_websocket_info_t *)handle;
    return _aiui_websocket_send(self, msg, len, AIUI_WS_TXTDATA);
}

void aiui_websocket_destroy(AIUI_WEBSOCKET_HANDLE handle)
{
    aiui_websocket_disconect(handle);
    aiui_hal_free(handle);
}
