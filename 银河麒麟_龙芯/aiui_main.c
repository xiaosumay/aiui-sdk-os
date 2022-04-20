#include <stdio.h>
#include <string.h>
#include "alas_api.h"
#include "aiui_log.h"
#include "ringbuf.h"
#include "aiui_wrappers_os.h"
#include "aiui_api.h"
#include "aiui_audio_control.h"
#include "aiui_device_info.h"
#include "CAEAPI.h"

#define ONCE_CAPTURE_POINTS 320
#define RING_BUF_SIZE       (16000 * 2)
#define AIUI_TAG            "aiui_main"

static void *s_snd_handle = NULL;
static AIUI_HANDLE s_aiui_interactive_handle = NULL;
static AIUI_HANDLE s_aiui_tts_handle = NULL;
static bool s_quit = false;
static bool s_start_send = false;
static void *s_wakeup_pid = NULL;
static void *s_capture_pid = NULL;
static void *s_send_pid = NULL;
static void *s_finish_pid = NULL;
static ringbuf_t *s_capture_buf = NULL;
static ringbuf_t *s_iat_buf = NULL;
static void *s_capture_sem = NULL;
static void *s_send_sem = NULL;
static void *s_finish_sem = NULL;
static bool s_test_audio = false;    // 测试音频标记,防止音频文件测试时,发送实时音频给云端
static CAE_HANDLE s_cae_handle = NULL;
static void *finish_result(void *param)
{
    while (1) {
        aiui_hal_sem_wait(s_finish_sem, -1);
        if (s_quit) break;
        aiui_hal_sem_clear(s_capture_sem);
        ringbuf_clear(s_capture_buf);
        //aiui_start(s_aiui_interactive_handle, AIUI_AUDIO_INTERACTIVE);
    }
    LOGD(AIUI_TAG, "finish_result end");
    return NULL;
}

static void on_event(void *param, AIUI_EVENT_TYPE type, const char *msg)
{
    LOGD(AIUI_TAG, "%p %d %s", param, type, msg);
    switch (type) {
        // 收到开始发送音频事件
        case AIUI_EVENT_START_SEND:
            s_start_send = true;
            break;
        // 收到停止发送音频事件
        case AIUI_EVENT_STOP_SEND:
            s_start_send = false;
            aiui_stop(s_aiui_interactive_handle);
            break;
        case AIUI_EVENT_IAT:
            break;
        case AIUI_EVENT_ANSWER:
            break;
        case AIUI_EVENT_FINISH:
            aiui_hal_sem_post(s_finish_sem);
            break;
    }
}

static void on_event_tts(void *param, AIUI_EVENT_TYPE type, const char *msg)
{
    LOGD(AIUI_TAG, "%p %d %s", param, type, msg);
    switch (type) {
        case AIUI_EVENT_START_SEND:
            break;
        case AIUI_EVENT_STOP_SEND:
            break;
        case AIUI_EVENT_IAT:
            break;
        case AIUI_EVENT_ANSWER:
            break;
        case AIUI_EVENT_FINISH:
            break;
    }
}

static FILE *s_file = NULL;
// 唤醒线程
static void *wakeup(void *param)
{
    while (!s_quit) {
        short wakeup_buf[ONCE_CAPTURE_POINTS] = {0};
        short need_buf[ONCE_CAPTURE_POINTS * 2] = {0};    // 1路mic +1路echo
        int rlen;
        rlen = ringbuf_read(s_capture_buf, wakeup_buf, ONCE_CAPTURE_POINTS * 2);
        if (0 == rlen) {
            aiui_hal_sem_clear(s_capture_sem);
            aiui_hal_sem_wait(s_capture_sem, -1);
            continue;
        }
        //mic信号和echo信号交叉排列
        for (int i = 0; i < ONCE_CAPTURE_POINTS * 2; i++) {
            if (i % 2 == 0)
                need_buf[i] = wakeup_buf[i / 2];
            else
                need_buf[i] = 0;
        }
        CAEAudioWrite(s_cae_handle, need_buf, ONCE_CAPTURE_POINTS * 4);
        //LOGD(AIUI_TAG, "cae write");
    }
    LOGD(AIUI_TAG, "wakeup end");
    return NULL;
}

// 实时录音,把音频放到缓存buf里
static void *capture_audio(void *param)
{
    short capture_buf[ONCE_CAPTURE_POINTS] = {0};
    while (!s_quit) {
        int r = read_alsa_frame(s_snd_handle, ONCE_CAPTURE_POINTS, capture_buf);
        //LOGD(AIUI_TAG, "r=%d", r);
        if (!ringbuf_is_write_able(s_capture_buf, ONCE_CAPTURE_POINTS * 2))
            ringbuf_write_update(s_capture_buf, ONCE_CAPTURE_POINTS * 2);
        ringbuf_write(s_capture_buf, capture_buf, r * 2);
        aiui_hal_sem_post(s_capture_sem);
    }
    LOGD(AIUI_TAG, "capture_audio thread quit");
    close_alsa_device(s_snd_handle, AIUI_ALSA_STOP_DROP);
    return NULL;
}

// 从缓存buf里取音频,然后把音频发送到云端
static void *send_audio(void *param)
{
    short send_buf[ONCE_CAPTURE_POINTS] = {0};
    int rlen = 0;
    while (!s_quit) {
        if (!s_start_send || s_test_audio) {
            aiui_hal_sleep(1);
            continue;
        }
        rlen = ringbuf_read(s_iat_buf, send_buf, ONCE_CAPTURE_POINTS * 2);
        if (0 == rlen) {
            aiui_hal_sem_clear(s_send_sem);
            aiui_hal_sem_wait(s_send_sem, -1);
            continue;
        }
        aiui_send(s_aiui_interactive_handle, (const char *)send_buf, ONCE_CAPTURE_POINTS * 2);
    }
    LOGD(AIUI_TAG, "send_audio thread quit");
    return NULL;
}

static const char *s_tts_text = "今天天气怎么样";

/*
*文本交互
*文本内容utf-8格式
*文本交互和语音交互可以共用一个句柄
*不需要等待AIUI_EVENT_START_SEND事件
*/
static void test_text_interactive()
{
    aiui_start(s_aiui_interactive_handle, AIUI_TEXT_INTERACTIVE);
    aiui_send(s_aiui_interactive_handle, s_tts_text, strlen(s_tts_text));
    aiui_stop(s_aiui_interactive_handle);
}

/*
*文本合成
*文本内容utf-8格式
*/
static void test_tts()
{
    aiui_start(s_aiui_tts_handle, AIUI_TTS);
    aiui_send(s_aiui_tts_handle, s_tts_text, strlen(s_tts_text));
    aiui_stop(s_aiui_tts_handle);
}

static void test_audio()
{
    FILE *file = fopen("../audio/test.wav", "r");
    if (NULL == file) {
        LOGE(AIUI_TAG, "no file");
        return;
    }
    fseek(file, 44, SEEK_CUR);
    short send_buf[ONCE_CAPTURE_POINTS] = {0};
    s_test_audio = true;
    aiui_start(s_aiui_interactive_handle, AIUI_AUDIO_INTERACTIVE);
    while (1) {
        int ret = fread(send_buf, 1, ONCE_CAPTURE_POINTS * 2, file);
        if (!feof(file))
            aiui_send(s_aiui_interactive_handle, (const char *)send_buf, ret);
        else
            break;
    }
    aiui_stop(s_aiui_interactive_handle);
    fclose(file);
}

static void IvwResult(const char *result, int len, void *userData)
{
    LOGD(AIUI_TAG, "IvwResult");
    aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_PAUSE, NULL);
    aiui_hal_sem_clear(s_send_sem);
    ringbuf_clear(s_iat_buf);
    LOGD(AIUI_TAG,
         "IvwResult read_pos write_pos size = %d %d %d",
         s_iat_buf->read_pos,
         s_iat_buf->write_pos,
         s_iat_buf->size);
    s_test_audio = false;
    aiui_start(s_aiui_interactive_handle, AIUI_AUDIO_INTERACTIVE);
}
void IvwAudio(
    const void *audioData, unsigned int audioLen, int param1, const void *param2, void *userData)
{
    //LOGD(AIUI_TAG, "IvwAudio");
}

void IatAudio(
    const void *audioData, unsigned int audioLen, int param1, const void *param2, void *userData)
{
    //LOGD(AIUI_TAG, "IatAudio");
    fwrite(audioData, 1, audioLen, s_file);
    if (!ringbuf_is_write_able(s_iat_buf, audioLen)) ringbuf_write_update(s_iat_buf, audioLen);
    ringbuf_write(s_iat_buf, audioData, audioLen);
    aiui_hal_sem_post(s_send_sem);
}
int main(int argc, char **argv)
{
    int ret = CAENew(
        &s_cae_handle, AIUI_AUTH_ID, IvwResult, IvwAudio, IatAudio, "../config/vtn.ini", NULL);
    if (ret != 0) {
        LOGE(AIUI_TAG, "CAE new failed");
        return -1;
    }
    s_file = fopen("wakeup.pcm", "w+");
    s_capture_sem = aiui_hal_sem_create();
    s_send_sem = aiui_hal_sem_create();
    s_finish_sem = aiui_hal_sem_create();
    s_capture_buf = init_ringbuf(RING_BUF_SIZE);
    s_iat_buf = init_ringbuf(RING_BUF_SIZE);
    //创建AIUI交互句柄
    s_aiui_interactive_handle = aiui_init(AIUI_AUDIO_INTERACTIVE, on_event, NULL);
    //创建AIUI合成句柄
    s_aiui_tts_handle = aiui_init(AIUI_TTS, on_event_tts, NULL);
    s_snd_handle = open_alsa_device(AIUI_ALSA_STREAM_CAPTURE);    // 打开录音设备
    set_alsa_param(s_snd_handle, 1, 16000, 16);
    aiui_thread_param_t thread_param;
    thread_param.name = "wakeup";
    thread_param.deatch = 0;
    // 创建线程,实时录音并把音频保存到缓存buf里
    aiui_hal_thread_create(&s_wakeup_pid, &thread_param, wakeup, NULL);
    thread_param.name = "capture_audio";
    // 创建线程,实时录音并把音频保存到缓存buf里
    aiui_hal_thread_create(&s_capture_pid, &thread_param, capture_audio, NULL);
    thread_param.name = "send_audio";
    //创建线程,从缓存buf里取出音频并发送到云端
    aiui_hal_thread_create(&s_send_pid, &thread_param, send_audio, NULL);
    thread_param.name = "finish_result";
    // 创建线程,检测一轮回话是否结束,用来模拟持续交互
    aiui_hal_thread_create(&s_finish_pid, &thread_param, finish_result, NULL);
    while (!s_quit) {
        char in[10] = {};
        printf(
            "小薇小薇语音唤醒"
            "或者输入命令后回车,即可调用相应功能,支持的命令如下:\n"
            "tts:单合成示例,返回合成音频的url.\n"
            "text:文本交互示例,返回语义和合成.\n"
            "audio:音频交互示例,结束后返回识别、语义和合成的结果.\n"
            "real:语音实时交互示例,说话结束后返回识别、语义和合成的结果. \n"
            "q:退出\n");
        scanf("%s", in);

        if (strcmp(in, "tts") == 0) {
            aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_PAUSE, NULL);
            test_tts();    // 测试文本合成功能
        } else if (strcmp(in, "text") == 0) {
            aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_PAUSE, NULL);
            test_text_interactive();    // 测试文本交互功能
        } else if (strcmp(in, "audio") == 0) {
            aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_PAUSE, NULL);
            test_audio();    // 测试音频交互功能
        } else if (strcmp(in, "real") == 0) {
            aiui_audio_ctrl_cmd(AIUI_AUDIO_CTRL_PAUSE, NULL);
            aiui_hal_sem_clear(s_send_sem);
            ringbuf_clear(s_iat_buf);
            s_test_audio = false;
            aiui_start(s_aiui_interactive_handle, AIUI_AUDIO_INTERACTIVE);
        } else if (strcmp(in, "q") == 0) {
            s_quit = true;
            break;
        }
    }
    aiui_hal_sem_post(s_capture_sem);
    aiui_hal_sem_post(s_send_sem);
    aiui_hal_sem_post(s_finish_sem);
    aiui_hal_thread_destroy(s_finish_pid);
    aiui_hal_thread_destroy(s_send_pid);
    aiui_hal_thread_destroy(s_capture_pid);
    aiui_hal_thread_destroy(s_wakeup_pid);
    uninit_ringbuf(s_capture_buf);
    uninit_ringbuf(s_iat_buf);
    aiui_hal_sem_destroy(s_capture_sem);
    aiui_hal_sem_destroy(s_send_sem);
    aiui_hal_sem_destroy(s_finish_sem);
    aiui_uninit(s_aiui_interactive_handle);
    aiui_uninit(s_aiui_tts_handle);
    aiui_print_mem_info();
    fclose(s_file);
    return 0;
}
