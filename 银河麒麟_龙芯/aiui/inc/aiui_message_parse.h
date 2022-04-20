#ifndef __AIUI_MESSAGE_PARSE_H__
#define __AIUI_MESSAGE_PARSE_H__

typedef enum {
    AIUI_MESSAGE_CONNECTED = 0,
    AIUI_MESSAGE_STARTED,
    AIUI_MESSAGE_VAD,
    AIUI_MESSAGE_FINISH,
    AIUI_MESSAGE_ERROR,
    AIUI_MESSAGE_IAT,
    AIUI_MESSAGE_ANSWER,
    AIUI_MESSAGE_TTS,

    AIUI_MESSAGE_MAX
} AIUI_MESSAGE_TYPE;

typedef void *AIUI_MESSAGE_PARSE_HANDLE;
typedef void (*aiui_message_parse_cb)(void *param, AIUI_MESSAGE_TYPE type, const char *msg);

AIUI_MESSAGE_PARSE_HANDLE aiui_message_parse_init(aiui_message_parse_cb cb, void *param);

int aiui_message_parse(AIUI_MESSAGE_PARSE_HANDLE handle, const char *msg, const int len);

void aiui_message_parse_uninit(AIUI_MESSAGE_PARSE_HANDLE handle);
#endif    // __AIUI_MESSAGE_PARSE_H__
