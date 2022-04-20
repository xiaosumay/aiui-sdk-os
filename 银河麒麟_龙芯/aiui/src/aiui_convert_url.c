#include <stdbool.h>
#include <string.h>
#include "aiui_wrappers_os.h"
#include "aiui_log.h"
#include "time.h"
#include "md5.h"
#include "cjson.h"
#include "aiui_http_api.h"
#include "aiui_socket_api.h"
#include "aiui_device_info.h"
#include "md5.h"
#include "aiui_convert_url.h"

#define AIUI_TAG                 "aiui_convert"
#define AIUI_KW_HOST             "https://adf.xfyun.cn"
#define AIUI_KW_ACTIVE_RESOURCE  "/kuwo/active"
#define AIUI_KW_CONVERT_RESOURCE "/kuwo/tranklink"
static bool s_is_active = false;
static const char *s_bitrate[] = {"24kaac", "128kmp3"};

static int _aiui_parse_active(const char *data)
{
    if (NULL == data) return -1;

    LOGD(AIUI_TAG, "active recv body: %s\n", data);
    cJSON *result = cJSON_Parse(data);
    if (cJSON_IsInvalid(result)) {
        LOGD(AIUI_TAG, "invalid json format");
        return -1;
    }
    cJSON *code = cJSON_GetObjectItem(result, "code");
    if (code != NULL && code->valueint == 200) {
        LOGD(AIUI_TAG, "active succeed");
        cJSON_Delete(result);
        return 0;
    }
    cJSON_Delete(result);
    LOGD(AIUI_TAG, "active failed");
    return -1;
}

static const char *_aiui_parse_url_data(char *data)
{
    if (NULL == data) return NULL;
    LOGD(AIUI_TAG, "parse: %s\n", data);
    cJSON *result = cJSON_Parse(data);
    if (NULL == result) {
        return NULL;
    }
    if (!cJSON_IsInvalid(result)) {
        cJSON *code = cJSON_GetObjectItem(result, "code");
        if (code != NULL && code->valueint != 200) {
            cJSON_Delete(result);
            LOGE(AIUI_TAG, "parse url failed");
            return NULL;
        }
        cJSON *data = cJSON_GetObjectItem(result, "data");
        if (NULL == data) {
            cJSON_Delete(result);
            LOGE(AIUI_TAG, "parse url failed");
            return NULL;
        }
        cJSON *slot_data = cJSON_GetArrayItem(data, 0);
        if (slot_data != NULL) {
            cJSON *slots = cJSON_GetObjectItem(slot_data, "audiopath");
            int url_len = strlen(slots->valuestring);
            if (slots != NULL && url_len != 0) {
                char *url = (char *)aiui_hal_malloc(url_len + 1);
                memset(url, 0, url_len + 1);
                memcpy(url, slots->valuestring, url_len);
                cJSON_Delete(result);
                return url;
            }
        }
    }
    cJSON_Delete(result);
    return NULL;
}

char *_aiui_format_token(const char *appid, const char *key, int cur_timestamp)
{
    // 将appId、appKey和timestamp进行字符串拼接，然后md5加密。md5(appId+appKey+timestamp)
    char token[256] = {0};
    snprintf(token, 256, "%s%s%d", appid, key, cur_timestamp);
    unsigned char output[16] = {0};
    mbedtls_md5_ret((const unsigned char *)token, strlen(token), output);
    // 将byte数据转换成HEX字符串
    char *resultStr = (char *)aiui_hal_malloc(33);
    memset(resultStr, '\0', 33);
    for (int i = 0; i < 16; ++i) {
        sprintf(resultStr + i * 2, "%02x", output[i]);
    }
    return resultStr;
}

int aiui_active()
{
    if (s_is_active) return 0;
    LOGD(AIUI_TAG, "%s %s %s", AIUI_APP_ID, AIUI_APP_KEY, AIUI_AUTH_ID);
    if (strlen(AIUI_APP_ID) == 0 || strlen(AIUI_APP_KEY) == 0 || strlen(AIUI_AUTH_ID) == 0) {
        LOGE(AIUI_TAG, "appid or appkey or deviceID is NULL");
        return -1;
    }
    int cur_timestamp = time(NULL);
    char *token = _aiui_format_token(AIUI_APP_ID, AIUI_APP_KEY, cur_timestamp);
    AIUI_HTTP_HANDLE handle =
        aiui_http_create_request(AIUI_KW_HOST AIUI_KW_ACTIVE_RESOURCE, "POST", NULL, NULL, NULL);
    aiui_http_add_header(handle, "Content-Type: application/x-www-form-urlencoded");
    aiui_http_add_header(handle, "Cache-Control: no-cache");
    aiui_http_add_header(handle, "Connection: close");
    char body[256] = {0};
    snprintf(body,
             256,
             "appId=%s&token=%s&timestamp=%d&serialNumber=%s",
             AIUI_APP_ID,
             token,
             cur_timestamp,
             AIUI_AUTH_ID);
    aiui_http_send_request(handle, body);

    char *recv = (char *)aiui_hal_malloc(1024);
    memset(recv, 0, 1024);
    aiui_http_recv(handle, recv, 1024);
    aiui_hal_free(token);
    aiui_http_destroy_request(handle);
    _aiui_parse_active(recv);
    aiui_hal_free(recv);
    return 0;
}

static const char *_aiui_get_url(const char *appid,
                                 const char *token,
                                 const char *serial_num,
                                 const int timestamp,
                                 const char *itemID,
                                 const int rate)
{
    char body[256] = {0};
    snprintf(body,
             256,
             "appId=%s&token=%s&timestamp=%d&serialNumber=%s&itemid=%s&format=%s",
             appid,
             token,
             timestamp,
             serial_num,
             itemID,
             s_bitrate[rate]);
    AIUI_HTTP_HANDLE handle =
        aiui_http_create_request(AIUI_KW_HOST AIUI_KW_CONVERT_RESOURCE, "POST", NULL, NULL, NULL);
    LOGD(AIUI_TAG, "handle = %p", handle);
    aiui_http_add_header(handle, "Content-Type: application/x-www-form-urlencoded");
    aiui_http_add_header(handle, "Cache-Control: no-cache");
    aiui_http_add_header(handle, "Connection: close");
    aiui_http_send_request(handle, body);
    char *recv = (char *)aiui_hal_malloc(1024);
    memset(recv, 0, 1024);
    aiui_http_recv(handle, recv, 1024);
    aiui_http_destroy_request(handle);
    const char *url = _aiui_parse_url_data(recv);
    aiui_hal_free(recv);
    return url;
}

const char *aiui_convert_url(const char *itemID, const int rate)
{
    if (strlen(AIUI_APP_ID) == 0 || strlen(AIUI_APP_KEY) == 0 || strlen(AIUI_AUTH_ID) == 0) {
        LOGE(AIUI_TAG, "appid or appkey or deviceID is NULL");
        return NULL;
    }
    int cur_timestamp = time(NULL);
    char *token = _aiui_format_token(AIUI_APP_ID, AIUI_APP_KEY, cur_timestamp);
    LOGD(AIUI_TAG, "token %s", token);
    if (NULL == token) {
        return NULL;
    }
    const char *url = _aiui_get_url(AIUI_APP_ID, token, AIUI_AUTH_ID, cur_timestamp, itemID, rate);

    aiui_hal_free(token);
    LOGD(AIUI_TAG, "url: %s", url);
    return url;
}
