/**
* @file  aiui_api.h  
* @brief sdk对外接口
* @author    xqli
* @date      2022-3-14 
* @version   v1.0.0
* @copyright iflytek                                                        
*/

#ifndef __AIUI_API_H__
#define __AIUI_API_H__

typedef enum {
    /**
     * @brief 
     * 语音交互和文本交互可以共用一个句柄,
     * 交互和合成目前不能共用一个句柄,
     * 当既有交互和合成时，需要创建两个句柄.
     */
    AIUI_AUDIO_INTERACTIVE = 0,    // aiui语音交互
    AIUI_TEXT_INTERACTIVE,         // aiui文本交互
    AIUI_TTS,                      // 语音合成

    AIUI_MAX
} AIUI_USE_TYPE;

typedef enum {
    AIUI_EVENT_START_SEND = 0,    // 开始写入数据
    AIUI_EVENT_STOP_SEND,         // 停止写入数据,并调用aiui_stop
    AIUI_EVENT_IAT,               // 识别结果
    AIUI_EVENT_ANSWER,            // 语义结果
    AIUI_EVENT_FINISH             // 一轮会话结束
} AIUI_EVENT_TYPE;

typedef void *AIUI_HANDLE;

/**
* 创建句柄aiui句柄
* @param[in] type  交互类型
* @param[in] event 事件回调函数.
* @param[in] param 回调函数私有参数
* @return 成功返回AIUI_API句柄，失败返回NULL
*/
AIUI_HANDLE aiui_init(AIUI_USE_TYPE type,
                      void (*event)(void *param, AIUI_EVENT_TYPE type, const char *msg),
                      void *param);

/**
* 销毁AIUI_HANDLE句柄
* @param[in] handle aiui_init返回句柄
*/
void aiui_uninit(AIUI_HANDLE handle);

/**
* 向云端发送数据
* @param[in] handle aiui_init返回句柄
* @param[in] buf 文本或音频数据内容,文本数据为utf-8类型
* @param[in] len 发送数据长度
* @return : > 0发送的数据长度, -1: 发送失败
*/
int aiui_send(AIUI_HANDLE handle, const char *buf, const int len);

/**
* 开启一轮新的会话
* @param[in] handle aiui_init返回句柄
* @param[in] type 这轮要开启的会话类型
* @return 0:发送成功, -1: 发送失败    
*/
int aiui_start(AIUI_HANDLE handle, AIUI_USE_TYPE type);

/**
* 不再向云端发送数据
* @param[in] handle aiui_init返回句柄
* @return 0:发送成功, -1: 发送失败    
*/
int aiui_stop(AIUI_HANDLE handle);

/**
    获取版本信息
*/
const char *aiui_get_version();
#endif    // __AIUI_H__
