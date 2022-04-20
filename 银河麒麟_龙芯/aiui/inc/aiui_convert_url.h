
#ifndef __AIUI_CONVERT_URL_H__
#define __AIUI_CONVERT_URL_H__

/**
* 酷我信源激活
* @return 0:激活成功 1:激活失败
*/
int aiui_active();
/**
* 酷我信源音乐的存储方式itemID，需要将itemID通过酷我的API接口来获取url
* @param[in] itemID 用来转换URL 的音乐ID，存放在item_info_t itemID字段中
* @param[in] rate MP3的位率 ，0 ：24kaac 1： 128kmp3，此参数的目的是为了防止有些url不支持128kmp3(实际中还未碰到).
*    存放在item_info_t allrate字段中，如果包含128kmp3,则该参数传1，否则传0
* @return 返回音乐的url
*/
const char *aiui_convert_url(const char *itemID, const int rate);

#endif    // __AIUI_CONVERT_URL_H__
