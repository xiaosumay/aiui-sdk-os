#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "md5.h"
#include "cjson.h"
#include "aiui_log.h"
#include "aiui_wrappers_os.h"
#include "aiui_http_api.h"
#include "aiui_search_weather.h"

#define AIUI_TAG          "aiui_weather"
#define AIUI_WEATHER_HOST "https://content.xfyun.cn"
#define AIUI_WEATHER_RESOURCE \
    "/v1/weather/query?&appId=%s&deviceId=%s&token=%s&timestamp=%lld&city=%s"

static void _aiui_parse_weather(const char *data)
{
    if (NULL == data) return;

    LOGD(AIUI_TAG, "recv body: %s", data);
    cJSON *root = cJSON_Parse(data);
    if (cJSON_IsInvalid(root)) {
        LOGD(AIUI_TAG, "invalid json format");
        return;
    }
    cJSON *slots_result = cJSON_GetObjectItem(root, "result");
    if (cJSON_IsInvalid(root)) {
        LOGD(AIUI_TAG, "get upload failed");
        cJSON_Delete(root);
        return;
    }
    cJSON *result_0 = cJSON_GetArrayItem(slots_result, 0);
    cJSON *airdata = cJSON_GetObjectItem(result_0, "aqi");          // 空气指数
    cJSON *humidity = cJSON_GetObjectItem(result_0, "humidity");    //空气湿度
    cJSON *temp = cJSON_GetObjectItem(result_0, "temperature");     // 当前温度
    if (NULL == airdata || NULL == humidity || NULL == temp) return;
    LOGD(AIUI_TAG,
         "airdata=%s, humidity=%s,temp=%s",
         airdata->valuestring,
         humidity->valuestring,
         temp->valuestring);
    cJSON_Delete(root);
    return;
}

int aiui_search_weather(const char *appid, const char *apikey, const char *deviceid)
{
    char auth_format[128] = {0};
    long long time_stamp = time(NULL) * 1000;
    char checksum_md5[16] = {0};
    char checksum_md5_hex[33] = {0};
    char url[256] = {0};
    snprintf(auth_format, 128, "%s%s%lld", appid, apikey, time_stamp);
    mbedtls_md5_ret(
        (unsigned char *)auth_format, strlen(auth_format), (unsigned char *)checksum_md5);
    for (int i = 0; i < 16; ++i) {
        sprintf(checksum_md5_hex + i * 2, "%02x", (unsigned char)checksum_md5[i]);
    }
    snprintf(url,
             256,
             AIUI_WEATHER_HOST AIUI_WEATHER_RESOURCE,
             appid,
             deviceid,
             checksum_md5_hex,
             time_stamp,
             "布拉格");
    AIUI_HTTP_HANDLE handle = aiui_http_create_request(url, "GET", NULL, NULL, NULL);
    aiui_http_send_request(handle, NULL);
    char recv[1024] = {0};
    char *result = NULL;
    int len = 0;
    while (1) {
        int size = aiui_http_recv(handle, recv, 1024);
        //LOGD(AIUI_TAG, "size = %d", size);
        if (size <= 0) break;
        if (NULL == result) {
            len = size + 1;
            result = (char *)aiui_hal_malloc(len);
            memcpy(result, recv, size);
        } else {
            int offset = len - 1;
            len += size;
            char *tmp = (char *)aiui_hal_malloc(len);
            memcpy(tmp, result, offset);
            memcpy(tmp + offset, recv, size);
            aiui_hal_free((void *)result);
            result = tmp;
        }
    }
    result[len] = '\0';
    _aiui_parse_weather(result);
    aiui_hal_free((void *)result);
    aiui_http_destroy_request(handle);
    return 0;
}