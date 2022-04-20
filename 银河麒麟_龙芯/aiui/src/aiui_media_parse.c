#include "aiui_wrappers_os.h"
#include "aiui_log.h"
#include "string.h"
#include "base64.h"
#include "aiui_media_parse.h"

#define AIUI_TAG "aiui_media"

static aiui_playlist_t s_media_list = {0};

void aiui_media_free()
{
    for (int i = 0; i < s_media_list.item_num; i++) {
        if (s_media_list.item[i].playUrl != NULL) {
            aiui_hal_free(s_media_list.item[i].playUrl);
        }
        if (s_media_list.item[i].itemID != NULL) {
            aiui_hal_free(s_media_list.item[i].itemID);
        }
        if (s_media_list.item[i].name != NULL) {
            aiui_hal_free(s_media_list.item[i].name);
        }
        if (s_media_list.item[i].allrate != NULL) {
            aiui_hal_free(s_media_list.item[i].allrate);
        }
    }

    memset(&s_media_list, 0, sizeof(s_media_list));
}

aiui_playlist_t *aiui_get_media_info()
{
    return &s_media_list;
}

char *aiui_parse_tts_url(const char *data)
{
    int inlen = strlen(data);
    if (NULL == data || inlen <= 0) return NULL;

    char *output = (char *)aiui_hal_malloc(inlen);
    memset(output, 0, inlen);
    size_t write_len = 0;
    if (mbedtls_base64_decode(
            (unsigned char *)output, inlen, &write_len, (const unsigned char *)data, inlen) != 0) {
        return NULL;
    } else {
        return output;
    }
}

int aiui_parse_media_info(const cJSON *data)
{
    if (cJSON_IsInvalid(data)) return -1;
    cJSON *result = cJSON_GetObjectItem(data, "result");
    if (cJSON_IsInvalid(result)) return -1;
    int array_size = cJSON_GetArraySize(result);
    if (array_size <= 0) return -1;
    if (array_size > AIUI_ITEM_NUM_MAX) array_size = AIUI_ITEM_NUM_MAX;
    LOGD(AIUI_TAG, "array_size: %d\n", array_size);
    aiui_media_free();
    int i = 0;    // 媒资下标
    for (int k = 0; k < array_size; k++) {
        cJSON *item = cJSON_GetArrayItem(result, k);
        cJSON *part = cJSON_GetObjectItem(item, "playable");
        if (part != NULL) {
            LOGD(AIUI_TAG, "playable: %d\n", part->valueint);
            if (part->valueint == 0)    // 不能播放的itemID不保存
            {
                continue;
            }
        }
        // parse allrate
        part = cJSON_GetObjectItem(item, "allrate");
        if (part != NULL && strlen(part->valuestring) > 0) {
            s_media_list.item[i].allrate = (char *)aiui_hal_malloc(strlen(part->valuestring) + 1);
            memcpy(s_media_list.item[i].allrate, part->valuestring, strlen(part->valuestring));
            s_media_list.item[i].allrate[strlen(part->valuestring)] = '\0';
            LOGD(AIUI_TAG, "allrate: %s\n", s_media_list.item[i].allrate);
        }
        // parse url
        part = cJSON_GetObjectItem(item, "uni_url");
        if (NULL == part) {
            part = cJSON_GetObjectItem(item, "url");
        }
        if (part != NULL && strlen(part->valuestring) > 0) {
            s_media_list.item[i].playUrl = (char *)aiui_hal_malloc(strlen(part->valuestring) + 1);
            memcpy(s_media_list.item[i].playUrl, part->valuestring, strlen(part->valuestring));
            s_media_list.item[i].playUrl[strlen(part->valuestring)] = '\0';
            LOGD(AIUI_TAG, "url: %s\n", s_media_list.item[i].playUrl);
        }
        // parse itemID
        part = cJSON_GetObjectItem(item, "itemid");
        if (part != NULL && strlen(part->valuestring) > 0) {
            s_media_list.item[i].itemID = (char *)aiui_hal_malloc(strlen(part->valuestring) + 1);
            memcpy(s_media_list.item[i].itemID, part->valuestring, strlen(part->valuestring));
            s_media_list.item[i].itemID[strlen(part->valuestring)] = '\0';
            LOGD(AIUI_TAG, "itemID: %s\n", s_media_list.item[i].itemID);
        }
        // parse name
        part = cJSON_GetObjectItem(item, "name");
        if (part != NULL && strlen(part->valuestring) > 0) {
            s_media_list.item[i].name = (char *)aiui_hal_malloc(strlen(part->valuestring) + 1);
            memcpy(s_media_list.item[i].name, part->valuestring, strlen(part->valuestring));
            s_media_list.item[i].name[strlen(part->valuestring)] = '\0';
            LOGD(AIUI_TAG, "name: %s\n", s_media_list.item[i].name);
        }
        i++;
        s_media_list.item_num++;
    }
    s_media_list.item_pos = 1;
    LOGD(AIUI_TAG, "media size %d\n", s_media_list.item_num);
    return 0;
}
