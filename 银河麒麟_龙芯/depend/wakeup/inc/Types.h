#pragma once

typedef void *CAE_HANDLE;

#define MIDDLE_VERSION  "0.0.1"
#define PROXY_IVW_AUDIO "proxy_ivw_audio"
#define PROXY_CAE_AUDIO "proxy_cae_audio"

//VTN自定义参数
//设置在唤醒前时间段的免唤醒音频
#define SET_PRE_FREE_TIME_MS (2)

//输出唤醒结果和免唤醒音频
#define WAKEUP_ON            (1)

//不输出唤醒结果和免唤醒音频
#define WAKEUP_OFF   	  	 (0)

/*
* 功能：callback(应用层注册到API的接口), 唤醒成功后抛出角度、能力等数值。一期只做降噪，不做唤醒，接口暂时保留
* 参数：
*     pdata        唤醒结果数据，为json格式的字符串[out]
*     len          唤醒结果数据的长度[out]
*     userData     用户回调数据[in]
* 返回值： 无
*/
typedef void (*cae_ivw_fn)(const char* pdata, int len, void *userData);

/*
* 功能：callback(应用层注册到API的接口), 抛出识别音频
* 参数：
*     audioData     识别音频地址[in]
*     audioLen      识别音频字节数[in]
*     userData      用户回调数据[in]
*     param1        预留参数[in]
*     param2        预留参数[in]
* 返回值： 无
*/
typedef void (*cae_audio_fn)(
    const void *audioData, unsigned int audioLen, int param1, const void *param2, void *userData);

/*
* 功能：callback(应用层注册到API的接口), 抛出唤醒音频
* 参数：
*     audioData     唤醒音频地址[in]
*     audioLen      唤醒音频字节数[in]
*     userData      用户回调数据[in]
*     param1        预留参数[in]
*     param2        预留参数[in]
* 返回值： 无
*/
typedef void (*cae_ivw_audio_fn)(
    const void *audioData, unsigned int audioLen, int param1, const void *param2, void *userData);

// proxy层注册到降噪引擎的接口,回写降噪后给唤醒用的音频
typedef void (*proxy_ivw_audio_cb)(void *userdata, void *data, int len);

// proxy层注册到降噪引擎的接口,回写降噪后给识别用的音频
typedef void (*proxy_ace_audio_cb)(void *userdata, void *data, int len);

//  proxy层注册到唤醒引擎，获取性别、年龄检查结果的回调
typedef void(*proxy_cae_gender_age_cb)(void *userdata, const char* result, int len);


/*
* 功能：callback(应用层注册到API的接口), 抛出vpr训练结果
* 参数：
*     status        训练状态[out]: 0 训练成功， 1 训练中， 2 训练失败
*     param1        当前已收录的有效音频的条数[out]
*     param2        预留参数[out]
*     userdata      用户私有数据[in]
* 返回值： 无
*/
typedef void (*vpr_train_res_cb)(int status, int param1, int param2, void* userdata);


//callback(应用层注册到API的接口), 抛出打印日志
typedef void(*log_res_cb)(const char* log, int len);