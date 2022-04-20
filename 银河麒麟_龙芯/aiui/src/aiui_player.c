#include <string.h>
#include <stdio.h>
#include "aiui_log.h"
#include "aiui_wrappers_os.h"
//#define AIUI_SUPPORT_PLAY_URL

#ifndef AIUI_SUPPORT_PLAY_URL
#    include "aiui_list.h"
#    include "aiui_url_download.h"
#    include "aiui_mp3_decode.h"
#    include "alas_api.h"
#endif

#include "aiui_audio_control.h"
#include "aiui_player.h"

#define AIUI_TAG "aiui_player"

#ifndef AIUI_SUPPORT_PLAY_URL
#    define AIUI_MAX_CACHE_SIZE (1024 * 512)
#    define AIUI_MIN_CACHE_SIZE (80 * 150)

typedef struct aiui_player_s
{
    AIUI_URL_DOWNLOAD_HANDLE url_download_handle;
    void *alsa_handle;
    int recv_size;
    AIUI_MP3_DECODE_HANDLE decode_handle;
    aiui_list_t *list_mp3;
    aiui_list_t *list_pcm;
    void *decode_pid;
    void *play_pid;
    void *decode_lock;
    void *decode_sem;
    void *pcm_lock;
    void *pcm_sem;
    bool stop;
    bool play_end;
} aiui_player_t;

static void _aiui_url_info_recv_cb(void *param,
                                   const AIUI_URL_DOWNLOAD_TYPE type,
                                   void *buf,
                                   const int len)
{
    //LOGD(AIUI_TAG, "receive mp3 data");
    if (NULL == param) {
        return;
    }

    aiui_player_t *self = (aiui_player_t *)param;
    switch (type) {
        case AIUI_URL_DOWNLOAD_BEGIN:
            break;
        case AIUI_URL_DOWNLOAD_END:
            aiui_hal_mutex_lock(self->decode_lock);
            aiui_list_push(self->list_mp3, NULL, -1);
            aiui_hal_mutex_unlock(self->decode_lock);
            aiui_hal_sem_post(self->decode_sem);
            break;
        case AIUI_URL_DOWNLOAD_DATA:
            aiui_hal_mutex_lock(self->decode_lock);
            aiui_list_push(self->list_mp3, buf, len);
            aiui_hal_mutex_unlock(self->decode_lock);
            aiui_hal_sem_post(self->decode_sem);
            break;
        default:
            break;
    }
}

static void *_aiui_decode_thread(void *param)
{
    if (NULL == param) {
        return NULL;
    }
    aiui_player_t *self = (aiui_player_t *)param;
    aiui_mp3_deocde(self->decode_handle);
    return NULL;
}

static void *_aiui_play_thread(void *param)
{
    if (NULL == param) {
        return NULL;
    }
    aiui_player_t *self = (aiui_player_t *)param;
    self->play_end = false;
    while (1) {
        aiui_hal_sem_wait(self->pcm_sem, -1);
        if (self->stop) break;
        aiui_hal_mutex_lock(self->pcm_lock);
        aiui_list_t *data = aiui_list_pop(self->list_pcm);
        aiui_hal_mutex_unlock(self->pcm_lock);
        if (NULL == data) {
            continue;
        }
        if (data->len < 0) {
            aiui_hal_mutex_lock(self->pcm_lock);
            aiui_list_node_destroy(data);
            aiui_hal_mutex_unlock(self->pcm_lock);
            break;
        }
        write_alsa_frame(self->alsa_handle, data->len / 2, (short *)data->val);
        aiui_hal_mutex_lock(self->pcm_lock);
        aiui_list_node_destroy(data);
        aiui_hal_mutex_unlock(self->pcm_lock);
    }
    if (!self->stop)
        close_alsa_device(self->alsa_handle, AIUI_ALSA_STOP_DRAIN);
    else
        close_alsa_device(self->alsa_handle, AIUI_ALSA_STOP_DROP);
    self->alsa_handle = NULL;
    self->play_end = true;
    if (!self->stop) aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_PLAY_END, NULL);

    return NULL;
}
static void _aiui_get_mp3_data(void *buf, int *len, void *priv)
{
    aiui_player_t *self = (aiui_player_t *)priv;
    if (NULL == self) return;
    aiui_hal_sem_wait(self->decode_sem, -1);
    aiui_hal_mutex_lock(self->decode_lock);
    aiui_list_t *data = aiui_list_pop(self->list_mp3);
    if (NULL == data) {
        aiui_hal_mutex_unlock(self->decode_lock);
        return;
    }
    *len = data->len;
    if (*len > 0) memcpy(buf, data->val, data->len);
    aiui_list_node_destroy(data);
    aiui_hal_mutex_unlock(self->decode_lock);
}

static void _aiui_output_pcm_data(void *buf, int len, int ch, int sample, void *priv)
{
    aiui_player_t *self = (aiui_player_t *)priv;
    if (NULL == self->alsa_handle) {
        self->alsa_handle = open_alsa_device(AIUI_ALSA_STREAM_PLAY);
        set_alsa_param(self->alsa_handle, 1, sample, 16);
    }
    aiui_hal_mutex_lock(self->pcm_lock);
    aiui_list_push(self->list_pcm, buf, len);
    aiui_hal_mutex_unlock(self->pcm_lock);
    aiui_hal_sem_post(self->pcm_sem);
}
#endif

AIUI_PLAYER_HANDLE aiui_media_init()
{
#ifndef AIUI_SUPPORT_PLAY_URL
    aiui_player_t *handle = (aiui_player_t *)aiui_hal_malloc(sizeof(aiui_player_t));
    handle->url_download_handle = NULL;
    handle->recv_size = 0;
    handle->list_mp3 = NULL;
    handle->list_pcm = NULL;
    handle->decode_pid = NULL;
    handle->decode_lock = aiui_hal_mutex_create();
    handle->decode_sem = aiui_hal_sem_create();
    handle->pcm_lock = aiui_hal_mutex_create();
    handle->pcm_sem = aiui_hal_sem_create();
    handle->decode_handle = aiui_mp3_decode_init(_aiui_get_mp3_data, _aiui_output_pcm_data, handle);
    handle->play_end = true;
    handle->alsa_handle = NULL;
    return (AIUI_PLAYER_HANDLE)handle;
#endif
    return NULL;
}

void aiui_media_uninit(AIUI_PLAYER_HANDLE handle)
{
    aiui_media_stop(handle, NULL);
#ifndef AIUI_SUPPORT_PLAY_URL
    aiui_player_t *self = (aiui_player_t *)handle;
    aiui_mp3_uninit(self->decode_handle);
    aiui_hal_sem_destroy(self->decode_sem);
    aiui_hal_sem_destroy(self->pcm_sem);
    aiui_hal_mutex_destroy(self->decode_lock);
    aiui_hal_mutex_destroy(self->pcm_lock);
    aiui_hal_free(handle);
#endif
    LOGD(AIUI_TAG, "aiui_media_uninit");
}

#ifndef AIUI_SUPPORT_PLAY_URL

#endif

static void _aiui_media_play(AIUI_PLAYER_HANDLE handle, const char *url, int offset)
{
#ifndef AIUI_SUPPORT_PLAY_URL
    if (NULL == handle) {
        return;
    }
    aiui_player_t *self = (aiui_player_t *)handle;
    aiui_media_stop(handle, NULL);
    self->list_mp3 = aiui_list_create();
    self->list_pcm = aiui_list_create();
    self->url_download_handle =
        aiui_url_download_start(url, _aiui_url_info_recv_cb, offset, handle);
    aiui_hal_thread_create(&self->decode_pid, NULL, _aiui_decode_thread, self);
    aiui_hal_thread_create(&self->play_pid, NULL, _aiui_play_thread, self);
    self->stop = false;

#endif
}

void aiui_media_play(AIUI_PLAYER_HANDLE handle, const char *url)
{
    _aiui_media_play(handle, url, 0);
}

void aiui_media_stop(AIUI_PLAYER_HANDLE handle, const char *url)
{
#ifndef AIUI_SUPPORT_PLAY_URL
    if (NULL == handle) {
        return;
    }
    aiui_player_t *self = (aiui_player_t *)handle;
    self->stop = true;
    if (self->decode_handle != NULL) {
        aiui_mp3_stop_decode(self->decode_handle);
    }
    aiui_hal_sem_post(self->decode_sem);
    aiui_hal_sem_post(self->pcm_sem);
    while (!self->play_end) {
        aiui_hal_sleep(10);
    }
    aiui_url_download_stop(self->url_download_handle);
    self->url_download_handle = NULL;
    aiui_list_destroy(self->list_mp3);
    self->list_mp3 = NULL;
    aiui_hal_mutex_lock(self->pcm_lock);
    aiui_list_destroy(self->list_pcm);
    self->list_pcm = NULL;
    aiui_hal_mutex_unlock(self->pcm_lock);
#endif
}

void aiui_media_resume(AIUI_PLAYER_HANDLE handle, const char *url)
{
    _aiui_media_play(handle, url, 0);
}

int aiui_media_get_volume(AIUI_PLAYER_HANDLE handle)
{
    return 50;
}

void aiui_media_volume_change(AIUI_PLAYER_HANDLE handle, int vol) {}
