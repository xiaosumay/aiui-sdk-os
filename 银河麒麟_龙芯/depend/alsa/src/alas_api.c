#include <alsa/asoundlib.h>
#include <sys/time.h>
#include "alas_api.h"

// Set pcm Parameter function
static snd_pcm_stream_t s_stream_type[] = {SND_PCM_STREAM_PLAYBACK, SND_PCM_STREAM_CAPTURE};

int set_alsa_param(void *handle, int channel, unsigned int frequency, int bitsPerSample)
{
    snd_pcm_t *snd_handle = (snd_pcm_t *)handle;
    snd_pcm_hw_params_t *hdParams = NULL;    // HardWare parameter
    snd_pcm_sw_params_t *swParams = NULL;    // SoftWare parameter

    snd_pcm_hw_params_alloca(&hdParams);

    // Check the parameter
    if (NULL == hdParams) {
        return -1;
    }

    // Check the parameter
    snd_pcm_sw_params_alloca(&swParams);
    if (NULL == swParams) {
        return -1;
    }

    //////////////////////////////////////////////////////////////////////////
    //                            hardware Setting
    //////////////////////////////////////////////////////////////////////////

    // Fill params with a full configuration space for a PCM
    int result = snd_pcm_hw_params_any(snd_handle, hdParams);
    if (0 > result) {
        return -1;
    }

    // Restrict a configuration space to contain only one access type.
    result = snd_pcm_hw_params_set_access(snd_handle, hdParams, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (0 > result) {
        return -1;
    }

    // Restrict a configuration space to contain only one format
    snd_pcm_format_t format = SND_PCM_FORMAT_UNKNOWN;
    switch (bitsPerSample / 8) {
        case 1:
            format = SND_PCM_FORMAT_U8;
            break;
        case 2:
            format = SND_PCM_FORMAT_S16_LE;
            break;

        case 3:
            format = SND_PCM_FORMAT_S24_LE;
            break;
        case 4:
            format = SND_PCM_FORMAT_S32_LE;
            break;

        default:
            format = SND_PCM_FORMAT_S16_LE;
            break;
    }

    if (SND_PCM_FORMAT_UNKNOWN == format) {
        return -1;
    }

    result = snd_pcm_hw_params_set_format(snd_handle, hdParams, format);
    if (0 > result) {
        return -1;
    }

    result = snd_pcm_hw_params_set_channels(snd_handle, hdParams, channel);
    if (0 > result) {
        return -1;
    }

    unsigned int rate = frequency;
    // Restrict a configuration space to have rate nearest to a target
    result = snd_pcm_hw_params_set_rate_near(snd_handle, hdParams, &rate, 0);
    if (0 > result) {
        return -1;
    }

    // Check the rate
    if (rate != frequency) {
        return -1;
    }

    // Extract maximum buffer time from a configuration space
    unsigned int bufferTime = 0;
    result = snd_pcm_hw_params_get_buffer_time_max(hdParams, &bufferTime, 0);
    if (0 > result) {
        return -1;
    }

    if (bufferTime > 500000) {
        bufferTime = 500000;
    }

    unsigned int periodTime;
    periodTime = bufferTime / 4;

    // Restrict a configuration space to have period time nearest to a target
    result = snd_pcm_hw_params_set_period_time_near(snd_handle, hdParams, &periodTime, 0);
    if (0 > result) {
        return -1;
    }

    // Restrict a configuration space to have buffer time nearest to a target
    result = snd_pcm_hw_params_set_buffer_time_near(snd_handle, hdParams, &bufferTime, 0);
    if (0 > result) {
        return -1;
    }

    // Install one PCM hardware configuration
    result = snd_pcm_hw_params(snd_handle, hdParams);
    if (0 > result) {
        return -1;
    }

    // Extract buffer size from a configuration space
    snd_pcm_uframes_t bufferSize = 0;
    result = snd_pcm_hw_params_get_buffer_size(hdParams, &bufferSize);
    if (0 > result) {
        return -1;
    }

    // Extract period size from a configuration space
    snd_pcm_uframes_t periodFrames;
    result = snd_pcm_hw_params_get_period_size(hdParams, &periodFrames, 0);
    if (0 > result) {
        return -1;
    }

    // Check the buffer size is equal to period size or not
    if (bufferSize == periodFrames) {
        return -1;
    }
    //////////////////////////////////////////////////////////////////////////
    //                            Software setting
    //////////////////////////////////////////////////////////////////////////

    // Return current software configuration for a PCM
    result = snd_pcm_sw_params_current(snd_handle, swParams);
    if (0 > result) {
        return -1;
    }

    // Set start threshold inside a software configuration container
    result = snd_pcm_sw_params_set_start_threshold(snd_handle, swParams, bufferSize);
    if (0 > result) {
        return -1;
    }

    // Set stop threshold inside a software configuration container
    result = snd_pcm_sw_params_set_stop_threshold(snd_handle, swParams, bufferSize);
    if (0 > result) {
        return -1;
    }

    // Set avail min inside a software configuration container
    result = snd_pcm_sw_params_set_avail_min(snd_handle, swParams, periodFrames);
    if (0 > result) {
        return -1;
    }

    // Install PCM software configuration defined by params
    result = snd_pcm_sw_params(snd_handle, swParams);
    if (0 > result) {
        return -1;
    }
    result = snd_pcm_start(snd_handle);
    if (0 > result) {
        return -1;
    }
    snd_pcm_prepare(snd_handle);
    return 0;
}

//init and open alsa device
void *open_alsa_device(AIUI_ALSA_STREAM_MODE mode)
{
    snd_pcm_t *handle = NULL;
    int result = snd_pcm_open(&handle, "default", s_stream_type[mode], 0);
    if (result < 0) {
        return NULL;
    }
    return (void *)handle;
}

//close alsa device
void close_alsa_device(void *handle, AIUI_ALSA_STOP_MODE mode)
{
    snd_pcm_t *snd_handle = (snd_pcm_t *)handle;
    if (NULL == snd_handle) {
        return;
    }
    if (AIUI_ALSA_STOP_DRAIN == mode) {
        snd_pcm_drain(snd_handle);
    } else {
        snd_pcm_drop(snd_handle);
    }
    snd_pcm_close(snd_handle);
}

/* I/O error handler */
static void xrun(snd_pcm_t *handle, snd_pcm_stream_t stream)
{
    snd_pcm_t *snd_handle = (snd_pcm_t *)handle;
    snd_pcm_status_t *status;
    int res;

    snd_pcm_status_alloca(&status);
    if ((res = snd_pcm_status(snd_handle, status)) < 0) {
        printf("status error: %s\n", snd_strerror(res));
        exit(EXIT_FAILURE);
    }
    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
        struct timeval now, diff, tstamp;
        gettimeofday(&now, 0);
        snd_pcm_status_get_trigger_tstamp(status, &tstamp);
        timersub(&now, &tstamp, &diff);
        fprintf(stderr,
                "%s!!! (at least %.3f ms long)\n",
                stream == SND_PCM_STREAM_PLAYBACK ? "underrun" : "overrun",
                diff.tv_sec * 1000 + diff.tv_usec / 1000.0);

        if ((res = snd_pcm_prepare(snd_handle)) < 0) {
            printf("xrun: prepare error: %s\n", snd_strerror(res));
            exit(EXIT_FAILURE);
        }
        return; /* ok, data should be accepted again */
    }
    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
        printf("draining!!!\n");
        if (stream == SND_PCM_STREAM_CAPTURE) {
            fprintf(stderr, "capture stream format change? attempting recover...\n");
            if ((res = snd_pcm_prepare(snd_handle)) < 0) {
                printf("xrun(DRAINING): prepare error: %s", snd_strerror(res));
                exit(EXIT_FAILURE);
            }
            return;
        }
    }
    exit(EXIT_FAILURE);
}

// get alsa frame
int read_alsa_frame(void *handle, unsigned long samples_num, short *samples)
{
    snd_pcm_t *snd_handle = (snd_pcm_t *)handle;
    ssize_t r = snd_pcm_readi(snd_handle, samples, samples_num);
    if (r == -EAGAIN) {
        snd_pcm_wait(snd_handle, 1000);
    } else if (r == -EPIPE) {
        xrun(snd_handle, SND_PCM_STREAM_CAPTURE);
    } else if (r < 0) {
        printf("read error: %s", snd_strerror(r));
        exit(EXIT_FAILURE);
    }

    return r;
}

int write_alsa_frame(void *handle, unsigned long samples_num, short *samples)
{
    snd_pcm_t *snd_handle = (snd_pcm_t *)handle;
    if (NULL == snd_handle) return -1;
    ssize_t rc = snd_pcm_writei(snd_handle, samples, samples_num);
    if (rc == -EPIPE) {
        /* EPIPE means underrun */
        fprintf(stderr, "underrun occurred\n");
        snd_pcm_prepare(snd_handle);
    } else if (rc < 0) {
        fprintf(stderr, "error from writei: %s\n", snd_strerror(rc));
    }
    return 0;
}
