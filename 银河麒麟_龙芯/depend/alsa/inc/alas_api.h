#ifndef __ALSA_API_H__
#define __ALSA_API_H__

typedef enum { AIUI_ALSA_STREAM_PLAY = 0, AIUI_ALSA_STREAM_CAPTURE } AIUI_ALSA_STREAM_MODE;
typedef enum { AIUI_ALSA_STOP_DRAIN = 0, AIUI_ALSA_STOP_DROP } AIUI_ALSA_STOP_MODE;
//init and open alsa device
void *open_alsa_device(AIUI_ALSA_STREAM_MODE mode);

//close alsa device
void close_alsa_device(void *handle, AIUI_ALSA_STOP_MODE mode);

int set_alsa_param(void *handle, int channel, unsigned int frequency, int bitsPerSample);
// get alsa frame
int read_alsa_frame(void *handle, unsigned long samples_num, short *samples);

int write_alsa_frame(void *handle, unsigned long samples_num, short *samples);

#endif    // __ALSA_API_H__