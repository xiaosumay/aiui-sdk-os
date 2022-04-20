#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "aiui_log.h"
#include "aiui_wrappers_os.h"
#include "base64.h"
#include "md5.h"
#include "aiui_socket_api.h"
#include "aiui_device_info.h"
#include "cjson.h"
#include "aiui_upload_dynamic_entity.h"

#define AIUI_TAG "aiui_dynamic"
static const char *s_http_format =
    "POST %s HTTP/1.1\r\n"
    "Content-Type: application/x-www-form-urlencoded; charset=utf-8\r\n"
    "X-NameSpace: %s\r\n"
    "X-Nonce: %d\r\n"
    "X-CurTime: %d\r\n"
    "X-CheckSum: %s\r\n"
    "Host: openapi.xfyun.cn\r\n";

static const char *s_url[2] = {"/v2/aiui/entity/upload-resource", "/v2/aiui/entity/check-resource"};
static const char *s_dynamic_level[3] = {
    "appid",
    "auth_id",
    "rest"    // TODO 根据自己定义的技能修改(我的动态实体-资源维度里)
};

static void _aiui_parse(const char *request)
{
    const char *cur_pos = NULL;
    cJSON *result = NULL;
    if (NULL == request) return;
    cur_pos = request;
    while (1) {
        char *pos = strstr((char *)cur_pos, "\r\n");
        if (NULL == pos) break;
        cur_pos = pos + 2;
    }
    result = cJSON_Parse(cur_pos);
    if (NULL == result) {
        LOGE(AIUI_TAG, "_aiui_parse result empty");
        return;
    }
    if (!cJSON_IsInvalid(result)) {
        cJSON *data = NULL;
        cJSON *code = cJSON_GetObjectItem(result, "code");
        if (code != NULL && code->valuestring != NULL) {
            LOGD(AIUI_TAG, "_aiui_parse code = %s\n", code->valuestring);
        }
        data = cJSON_GetObjectItem(result, "data");
        if (data != NULL) {
            cJSON *sid = cJSON_GetObjectItem(data, "sid");
            if (sid != NULL && sid->valuestring != NULL)
                LOGD(AIUI_TAG, "_aiui_parse sid = %s\n", sid->valuestring);
        }
    }
    cJSON_Delete(result);
}

static char *_aiui_header_format(int type)
{
    int i = 0;
    int nonce = 0;
    char check_sum[1024] = {0};
    unsigned char md5_output[16] = {0};
    char check_sum_md5[33] = {0};
    char *header_format = NULL;
    int cur_timestamp = 0;
    int header_len = 0;
    cur_timestamp = time(NULL);
    srand(cur_timestamp);
    nonce = rand();
    snprintf(check_sum, 1023, "%s%d%d", AIUI_ACCOUNT_KEY, nonce, cur_timestamp);
    mbedtls_md5_ret((const unsigned char *)check_sum, strlen(check_sum), md5_output);
    // 将byte数据转换成HEX字符串
    for (i = 0; i < 16; ++i) {
        sprintf(check_sum_md5 + i * 2, "%02x", md5_output[i]);
    }
    header_len = strlen(s_http_format) + strlen(check_sum) + strlen(AIUI_NAMESPACE) +
                 strlen(s_url[type]) + strlen(check_sum_md5);
    header_format = (char *)aiui_hal_malloc(header_len);
    memset(header_format, 0, header_len);
    snprintf(header_format,
             header_len,
             s_http_format,
             s_url[type],
             AIUI_NAMESPACE,
             nonce,
             cur_timestamp,
             check_sum_md5);
    return header_format;
}

int aiui_upload_dynamic_entity(const char *param,
                               const char *dynamic_name,
                               const AIUI_DYNAMIC_LEVEL_T level,
                               const char *id)
{
    AIUI_SOCKET_HANDLE sock = NULL;
    int base64_len = 0;
    char *base64_data = NULL;
    int body_len = 0;
    int post_len = 0;
    char *body_data = NULL;
    char *post_data = NULL;
    char recv_buf[1024] = {0};
    char *header_format = NULL;
    if ((sock = aiui_socket_connect("https://openapi.xfyun.cn", "443", AIUI_SOCKET_SSL)) == NULL) {
        LOGE(AIUI_TAG, "connect error");
        return -1;
    }
    base64_len = (strlen(param) + 2) / 3 * 4 + 4;
    base64_data = (char *)aiui_hal_malloc(base64_len);
    memset(base64_data, 0, base64_len);
    size_t write_len = 0;
    if (mbedtls_base64_encode((unsigned char *)base64_data,
                              base64_len,
                              &write_len,
                              (unsigned char *)param,
                              strlen(param)) != 0) {
        LOGE(AIUI_TAG, "connect error");
        aiui_hal_free(base64_data);
        base64_data = NULL;
        return -1;
    }
    body_len = base64_len + 128;
    body_data = (char *)aiui_hal_malloc(body_len);
    snprintf(body_data,
             body_len,
             "appid=%s&res_name=%s&pers_param={\"%s\":\"%s\"}&data=%s",
             AIUI_APP_ID,
             dynamic_name,
             s_dynamic_level[level],
             id,
             base64_data);
    header_format = _aiui_header_format(0);
    post_len =
        strlen(header_format) + strlen(body_data) + strlen("%sContent-Length: %d\r\n\r\n%s") + 1;
    post_data = (char *)aiui_hal_malloc(post_len);
    snprintf(post_data,
             post_len,
             "%sContent-Length: %lu\r\n\r\n%s",
             header_format,
             strlen(body_data),
             body_data);

    aiui_socket_send(sock, post_data, strlen(post_data));
    aiui_hal_free(base64_data);
    aiui_hal_free(header_format);
    aiui_hal_free(body_data);
    aiui_hal_free(post_data);
    base64_data = NULL;
    body_data = NULL;
    post_data = NULL;
    if (aiui_socket_recv(sock, recv_buf, 1024) > 0) _aiui_parse(recv_buf);
    aiui_socket_close(sock);
    aiui_socket_destroy(sock);
    return 0;
}

int aiui_check_dynamic_entity(const char *sid)
{
    AIUI_SOCKET_HANDLE sock = NULL;
    char body_data[1024] = {0};
    char post_data[1024] = {0};
    char recv_buf[1024] = {0};
    char *header_format = NULL;
    if ((sock = aiui_socket_connect("https://openapi.xfyun.cn", "443", AIUI_SOCKET_SSL)) == NULL) {
        LOGE(AIUI_TAG, "connect error\n");
        return -1;
    }
    snprintf(body_data, 1023, "sid=%s", sid);
    header_format = _aiui_header_format(1);
    snprintf(post_data,
             1023,
             "%sContent-Length: %lu\r\n\r\n%s",
             header_format,
             strlen(body_data),
             body_data);
    aiui_socket_send(sock, post_data, strlen(post_data));
    aiui_hal_free(header_format);
    header_format = NULL;
    if (aiui_socket_recv(sock, recv_buf, 1024) > 0) _aiui_parse(recv_buf);
    aiui_socket_close(sock);
    aiui_socket_destroy(sock);
    return 0;
}
