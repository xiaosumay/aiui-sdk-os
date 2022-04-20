#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "md5.h"
#include "cjson.h"
#include "aiui_wrappers_os.h"
#include "aiui_log.h"
#include "aiui_device_info.h"
#include "aiui_http_api.h"
#include "aiui_get_upload_info.h"

#define AIUI_UPLOAD_APIKEY "9d67a4b3-1f42-4a51-836f-4f4d7560cf42"
#define AIUI_UPLOAD_HOST   "https://aios.xfyun.cn"
#define AIUI_BIND_RESOURCE "/ota/firmware/version/search?appid=%s&deviceId=%s"
#define AIUI_TAG           "aiui_upload"

int _aiui_parse_response(const char *data, aiui_upload_info_t *info)
{
    if (NULL == data) return -1;

    LOGD(AIUI_TAG, "recv body: %s", data);
    cJSON *result = cJSON_Parse(data);
    if (cJSON_IsInvalid(result)) {
        LOGD(AIUI_TAG, "invalid json format");
        return -1;
    }
    cJSON *code = cJSON_GetObjectItem(result, "code");
    if (NULL == code || code->valueint != 0) {
        LOGD(AIUI_TAG, "get upload failed");
        cJSON_Delete(result);
        return -1;
    }
    cJSON *content = cJSON_GetObjectItem(result, "data");
    if (NULL == content) {
        LOGD(AIUI_TAG, "get upload info failed");
        cJSON_Delete(result);
        return 0;
    }

    memset(info, 0, sizeof(aiui_upload_info_t));
    cJSON *appid = cJSON_GetObjectItem(content, "appid");
    int max_len = 0;
    if (appid != NULL && appid->valuestring != NULL) {
        max_len = (strlen(appid->valuestring) >= 15) ? 15 : strlen(appid->valuestring);
        memcpy(info->appid, appid->valuestring, max_len);
    }
    cJSON *version = cJSON_GetObjectItem(content, "version");
    if (version != NULL && version->valuestring != NULL) {
        max_len = (strlen(version->valuestring) >= 15) ? 15 : strlen(version->valuestring);
        //LOGD(AIUI_TAG, "version = %s", version->valuestring);
        memcpy(info->version, version->valuestring, max_len);
    }
    cJSON *updateWay = cJSON_GetObjectItem(content, "updateWay");
    if (updateWay != NULL) info->update_way = updateWay->valueint;
    cJSON *downloadUrl = cJSON_GetObjectItem(content, "downloadUrl");
    if (downloadUrl != NULL && downloadUrl->valuestring != NULL) {
        max_len = (strlen(downloadUrl->valuestring) >= 127) ? 127
                                                            : strlen(downloadUrl->valuestring);
        memcpy(info->url, downloadUrl->valuestring, 127);
    }
    cJSON_Delete(result);
    return 0;
}

int aiui_get_upload_info(const char *appid, const char *deviceid, aiui_upload_info_t *info)
{
    char auth_format[128] = {0};
    int time_stamp = time(NULL);
    char checksum_md5[16] = {0};
    char checksum_md5_hex[33] = {0};
    char url[256] = {0};
    srand(time_stamp);
    int nonce = rand();
    snprintf(auth_format, 128, "%s%d%d", AIUI_UPLOAD_APIKEY, time_stamp, nonce);
    mbedtls_md5_ret(
        (unsigned char *)auth_format, strlen(auth_format), (unsigned char *)checksum_md5);
    for (int i = 0; i < 16; ++i) {
        sprintf(checksum_md5_hex + i * 2, "%02x", (unsigned char)checksum_md5[i]);
    }
    snprintf(url, 256, AIUI_UPLOAD_HOST AIUI_BIND_RESOURCE, appid, deviceid);
    AIUI_HTTP_HANDLE handle = aiui_http_create_request(url, "GET", NULL, NULL, NULL);
    char time_stamp_header[64] = {0};
    snprintf(time_stamp_header, 64, "X-Timestamp: %d", time_stamp);
    aiui_http_add_header(handle, time_stamp_header);
    char nonce_header[64] = {0};
    snprintf(nonce_header, 64, "X-Nonce: %d", nonce);
    aiui_http_add_header(handle, nonce_header);
    char sign_header[128] = {0};
    snprintf(sign_header, 128, "X-Sign: %s", checksum_md5_hex);
    aiui_http_add_header(handle, sign_header);
    if (aiui_http_send_request(handle, NULL) != 0) {
        aiui_http_destroy_request(handle);
        return -1;
    }
    char recv[1024] = {0};
    aiui_http_recv(handle, recv, 1024);
    aiui_http_destroy_request(handle);
    return _aiui_parse_response(recv, info);
}