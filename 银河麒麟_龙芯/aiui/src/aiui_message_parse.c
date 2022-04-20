#include <stdbool.h>
#include <string.h>
#include "cjson.h"
#include "aiui_log.h"
#include "aiui_wrappers_os.h"
#include "aiui_device_info.h"
#include "aiui_media_parse.h"
#include "aiui_skill_parse.h"
#include "aiui_message_parse.h"

#define AIUI_TAG "aiui_msg"

typedef struct aiui_message_parse_s
{
    aiui_message_parse_cb cb;
    void *cb_param;
} aiui_message_parse_t;

typedef int (*pfun)(aiui_message_parse_t *handle, const cJSON *json);

typedef struct aiui_function_map_s
{
    const char *type;
    pfun func;
} aiui_function_map_t;

static bool _aiui_get_cjson_bool(const cJSON *json, const char *name, bool flag)
{
    cJSON *item = cJSON_GetObjectItem(json, name);
    if (flag) {
        return (bool)(cJSON_IsTrue(item) ? true : false);
    } else {
        return (bool)(cJSON_IsFalse(item) ? true : false);
    }
}

static int _aiui_sub_iat(aiui_message_parse_t *handle, const cJSON *data)
{
    LOGD(AIUI_TAG, "sub is iat");
    if (cJSON_IsInvalid(data)) return -1;
    cJSON *text = cJSON_GetObjectItem(data, "text");
    handle->cb(handle->cb_param, AIUI_MESSAGE_IAT, text->valuestring);
    return 0;
}

static int _aiui_sub_nlp(aiui_message_parse_t *handle, const cJSON *data)
{
    LOGD(AIUI_TAG, "sub is nlp");
    if (cJSON_IsInvalid(data)) return -1;
    cJSON *intent = cJSON_GetObjectItem(data, "intent");
    if (cJSON_IsInvalid(intent)) return -1;
    cJSON *answer = cJSON_GetObjectItem(intent, "answer");
    cJSON *text = cJSON_GetObjectItem(answer, "text");
    if (text != NULL) {
        handle->cb(handle->cb_param, AIUI_MESSAGE_ANSWER, text->valuestring);
    }
    aiui_skill_parse(intent);
    return 0;
}

static int _aiui_sub_tpp(aiui_message_parse_t *handle, const cJSON *data)
{
    LOGD(AIUI_TAG, "sub is tpp");
    return 0;
}

static int _aiui_sub_tts(aiui_message_parse_t *handle, const cJSON *data)
{
    LOGD(AIUI_TAG, "sub is tts");
    if (cJSON_IsInvalid(data)) return -1;
    cJSON *tts_url_base64 = cJSON_GetObjectItem(data, "content");

    char *tts_url = aiui_parse_tts_url(tts_url_base64->valuestring);
    handle->cb(handle->cb_param, AIUI_MESSAGE_TTS, tts_url);
    if (tts_url != NULL) aiui_hal_free(tts_url);
#ifndef AIUI_LONG_CONNECT
    bool is_finish = _aiui_get_cjson_bool(data, "is_finish", true);
    if (is_finish) handle->cb(handle->cb_param, AIUI_MESSAGE_FINISH, NULL);
#endif
    return 0;
}

static aiui_function_map_t s_sub_func[] = {{"iat", _aiui_sub_iat},
                                           {"nlp", _aiui_sub_nlp},
                                           {"snlp", _aiui_sub_nlp},
                                           {"tpp", _aiui_sub_tpp},
                                           {"tts", _aiui_sub_tts}};

static int _aiui_action_connected(aiui_message_parse_t *handle, const cJSON *result)
{
    handle->cb(handle->cb_param, AIUI_MESSAGE_CONNECTED, NULL);
    return 0;
}

static int _aiui_action_pong(aiui_message_parse_t *handle, const cJSON *result)
{
    LOGD(AIUI_TAG, "recv pong");
    handle->cb(handle->cb_param, AIUI_MESSAGE_CONNECTED, NULL);
    return 0;
}
static int _aiui_action_started(aiui_message_parse_t *handle, const cJSON *result)
{
    cJSON *sid = cJSON_GetObjectItem(result, "sid");
    handle->cb(handle->cb_param, AIUI_MESSAGE_STARTED, sid->valuestring);
    return 0;
}

static int _aiui_action_result(aiui_message_parse_t *handle, const cJSON *result)
{
    if (cJSON_IsInvalid(result)) return -1;
    cJSON *data = cJSON_GetObjectItem(result, "data");
    if (!cJSON_IsObject(data)) return -1;
    cJSON *sub = cJSON_GetObjectItem(data, "sub");
    for (size_t i = 0; i < sizeof(s_sub_func) / sizeof(s_sub_func[0]); i++) {
        if (strcmp(s_sub_func[i].type, sub->valuestring) == 0) {
            s_sub_func[i].func(handle, data);
            break;
        }
    }
    return 0;
}

static int _aiui_action_vad(aiui_message_parse_t *handle, const cJSON *result)
{
    handle->cb(handle->cb_param, AIUI_MESSAGE_VAD, NULL);
    return 0;
}
static int _aiui_action_error(aiui_message_parse_t *handle, const cJSON *result)
{
    LOGD(AIUI_TAG, "recv error");
    handle->cb(handle->cb_param, AIUI_MESSAGE_ERROR, NULL);
    return 0;
}

static int _aiui_action_finsh(aiui_message_parse_t *handle, const cJSON *result)
{
    LOGD(AIUI_TAG, "recv finish");
    handle->cb(handle->cb_param, AIUI_MESSAGE_FINISH, NULL);
    return 0;
}

AIUI_MESSAGE_PARSE_HANDLE aiui_message_parse_init(aiui_message_parse_cb cb, void *param)
{
    aiui_message_parse_t *handle =
        (aiui_message_parse_t *)aiui_hal_malloc(sizeof(aiui_message_parse_t));
    handle->cb = cb;
    handle->cb_param = param;
    return (AIUI_MESSAGE_PARSE_HANDLE)handle;
}

static aiui_function_map_t s_action_func[] = {{"connected", _aiui_action_connected},
                                              {"started", _aiui_action_started},
                                              {"vad", _aiui_action_vad},
                                              {"finish", _aiui_action_finsh},
                                              {"pong", _aiui_action_pong},
                                              {"result", _aiui_action_result},
                                              {"error", _aiui_action_error}};

int aiui_message_parse(AIUI_MESSAGE_PARSE_HANDLE handle, const char *msg, const int len)
{
    if (NULL == msg || NULL == handle) return -1;
    aiui_message_parse_t *self = (aiui_message_parse_t *)handle;
    if (NULL == self->cb || NULL == self->cb_param) {
        LOGE(AIUI_TAG, "callback or callback param is null");
        return -1;
    }
    cJSON *result = cJSON_Parse(msg);
    if (NULL == result || cJSON_IsInvalid(result)) return -1;
    //LOGD(AIUI_TAG, "msg = %s", msg);
    cJSON *action = cJSON_GetObjectItem(result, "action");
    if (NULL == action || NULL == action->valuestring) {
        cJSON_Delete(result);
        return -1;
    }
    for (size_t i = 0; i < sizeof(s_action_func) / sizeof(s_action_func[0]); i++) {
        if (strcmp(s_action_func[i].type, action->valuestring) == 0) {
            s_action_func[i].func(self, result);
            break;
        }
    }
    cJSON_Delete(result);
    return 0;
}

void aiui_message_parse_uninit(AIUI_MESSAGE_PARSE_HANDLE handle)
{
    aiui_hal_free(handle);
}