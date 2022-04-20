#ifndef __AIUI_DEVICE_INFO_H__
#define __AIUI_DEVICE_INFO_H__

// 需要自己实现
static inline const char *aiui_get_auth_id()
{
    return "1ab2dd17e241594e31cb52d7d6bad3d1";
}

/*以下宏在应用上线之前，需要替换成自己的相关信息*/
// appid appkey:在aiui.xfyun.cn  我的应用->应用信息中查询
#define AIUI_APP_ID  "7e90d63e"
#define AIUI_APP_KEY "f9ac9848c9ffce81118ac1bcc803e58f"

// accountkey namespace: 在技能工作室->我的实体->动态密钥中查询
#define AIUI_ACCOUNT_KEY "8b2282b94b9944d69a5b3cd2a408659b"
#define AIUI_NAMESPACE   "OS11933239097"

// authid: 用户唯一ID（32位字符串，包括英文小写字母与数字，开发者需保证该值与终端用户一一对应）
#define AIUI_AUTH_ID aiui_get_auth_id()

//音频采样率
#define AIUI_AUDIO_SAMPLE 16000

// 如果是是按键开启录音，抬起停止录音模式，注释此宏.
//#define AIUI_OPEN_VAD

#endif    // __AIUI_DEVICE_INFO_H__
