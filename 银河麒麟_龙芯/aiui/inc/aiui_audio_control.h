#ifndef __AIUI_AUDIO_CONTROL_H__
#define __AIUI_AUDIO_CONTROL_H__

typedef enum aiui_audio_ctrl_cmd_s {
    AIUI_AUDIO_CTRL_TTS = 0,    // 播放TTS
    AIUI_AUDIO_CTRL_PLAY,       // 开始播放命令
    AIUI_AUDIO_CTRL_RESUME,     // 恢复播放命令
    AIUI_AUDIO_CTRL_PAUSE,    // 临时暂停,比如唤醒时会停止播放,使用此枚举,交互结束后会重新尝试播放媒资
    AIUI_AUDIO_CTRL_STOP,     // 停止播放命令,交互结束后,不再重新尝试播放媒资
    AIUI_AUDIO_CTRL_NEXT,    // 下一首命令,交互可以触发,内部也可以触发(当前媒资播放结束,会自动播放下一首)
    AIUI_AUDIO_CTRL_PREV,    // 上一首命令
    AIUI_AUDIO_CTRL_VOLUP,       // 音量增大命令
    AIUI_AUDIO_CTRL_VOLDOWN,     // 音量减小命令
    AIUI_AUDIO_CTRL_VOLVALUE,    // 设置音量命令
    AIUI_AUDIO_CTRL_VOLMAX,      // 音量最大命令
    AIUI_AUDIO_CTRL_VOLMIN,      // 音量最小命令
    AIUI_AUDIO_CTRL_PLAY_END,    // sdk内部无法知道什么时候播放结束,应用收到结束事件时,通过此枚举传入给sdk
    AIUI_AUDIO_CTRL_SET_STATE,    // 设置当前命令,比如上一首,先保存状态,等tts(如果有)播放完毕后,执行当前命令
    AIUI_AUDIO_CTRL_MAX
} aiui_audio_ctrl_cmd_t;

int aiui_audio_ctrl_init();

void aiui_audio_ctrl_deinit();

int aiui_audio_ctrl_cmd(aiui_audio_ctrl_cmd_t cmd, void *param);
#endif    // __AIUI_AUDIO_CONTROL_H__