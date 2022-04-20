#ifndef __AIUI_MP3_DECODE_H__
#define __AIUI_MP3_DECODE_H__

typedef void *AIUI_MP3_DECODE_HANDLE;
typedef void (*aiui_decode_get_cb)(void *buf, int *len, void *priv);
typedef void (*aiui_decode_put_cb)(void *buf, int len, int ch, int sample, void *priv);

AIUI_MP3_DECODE_HANDLE aiui_mp3_decode_init(aiui_decode_get_cb get,
                                            aiui_decode_put_cb put,
                                            void *priv);
int aiui_mp3_deocde(AIUI_MP3_DECODE_HANDLE handle);

void aiui_mp3_stop_decode(AIUI_MP3_DECODE_HANDLE handle);

void aiui_mp3_uninit(AIUI_MP3_DECODE_HANDLE handle);
#endif    // __AIUI_MP3_DECODE_H__