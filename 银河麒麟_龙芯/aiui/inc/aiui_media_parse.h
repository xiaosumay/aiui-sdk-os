#ifndef __AIUI_MEDIA_PARSE_H__
#define __AIUI_MEDIA_PARSE_H__
#include "cjson.h"

#define AIUI_ITEM_NUM_MAX (10)

typedef struct aiui_item_info_s
{
    char *itemID;
    char *name;
    char *playUrl;
    char *allrate;
} aiui_item_info_t;

typedef struct aiui_playlist_s
{
    int item_pos;
    int item_num;
    aiui_item_info_t item[AIUI_ITEM_NUM_MAX];
} aiui_playlist_t;

char *aiui_parse_tts_url(const char *data);

int aiui_parse_media_info(const cJSON *data);

void aiui_media_free();
aiui_playlist_t *aiui_get_media_info();

void aiui_media_free();
#endif    //__AIUI_MEDIA_PARSE_H__
