#include "string.h"
#include "aiui_log.h"
#include "aiui_media_parse.h"
#include "aiui_skill_parse.h"
#include "aiui_device_info.h"
#include "aiui_audio_control.h"

#define AIUI_TAG "aiui_skill"

typedef void (*pfunc)(const cJSON *json);

typedef struct aiui_skill_func_map_s
{
    const char *type;
    pfunc func;
} aiui_func_map_t;

static void _aiui_vol_up(const cJSON *json)
{
    aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_VOLUP, NULL);
    LOGD(AIUI_TAG, "aiui_parse vol up");
}

static void _aiui_vol_down(const cJSON *json)
{
    aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_VOLDOWN, NULL);
    LOGD(AIUI_TAG, "aiui_parse vol down");
}

static void _aiui_vol_select(const cJSON *data)
{
    if (NULL == data || cJSON_IsInvalid(data)) return;
    aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_VOLVALUE, data->valuestring);
    LOGD(AIUI_TAG, "vol select %s", data->valuestring);
}

static void _aiui_vol_max(const cJSON *json)
{
    aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_VOLMAX, NULL);
    LOGD(AIUI_TAG, "volcontrol max");
}

static void _aiui_vol_min(const cJSON *json)
{
    aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_VOLMIN, NULL);
    LOGD(AIUI_TAG, "volcontrol min");
}

static void _aiui_audio_next(const cJSON *json)
{
    LOGD(AIUI_TAG, "next");
    aiui_audio_ctrl_cmd_t cmd = AIUI_AUDIO_CTRL_NEXT;
    aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_SET_STATE, &cmd);
}

static void _aiui_audio_past(const cJSON *json)
{
    LOGD(AIUI_TAG, "past");
    aiui_audio_ctrl_cmd_t cmd = AIUI_AUDIO_CTRL_PREV;
    aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_SET_STATE, &cmd);
}

static void _aiui_audio_stop(const cJSON *json)
{
    LOGD(AIUI_TAG, "pause");
    aiui_audio_ctrl_cmd_t cmd = AIUI_AUDIO_CTRL_STOP;
    aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_SET_STATE, &cmd);
}

static void _aiui_audio_replay(const cJSON *json)
{
    LOGD(AIUI_TAG, "replay");
    aiui_audio_ctrl_cmd_t cmd = AIUI_AUDIO_CTRL_RESUME;
    aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_SET_STATE, &cmd);
}

static aiui_func_map_t s_ctrl_func[] = {{"volume_plus", _aiui_vol_up},
                                        {"VOLUME_PLUS", _aiui_vol_up},
                                        {"volume_minus", _aiui_vol_down},
                                        {"VOLUME_MINUS", _aiui_vol_down},
                                        {"volume_select", _aiui_vol_select},
                                        {"VOLUME_SET", _aiui_vol_select},
                                        {"volume_max", _aiui_vol_max},
                                        {"VOLUME_MAX", _aiui_vol_max},
                                        {"volume_min", _aiui_vol_min},
                                        {"VOLUME_MIN", _aiui_vol_min},
                                        {"next", _aiui_audio_next},
                                        {"CHOOSE_NEXT", _aiui_audio_next},
                                        {"past", _aiui_audio_past},
                                        {"CHOOSE_PREVIOUS", _aiui_audio_past},
                                        {"pause", _aiui_audio_stop},
                                        {"PAUSE", _aiui_audio_stop},
                                        {"replay", _aiui_audio_replay},
                                        {"RESUME_PLAY", _aiui_audio_replay}};

static void _aiui_media_control(const cJSON *data)
{
    if (cJSON_IsInvalid(data)) {
        return;
    }
    int array_size = cJSON_GetArraySize(data);
    LOGD(AIUI_TAG, "size = %d", array_size);
    cJSON *instype = NULL;
    cJSON *value = NULL;
    for (int i = 0; i < array_size; i++) {
        cJSON *slot_i = cJSON_GetArrayItem(data, i);
        if (NULL == slot_i) continue;
        cJSON *name = cJSON_GetObjectItem(slot_i, "name");
        if (NULL == name) continue;
        if (strcmp(name->valuestring, "insType") == 0) {
            instype = cJSON_GetObjectItem(slot_i, "value");
        } else {
            value = cJSON_GetObjectItem(slot_i, "value");
        }
    }

    if (NULL == instype || NULL == instype->valuestring) return;
    for (size_t i = 0; i < sizeof(s_ctrl_func) / sizeof(s_ctrl_func[0]); i++) {
        if (strcmp(s_ctrl_func[i].type, instype->valuestring) == 0) {
            s_ctrl_func[i].func(value);
            break;
        }
    }
}

static void _aiui_skill_media(const cJSON *intent)
{
    cJSON *semantic = cJSON_GetObjectItem(intent, "semantic");
    if (NULL == semantic) return;
    cJSON *semantic_0 = cJSON_GetArrayItem(semantic, 0);
    if (NULL == semantic_0) return;
    cJSON *instruction = cJSON_GetObjectItem(semantic_0, "intent");
    if (NULL == instruction) return;
    LOGD(AIUI_TAG, "%s", instruction->valuestring);
    cJSON *media_info = cJSON_GetObjectItem(intent, "data");
    if (aiui_parse_media_info(media_info) == 0) {
        aiui_audio_ctrl_cmd_t cmd = AIUI_AUDIO_CTRL_PLAY;
        aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_SET_STATE, &cmd);
    }
    if (strcmp(instruction->valuestring, "INSTRUCTION") == 0) {
        LOGD(AIUI_TAG, "INSTRUCTION");
        cJSON *slots = cJSON_GetObjectItem(semantic_0, "slots");
        _aiui_media_control(slots);
    }
}

static void _aiui_skill_control(const cJSON *intent)
{
    cJSON *semantic = cJSON_GetObjectItem(intent, "semantic");
    if (NULL == semantic) return;
    cJSON *semantic_0 = cJSON_GetArrayItem(semantic, 0);
    if (NULL == semantic_0) return;
    cJSON *instruction = cJSON_GetObjectItem(semantic_0, "intent");
    if (NULL == instruction || NULL == instruction->valuestring) return;
    LOGD(AIUI_TAG, "%s", instruction->valuestring);
    cJSON *value = NULL;
    cJSON *slots = cJSON_GetObjectItem(semantic_0, "slots");
    if (slots != NULL) {
        cJSON *slot = cJSON_GetArrayItem(slots, 0);
        value = cJSON_GetObjectItem(slot, "value");
    }
    for (size_t i = 0; i < sizeof(s_ctrl_func) / sizeof(s_ctrl_func[0]); i++) {
        if (strcmp(s_ctrl_func[i].type, instruction->valuestring) == 0) {
            s_ctrl_func[i].func(value);
            break;
        }
    }
}

static aiui_func_map_t s_skill_func[] = {{"media", _aiui_skill_media},
                                         {"drama", _aiui_skill_media},
                                         {"AIUI.control", _aiui_skill_control}};

int aiui_skill_parse(const cJSON *intent)
{
    if (cJSON_IsInvalid(intent)) return -1;
    cJSON *service = cJSON_GetObjectItem(intent, "service");
    cJSON *service_pkg = cJSON_GetObjectItem(intent, "service_pkg");
    if (service != NULL) LOGD(AIUI_TAG, "service = %s", service->valuestring);
    if (service_pkg != NULL) LOGD(AIUI_TAG, "service_pkg = %s", service_pkg->valuestring);
    for (size_t i = 0; i < sizeof(s_skill_func) / sizeof(s_skill_func[0]); i++) {
        if ((NULL != service && strcmp(s_skill_func[i].type, service->valuestring) == 0) ||
            (NULL != service_pkg && strcmp(s_skill_func[i].type, service_pkg->valuestring) == 0)) {
            s_skill_func[i].func(intent);
            break;
        }
    }
    return 0;
}
