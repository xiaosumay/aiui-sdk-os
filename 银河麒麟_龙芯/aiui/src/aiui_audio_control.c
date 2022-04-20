#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "aiui_log.h"
#include "aiui_media_parse.h"
#include "aiui_wrappers_os.h"
#include "aiui_convert_url.h"
#include "aiui_player.h"
#include "aiui_audio_control.h"

#define AIUI_TAG "aiui_audio"

#define AIUI_VOLUME_STEP      10
#define AIUI_MIN_VOLUME_VALUE 0
#define AIUI_MAX_VOLUME_VALUE 100

typedef enum { AIUI_URL_TTS = 0, AIUI_URL_MEDIA } aiui_url_type_t;

typedef void (*pfunc)(void *param);

typedef struct aiui_audio_func_map_s
{
    aiui_audio_ctrl_cmd_t cmd;
    pfunc func;
} aiui_audio_func_map_t;

static aiui_audio_ctrl_cmd_t s_expected_cmd = AIUI_AUDIO_CTRL_MAX;
static bool s_is_resume = true;
/*标记当前播放的时tts还是媒资,用来处理url播放完毕后的逻辑：媒资播放完毕后,播放下一首
tts播放完毕后,尝试恢复播放*/
static aiui_url_type_t s_url_type = AIUI_URL_TTS;

static AIUI_PLAYER_HANDLE s_aiui_player_handle = NULL;
static void _aiui_audio_tts(void *param)
{
    if (NULL == param) {
        aiui_audio_ctrl_cmd(s_expected_cmd, NULL);
    } else {
        char *url = (char *)param;
        s_url_type = AIUI_URL_TTS;
        LOGD(AIUI_TAG, "url = %s", url);
        aiui_media_play(s_aiui_player_handle, url);
    }
}

static void _aiui_audio_play(void *param)
{
    if (aiui_get_media_info()->item_num == 0) return;
    aiui_get_media_info()->item_pos = 1;
    LOGD(AIUI_TAG, "play %s", aiui_get_media_info()->item[0].name);
    s_is_resume = true;
    char *url = aiui_get_media_info()->item[0].playUrl;
    if (NULL == url) {
        int rate = 0;
        if (strstr(aiui_get_media_info()->item[aiui_get_media_info()->item_pos - 1].allrate,
                   "128kmp3") != NULL)
            rate = 1;

        url = (char *)aiui_convert_url(aiui_get_media_info()->item[0].itemID, rate);
    }
    if (NULL == url) {
        return;
    }
    s_url_type = AIUI_URL_MEDIA;
    aiui_media_play(s_aiui_player_handle, url);
    s_expected_cmd = AIUI_AUDIO_CTRL_RESUME;
    if (NULL == aiui_get_media_info()->item[0].playUrl) aiui_hal_free(url);
}

static void _aiui_audio_resume(void *param)
{
    LOGD(AIUI_TAG, "resume=%d", s_is_resume);
    if (aiui_get_media_info()->item_num == 0) return;
    if (!s_is_resume) return;
    s_url_type = AIUI_URL_MEDIA;

    int pos = aiui_get_media_info()->item_pos - 1;
    LOGD(AIUI_TAG, "play %s", aiui_get_media_info()->item[pos].name);
    s_is_resume = true;
    char *url = aiui_get_media_info()->item[pos].playUrl;
    if (NULL == url) {
        int rate = 0;
        if (strstr(aiui_get_media_info()->item[pos].allrate, "128kmp3") != NULL) rate = 1;

        url = (char *)aiui_convert_url(aiui_get_media_info()->item[pos].itemID, rate);
    }
    if (NULL == url) {
        return;
    }
    s_url_type = AIUI_URL_MEDIA;
    s_expected_cmd = AIUI_AUDIO_CTRL_RESUME;
    aiui_media_resume(s_aiui_player_handle, url);
    if (NULL == aiui_get_media_info()->item[pos].playUrl) aiui_hal_free(url);
}

static void _aiui_audio_stop(void *param)
{
    s_is_resume = false;
    aiui_media_stop(s_aiui_player_handle, NULL);
}

static void _aiui_audio_pause(void *param)
{
    aiui_media_stop(s_aiui_player_handle, NULL);
}

static void _aiui_audio_next(void *param)
{
    if (aiui_get_media_info()->item_num == 0) return;
    /* 列表最后一曲，重复播放最后一首*/
    if (aiui_get_media_info()->item_num > aiui_get_media_info()->item_pos)
        aiui_get_media_info()->item_pos += 1;
    LOGD(
        AIUI_TAG, "play %s", aiui_get_media_info()->item[aiui_get_media_info()->item_pos - 1].name);
    s_is_resume = true;
    char *url = aiui_get_media_info()->item[aiui_get_media_info()->item_pos - 1].playUrl;
    if (NULL == url) {
        int rate = 0;
        if (strstr(aiui_get_media_info()->item[aiui_get_media_info()->item_pos - 1].allrate,
                   "128kmp3") != NULL)
            rate = 1;
        url = (char *)aiui_convert_url(
            aiui_get_media_info()->item[aiui_get_media_info()->item_pos - 1].itemID, rate);
    }
    if (NULL == url) {
        return;
    }
    LOGD(AIUI_TAG,
         "play %s, url:%s",
         aiui_get_media_info()->item[aiui_get_media_info()->item_pos - 1].name,
         url);
    s_url_type = AIUI_URL_MEDIA;
    aiui_media_play(s_aiui_player_handle, url);
    s_expected_cmd = AIUI_AUDIO_CTRL_RESUME;
    if (NULL == aiui_get_media_info()->item[aiui_get_media_info()->item_pos - 1].playUrl)
        aiui_hal_free(url);
}

static void _aiui_audio_prev(void *param)
{
    if (aiui_get_media_info()->item_num == 0) return;
    /* 列表第一曲的情况就重复播放第一曲 */
    if (aiui_get_media_info()->item_pos > 1 && aiui_get_media_info()->item_num > 0)
        aiui_get_media_info()->item_pos -= 1;
    s_is_resume = true;
    char *url = aiui_get_media_info()->item[aiui_get_media_info()->item_pos - 1].playUrl;
    if (NULL == url) {
        int rate = 0;
        if (strstr(aiui_get_media_info()->item[aiui_get_media_info()->item_pos - 1].allrate,
                   "128kmp3") != NULL)
            rate = 1;
        url = (char *)aiui_convert_url(
            aiui_get_media_info()->item[aiui_get_media_info()->item_pos - 1].itemID, rate);
    }
    if (NULL == url) {
        return;
    }
    LOGD(AIUI_TAG,
         "play %s, url:%s",
         aiui_get_media_info()->item[aiui_get_media_info()->item_pos - 1].name,
         url);
    s_url_type = AIUI_URL_MEDIA;
    aiui_media_play(s_aiui_player_handle, url);
    s_expected_cmd = AIUI_AUDIO_CTRL_RESUME;
    if (NULL == aiui_get_media_info()->item[aiui_get_media_info()->item_pos - 1].playUrl)
        aiui_hal_free(url);
}

static void _aiui_audio_volup(void *param)
{
    int vol = aiui_media_get_volume(s_aiui_player_handle);
    vol += AIUI_VOLUME_STEP;
    aiui_media_volume_change(s_aiui_player_handle, vol);
}

static void _aiui_audio_voldown(void *param)
{
    int vol = aiui_media_get_volume(s_aiui_player_handle);
    vol -= AIUI_VOLUME_STEP;
    aiui_media_volume_change(s_aiui_player_handle, vol);
}

static void _aiui_audio_volvalue(void *param)
{
    int vol = atoi((const char *)param);
    aiui_media_volume_change(s_aiui_player_handle, vol);
}

static void _aiui_audio_volmax(void *param)
{
    aiui_media_volume_change(s_aiui_player_handle, AIUI_MAX_VOLUME_VALUE);
}

static void _aiui_audio_volmin(void *param)
{
    aiui_media_volume_change(s_aiui_player_handle, AIUI_MIN_VOLUME_VALUE);
}

static void _aiui_audio_play_end(void *param)
{
    LOGD(AIUI_TAG, "play end %d %d", s_expected_cmd, s_url_type);
    if (AIUI_URL_TTS == s_url_type)
        aiui_audio_ctrl_cmd(s_expected_cmd, NULL);
    else
        aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_NEXT, NULL);
}
static void _aiui_audio_set_state(void *param)
{
    aiui_audio_ctrl_cmd_t cmd = *(aiui_audio_ctrl_cmd_t *)param;
    LOGD(AIUI_TAG, "cur cmd = %d", cmd);
    if (cmd == AIUI_AUDIO_CTRL_RESUME) s_is_resume = true;
    s_expected_cmd = cmd;
}
static aiui_audio_func_map_t s_audio_func[] = {{AIUI_AUDIO_CTRL_TTS, _aiui_audio_tts},
                                               {AIUI_AUDIO_CTRL_PLAY, _aiui_audio_play},
                                               {AIUI_AUDIO_CTRL_RESUME, _aiui_audio_resume},
                                               {AIUI_AUDIO_CTRL_PAUSE, _aiui_audio_pause},
                                               {AIUI_AUDIO_CTRL_STOP, _aiui_audio_stop},
                                               {AIUI_AUDIO_CTRL_NEXT, _aiui_audio_next},
                                               {AIUI_AUDIO_CTRL_PREV, _aiui_audio_prev},
                                               {AIUI_AUDIO_CTRL_VOLUP, _aiui_audio_volup},
                                               {AIUI_AUDIO_CTRL_VOLDOWN, _aiui_audio_voldown},
                                               {AIUI_AUDIO_CTRL_VOLVALUE, _aiui_audio_volvalue},
                                               {AIUI_AUDIO_CTRL_VOLMAX, _aiui_audio_volmax},
                                               {AIUI_AUDIO_CTRL_VOLMIN, _aiui_audio_volmin},
                                               {AIUI_AUDIO_CTRL_PLAY_END, _aiui_audio_play_end},
                                               {AIUI_AUDIO_CTRL_SET_STATE, _aiui_audio_set_state}};
int aiui_audio_ctrl_cmd(aiui_audio_ctrl_cmd_t cmd, void *param)
{
    for (size_t i = 0; i < sizeof(s_audio_func) / sizeof(s_audio_func[0]); i++) {
        if (s_audio_func[i].cmd == cmd) {
            s_audio_func[i].func(param);
            break;
        }
    }
    return 0;
}

int aiui_audio_ctrl_init()
{
    // 播放器只初始化一次
    if (s_aiui_player_handle != NULL) return 0;
    s_aiui_player_handle = aiui_media_init();
    LOGE(AIUI_TAG, "player handle = %p", s_aiui_player_handle);
    return 0;
}

void aiui_audio_ctrl_deinit()
{
    aiui_media_free();
    if (NULL == s_aiui_player_handle) return;
    aiui_media_uninit(s_aiui_player_handle);
    s_aiui_player_handle = NULL;
}
