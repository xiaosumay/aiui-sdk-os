#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include "aiui_wrappers_os.h"
#include "aiui_log.h"
#include "base64.h"
#include "sha256.h"
#include "aiui_message_parse.h"
#include "aiui_device_info.h"
#include "aiui_websocket_api.h"
#include "aiui_audio_control.h"
#include "aiui_convert_url.h"
#ifdef AIUI_OPEN_VAD
#    include "fvad.h"
#endif
#include "aiui_api.h"

#define AIUI_TAG "aiui_api"
#ifdef AIUI_LONG_CONNECT
#    define AIUI_WS_PATH_FORMAT "/v2/aiui?appid=%s&checksum=%s&curtime=%s&param=%s&signtype=sha256"
#else
#    define AIUI_WS_PATH_FORMAT "/v1/aiui?appid=%s&checksum=%s&curtime=%s&param=%s&signtype=sha256"
#endif
#define AIUI_WS_ORIGIN_HOST "wss://wsapi.xfyun.cn"

static const char *s_data_type[] = {"audio", "text", "text"};
static const char *s_scene[] = {"main_box", "main_box", "IFLYTEK.tts"};

static const char *s_aiui_param =
    "{\"result_level\":\"plain\","
    "\"auth_id\":\"%s\","
    "\"sn\":\"%s\","
    "\"data_type\":\"%s\","
    "\"aue\":\"raw\","
    "\"sample_rate\":\"%d\","
    "\"vad_info\":\"end\","
    "\"tts_res_type\":\"url\","
    "\"vcn\":\"x2_xiaojuan\","
    "\"tts_aue\":\"lame\","
    "\"scene\":\"%s\","
    "\"ip_location\":\"auto\","
    "\"cloud_vad_eos\":\"700\","
    "\"interact_mode\":\"continuous\","
    "\"pers_param\":\"{\\\"auti_id\\\":\\\"%s\\\"}\","
    "\"context\":\"{\\\"sdk_support\\\":[\\\"iat\\\",\\\"nlp\\\",\\\"tts\\\"]}\"}";

typedef enum aiui_connect_status_s {
    AIUI_DISCONNECTED = 0,
    AIUI_CONNECTED,
    AIUI_CONNECTING
} aiui_connect_status_t;

typedef enum aiui_session_status_s {
    AIUI_SESSION_STOPED = 0,
    AIUI_SESSION_STARTED,
    AIUI_SESSION_STARTING,
    AIUI_SESSION_VAD
} aiui_session_status_t;

typedef struct aiui_info_s
{
    aiui_connect_status_t connect_status;
    aiui_session_status_t session_status;
    unsigned int heart_beat_id;
    AIUI_MESSAGE_PARSE_HANDLE message_parse_handle;
    AIUI_WEBSOCKET_HANDLE websocket_handle;
    aiui_websocket_cb_t websokcet_cb;
    AIUI_USE_TYPE type;
    int vad_begin_count;
    int vad_end_count;
    int is_vad_begin;
    int is_process_tts;    //是否处理过tts，一轮只处理一次
    void *sem;
#ifdef AIUI_OPEN_VAD
    Fvad *fvad;
#endif
    void *app_param;
    void (*event)(void *param, AIUI_EVENT_TYPE type, const char *data);
} aiui_info_t;

static void _aiui_closed(void *param)
{
    aiui_info_t *aiui = (aiui_info_t *)param;
    if (NULL == param) return;
    aiui->session_status = AIUI_SESSION_STOPED;
    aiui->connect_status = AIUI_DISCONNECTED;
    LOGD(AIUI_TAG, "aiui closed");
}

static void _aiui_message(void *param, const char *msg, int len)
{
    aiui_info_t *aiui = (aiui_info_t *)param;
    if (NULL == param) return;
    aiui_message_parse(aiui->message_parse_handle, msg, len);
}

static void _aiui_message_parse_cb(void *param, AIUI_MESSAGE_TYPE type, const char *msg)
{
    if (NULL == param) return;
    aiui_info_t *aiui = (aiui_info_t *)param;
    LOGD(AIUI_TAG, "message type is %d", type);
    switch (type) {
        case AIUI_MESSAGE_CONNECTED:
            aiui->connect_status = AIUI_CONNECTED;
            break;
        case AIUI_MESSAGE_STARTED:
            LOGD(AIUI_TAG, "sid = %s", msg);
            aiui->connect_status = AIUI_CONNECTED;
            aiui->session_status = AIUI_SESSION_STARTED;
            aiui->is_process_tts = 0;
            // 只有语音交互时，才需要AIUI_EVENT_START_SEND消息
            if (aiui->event != NULL && AIUI_AUDIO_INTERACTIVE == aiui->type)
                aiui->event(aiui->app_param, AIUI_EVENT_START_SEND, NULL);
            aiui_hal_sem_post(aiui->sem);
            break;
        case AIUI_MESSAGE_VAD:
            LOGD(AIUI_TAG, "recv vad");
            if (aiui->session_status != AIUI_SESSION_STARTED) return;
            // 只有语音交互时，才需要AIUI_EVENT_STOP_SEND消息
            if (aiui->event != NULL && AIUI_AUDIO_INTERACTIVE == aiui->type)
                aiui->event(aiui->app_param, AIUI_EVENT_STOP_SEND, NULL);
            aiui->session_status = AIUI_SESSION_VAD;
            break;
        case AIUI_MESSAGE_IAT:
            LOGD(AIUI_TAG, "iat = %s", msg);
            if (aiui->event != NULL) aiui->event(aiui->app_param, AIUI_EVENT_IAT, msg);
            break;
        case AIUI_MESSAGE_ANSWER:
            LOGD(AIUI_TAG, "answer = %s", msg);
            if (aiui->event != NULL) aiui->event(aiui->app_param, AIUI_EVENT_ANSWER, msg);
            break;
        case AIUI_MESSAGE_TTS:
            if (msg != NULL) {
                aiui->is_process_tts = 1;
                aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_TTS, (void *)msg);
            }
            break;
        case AIUI_MESSAGE_ERROR:
            aiui_hal_sem_post(aiui->sem);
            break;
        case AIUI_MESSAGE_FINISH:
            aiui->session_status = AIUI_SESSION_STOPED;
            if (0 == aiui->is_process_tts) {
                aiui->is_process_tts = 1;
                aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_TTS, NULL);
            }
            if (aiui->event != NULL) aiui->event(aiui->app_param, AIUI_EVENT_FINISH, NULL);
            break;
        default:
            break;
    }
}

static char *_aiui_path_format(const char *appid,
                               const char *key,
                               AIUI_USE_TYPE type,
                               const char *authid,
                               const char *dynamic_id)
{
    LOGD(AIUI_TAG, "%s %s %s %s\n", appid, key, authid, dynamic_id);
    int param_len = strlen(s_aiui_param) + strlen(authid) * 3 + strlen(s_data_type[type]) +
                    strlen(s_scene[type]) + 1;
    char *param = (char *)aiui_hal_malloc(param_len);
    memset(param, 0, param_len);
    snprintf(param,
             param_len - 1,
             s_aiui_param,
             authid,
             authid,
             s_data_type[type],
             AIUI_AUDIO_SAMPLE,
             s_scene[type],
             dynamic_id);
    int base64_param_len = (strlen(param) + 2) / 3 * 4 + 4;
    char *base64_param = (char *)aiui_hal_malloc(base64_param_len);
    char time_str[20] = {0};
    snprintf(time_str, 19, "%lu", time(NULL));
    size_t write_len = 0;
    if (mbedtls_base64_encode((unsigned char *)base64_param,
                              base64_param_len,
                              &write_len,
                              (unsigned char *)param,
                              strlen(param)) != 0) {
        aiui_hal_free(param);
        aiui_hal_free(base64_param);
        return NULL;
    }
    int checksum_len = strlen(key) + strlen(time_str) + strlen(base64_param) + 4;
    char *checksum = (char *)aiui_hal_malloc(checksum_len);
    memset(checksum, 0, checksum_len);
    snprintf(checksum, checksum_len, "%s%s%s", key, time_str, base64_param);

    unsigned char checksum_encryption[32] = {0};
    sha256((const unsigned char *)checksum, strlen(checksum), checksum_encryption);
    char checksum_encryption_hex[70] = {0};
    for (int i = 0; i < 32; ++i) {
        sprintf(checksum_encryption_hex + i * 2, "%02x", checksum_encryption[i]);
    }
    int websocket_url_len = strlen(AIUI_WS_PATH_FORMAT) + strlen(appid) +
                            strlen(checksum_encryption_hex) + strlen(time_str) +
                            strlen(base64_param) + 1;

    char *websocket_url = (char *)aiui_hal_malloc(websocket_url_len);
    memset(websocket_url, 0, websocket_url_len);
    snprintf(websocket_url,
             websocket_url_len - 1,
             AIUI_WS_PATH_FORMAT,
             appid,
             checksum_encryption_hex,
             time_str,
             base64_param);
    aiui_hal_free(param);
    aiui_hal_free(checksum);
    aiui_hal_free(base64_param);
    return websocket_url;
}

static int aiui_connect(aiui_info_t *aiui)
{
    aiui_print_mem_info();
    if (NULL == aiui) {
        LOGE(AIUI_TAG, "websocket not init");
        return -1;
    }
    if (aiui->connect_status != AIUI_DISCONNECTED) {
        LOGE(AIUI_TAG, "websocket already running");
        return 0;
    }
    char *path =
        _aiui_path_format(AIUI_APP_ID, AIUI_APP_KEY, aiui->type, AIUI_AUTH_ID, AIUI_AUTH_ID);
    if (NULL == path) {
        LOGE(AIUI_TAG, "path format error");
        return -4;
    }
    aiui->connect_status = AIUI_CONNECTING;
    int ret = aiui_websocket_connect(aiui->websocket_handle, path, AIUI_WS_ORIGIN_HOST);
    aiui_hal_free(path);
    return ret;
}

#ifdef AIUI_OPEN_VAD
static int _aiui_judge_vad(aiui_info_t *aiui, int ret)
{
    //LOGD(AIUI_TAG, "ret = %d, begin_count = %d, end_count = %d", ret, aiui->vad_begin_count, aiui->vad_end_count);
    if (ret == -1) return -1;
    if (0 == aiui->is_vad_begin) {
        if (ret == 1)
            aiui->vad_begin_count++;
        else
            aiui->vad_begin_count = 0;
    } else {
        if (ret == 0)
            aiui->vad_end_count++;
        else
            aiui->vad_end_count = 0;
    }
    if (aiui->vad_begin_count > 15) aiui->is_vad_begin = 1;
    if (aiui->vad_end_count > 5) return 0;
    return -1;
}
#endif

AIUI_HANDLE aiui_init(AIUI_USE_TYPE type,
                      void (*event)(void *param, AIUI_EVENT_TYPE type, const char *msg),
                      void *param)
{
    aiui_info_t *aiui = (aiui_info_t *)aiui_hal_malloc(sizeof(aiui_info_t));
    memset(aiui, 0, sizeof(aiui_info_t));
    aiui_websocket_cb_t websocket_cb_tmp = {_aiui_closed, _aiui_message};
    aiui->type = type;
    aiui->event = event;
    aiui->app_param = param;
    aiui->connect_status = AIUI_DISCONNECTED;
    aiui->session_status = AIUI_SESSION_STOPED;
#ifdef AIUI_OPEN_VAD
    aiui->fvad = fvad_new();
    fvad_set_mode(aiui->fvad, 3);
    fvad_set_sample_rate(aiui->fvad, AIUI_AUDIO_SAMPLE);
#endif
    aiui->sem = aiui_hal_sem_create();
    aiui->websocket_handle = aiui_websocket_create(websocket_cb_tmp, (void *)aiui);
    aiui->message_parse_handle = aiui_message_parse_init(_aiui_message_parse_cb, (void *)aiui);
    if (type != AIUI_TTS) {
        aiui_active();    // 音乐授权
    }
    aiui_audio_ctrl_init();
    return (AIUI_HANDLE)aiui;
}

void aiui_uninit(AIUI_HANDLE handle)
{
    if (handle != NULL) {
        aiui_audio_ctrl_deinit();
        aiui_info_t *aiui = (aiui_info_t *)handle;
        aiui_hal_sem_post(aiui->sem);
        aiui_message_parse_uninit(aiui->message_parse_handle);
        aiui_websocket_destroy(aiui->websocket_handle);
#ifdef AIUI_OPEN_VAD
        fvad_free(aiui->fvad);
#endif
        aiui_hal_sem_destroy(aiui->sem);
        aiui_hal_free(handle);
        handle = NULL;
    }
    LOGD(AIUI_TAG, "aiui uninit");
    aiui_print_mem_info();
}

int aiui_send(AIUI_HANDLE handle, const char *buf, const int len)
{
    if (NULL == handle) return -1;
    aiui_info_t *aiui = (aiui_info_t *)handle;
#ifdef AIUI_LONG_CONNECT
    if (aiui->session_status != AIUI_SESSION_STARTED) return -1;
#endif
#ifdef AIUI_OPEN_VAD
    if (AIUI_AUDIO_INTERACTIVE == aiui->type)    // 只要音频数据才过VAD检测
    {
        int ret = fvad_process(aiui->fvad, (const int16_t *)buf, len / 2);
        if (0 == _aiui_judge_vad(aiui, ret)) {
            LOGD(AIUI_TAG, "send vad");
            if (aiui->event != NULL) aiui->event(aiui->app_param, AIUI_EVENT_STOP_SEND, NULL);
            return 0;
        }
    }
#endif
    //LOGD(AIUI_TAG, "send len = %d", len);
    if (aiui->connect_status != AIUI_CONNECTED) return -1;
    return aiui_websocket_send_bin(aiui->websocket_handle, buf, len);
}

#ifdef AIUI_LONG_CONNECT
int aiui_start(AIUI_HANDLE handle, AIUI_USE_TYPE type)
{
    if (NULL == handle) return -1;
    aiui_info_t *aiui = (aiui_info_t *)handle;
    if (AIUI_DISCONNECTED == aiui->connect_status) {
        aiui_websocket_disconect(aiui->websocket_handle);
        if (aiui_connect(aiui) != 0) {
            return -1;
        }
    }
    if (AIUI_SESSION_STARTED == aiui->session_status) {
        if (aiui->event != NULL) aiui->event(aiui->app_param, AIUI_EVENT_START_SEND, NULL);
        return 0;
    }
    if (AIUI_SESSION_STARTING == aiui->session_status) {
        return 0;
    }
    const char msg[] =
        "{\"action\": \"start\","
        "\"param\": {"
        "\"data_type\": \"%s\","
        "\"scene\":\"%s\","
        //"\"result_level\": \"plain\","
        "\"clean_dialog_history\": \"user\""
        //"\"aue\": \"raw\""
        "}"
        "}";
    aiui->type = type;
    aiui->session_status = AIUI_SESSION_STARTING;
    aiui->is_vad_begin = 0;
    aiui->vad_begin_count = 0;
    aiui->vad_end_count = 0;
    aiui->is_process_tts = 0;
    char start_msg[256] = {0};
    snprintf(start_msg, 256, msg, s_data_type[type], s_scene[type]);
    LOGD(AIUI_TAG, "start_msg = %s", start_msg);
    int ret = aiui_websocket_send_text(aiui->websocket_handle, start_msg, strlen(start_msg));
    int rc = aiui_hal_sem_wait(aiui->sem, 5000);
    LOGD(AIUI_TAG, "start ret = %d, rc = %d, error = %s", ret, rc, strerror(errno));
    if (rc != 0) {
        LOGE(AIUI_TAG, "recv started timeout");
        aiui->session_status = AIUI_SESSION_STOPED;
        aiui->connect_status = AIUI_DISCONNECTED;
        return -1;
    }
    return 0;
}
#else
int aiui_start(AIUI_HANDLE handle, AIUI_USE_TYPE type)
{
    if (NULL == handle) return -1;
    aiui_info_t *aiui = (aiui_info_t *)handle;
    aiui_websocket_disconect(aiui->websocket_handle);
    aiui->connect_status = AIUI_DISCONNECTED;
    aiui->type = type;
    aiui->session_status = AIUI_SESSION_STARTING;
    aiui->is_vad_begin = 0;
    aiui->vad_begin_count = 0;
    aiui->vad_end_count = 0;
    aiui->is_process_tts = 0;
    if (aiui_connect(aiui) != 0) {
        return -1;
    }
    int rc = aiui_hal_sem_wait(aiui->sem, 5000);
    if (rc != 0) {
        LOGE(AIUI_TAG, "recv started timeout");
        aiui->session_status = AIUI_SESSION_STOPED;
        aiui->connect_status = AIUI_DISCONNECTED;
        return -1;
    }
    return 0;
}
#endif

int aiui_stop(AIUI_HANDLE handle)
{
    if (NULL == handle) return -1;
    aiui_info_t *aiui = (aiui_info_t *)handle;
    LOGD(AIUI_TAG, "send end %d", aiui->session_status);
    if (aiui->session_status != AIUI_SESSION_STARTED) return -1;
#ifdef AIUI_LONG_CONNECT
    const char *msg = "{\"action\": \"end\"}";
#else
    const char *msg = "--end--";
#endif
    aiui->session_status = AIUI_SESSION_VAD;
    int ret = aiui_websocket_send_text(aiui->websocket_handle, msg, strlen(msg));
    return ret > 0 ? 0 : -1;
}

const char *aiui_get_version()
{
    return "v1.0.0";
}