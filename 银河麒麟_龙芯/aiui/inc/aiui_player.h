#ifndef __AIUI_PLAYER_H__
#define __AIUI_PLAYER_H__

typedef void *AIUI_PLAYER_HANDLE;

AIUI_PLAYER_HANDLE aiui_media_init();

void aiui_media_uninit(AIUI_PLAYER_HANDLE handle);

void aiui_media_play(AIUI_PLAYER_HANDLE handle, const char *url);

//void aiui_media_tts_play(AIUI_PLAYER_HANDLE handle, const char *url);

void aiui_media_stop(AIUI_PLAYER_HANDLE handle, const char *url);

void aiui_media_resume(AIUI_PLAYER_HANDLE handle, const char *url);

int aiui_media_get_volume(AIUI_PLAYER_HANDLE handle);

void aiui_media_volume_change(AIUI_PLAYER_HANDLE handle, int vol);
#endif    //__AIUI_PLAYER_H__