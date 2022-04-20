#include <string.h>
#include "mad.h"
#include "aiui_log.h"
#include "aiui_wrappers_os.h"
#include "aiui_mp3_decode.h"

#define AIUI_TAG "mp3_decode"
#define BUFSIZE  8192

typedef struct mp3_buffer_s
{
    unsigned char buf[BUFSIZE];
    int len;
} mp3_buffer_t;

typedef struct aiui_mp3_decode_s
{
    mp3_buffer_t buffer;
    struct mad_decoder decoder;
    aiui_decode_get_cb get;
    aiui_decode_put_cb put;
    void *cb_param;
    bool stop;
} aiui_mp3_decode_t;

static enum mad_flow _aiui_mp3_input(void *data, struct mad_stream *stream)
{
    aiui_mp3_decode_t *self = (aiui_mp3_decode_t *)data;
    if (self->stop) return MAD_FLOW_STOP;
    mp3_buffer_t *buffer = &self->buffer;
    int unproc_data_size = stream->bufend - stream->next_frame;
    //LOGD(AIUI_TAG, "unproc size = %d", unproc_data_size);
    memcpy(buffer->buf, buffer->buf + (buffer->len - unproc_data_size), unproc_data_size);
    int len = 0;
    self->get(buffer->buf + unproc_data_size, &len, self->cb_param);
    buffer->len = len + unproc_data_size;
    if (len < 0) {
        self->put(NULL, -1, 1, 16000, self->cb_param);
        return MAD_FLOW_STOP;
    }
    mad_stream_buffer(stream, buffer->buf, buffer->len);
    return MAD_FLOW_CONTINUE;
}

signed int scale(mad_fixed_t sample)
{
    /* round */
    sample += (1L << (MAD_F_FRACBITS - 16));

    /* clip */
    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    /* quantize */
    return sample >> (MAD_F_FRACBITS + 1 - 16);
}
static enum mad_flow _aiui_mp3_output(void *data,
                                      struct mad_header const *header,
                                      struct mad_pcm *pcm)
{
    aiui_mp3_decode_t *self = (aiui_mp3_decode_t *)data;
    if (NULL == self) return MAD_FLOW_STOP;
    mad_fixed_t *left_ch;

    left_ch = pcm->samples[0];
    short buf[1152] = {0};
    for (int i = 0; i < pcm->length; i++) {
        mad_fixed_t sample = scale(*left_ch++);
        buf[i] = sample & 0xffff;
    }
    self->put(buf, pcm->length * 2, 1, pcm->samplerate, self->cb_param);
    return MAD_FLOW_CONTINUE;
}

AIUI_MP3_DECODE_HANDLE aiui_mp3_decode_init(aiui_decode_get_cb get,
                                            aiui_decode_put_cb put,
                                            void *priv)
{
    aiui_mp3_decode_t *handle = (aiui_mp3_decode_t *)aiui_hal_malloc(sizeof(aiui_mp3_decode_t));
    handle->get = get;
    handle->put = put;
    handle->cb_param = priv;
    handle->stop = false;
    LOGE(AIUI_TAG, "handle->stop = %d", handle->stop);
    mad_decoder_init(&handle->decoder,
                     &handle->buffer,
                     _aiui_mp3_input,
                     NULL,
                     NULL,
                     _aiui_mp3_output,
                     NULL,
                     NULL);
    return (void *)handle;
}

int aiui_mp3_deocde(AIUI_MP3_DECODE_HANDLE handle)
{
    aiui_mp3_decode_t *self = (aiui_mp3_decode_t *)handle;
    self->stop = false;
    LOGD(AIUI_TAG, "decoder1 = %d", self->stop);
    int result = mad_decoder_run(&self->decoder, MAD_DECODER_MODE_SYNC);
    LOGD(AIUI_TAG, "decoder2 = %p", &self->decoder);

    return result;
}

void aiui_mp3_stop_decode(AIUI_MP3_DECODE_HANDLE handle)
{
    LOGD(AIUI_TAG, "aiui_mp3_stop_decode");
    aiui_mp3_decode_t *self = (aiui_mp3_decode_t *)handle;
    self->stop = true;
}

void aiui_mp3_uninit(AIUI_MP3_DECODE_HANDLE handle)
{
    aiui_mp3_decode_t *self = (aiui_mp3_decode_t *)handle;
    mad_decoder_finish(&self->decoder);
    LOGE(AIUI_TAG, "mad_decoder_finish = %p", &self->decoder);
    aiui_hal_free(handle);
}