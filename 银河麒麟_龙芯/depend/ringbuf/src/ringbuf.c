#include <string.h>
#include "aiui_wrappers_os.h"
#include "aiui_log.h"
#include "ringbuf.h"

#define AIUI_TAG "aiui_ringbuf"
ringbuf_t *init_ringbuf(int size)
{
    ringbuf_t *ringbuf = (ringbuf_t *)aiui_hal_malloc(sizeof(ringbuf_t));
    ringbuf->head = (u8 *)aiui_hal_malloc(size);
    ringbuf->capacity = size;
    ringbuf->read_pos = 0;
    ringbuf->write_pos = 0;
    ringbuf->size = 0;
    return ringbuf;
}

void uninit_ringbuf(ringbuf_t *ringbuf)
{
    if (NULL == ringbuf) return;
    if (ringbuf->head != NULL) {
        aiui_hal_free(ringbuf->head);
        ringbuf->head = NULL;
    }
    aiui_hal_free(ringbuf);
    ringbuf = NULL;
}

int ringbuf_write(ringbuf_t *ringbuf, const void *buf, int len)
{
    if (NULL == ringbuf || len <= 0 || NULL == buf) return 0;
    if (!ringbuf_is_write_able(ringbuf, len)) {
        LOGD(AIUI_TAG, "has no enough space to write");
        return 0;
    }

    if (ringbuf->write_pos + len >= ringbuf->capacity) {
        int space = ringbuf->capacity - ringbuf->write_pos;
        memcpy(ringbuf->head + ringbuf->write_pos, buf, space);
        char *buf_pos = (char *)buf + space;
        int left = len - space;
        memcpy(ringbuf->head, buf_pos, left);
        ringbuf->write_pos = left;

    } else {
        memcpy(ringbuf->head + ringbuf->write_pos, buf, len);
        ringbuf->write_pos += len;
    }
    ringbuf->size += len;
    //LOGD(AIUI_TAG, "write_pos = %d, read_pos = %d", ringbuf->write_pos, ringbuf->read_pos);
    return len;
}

int ringbuf_read(ringbuf_t *ringbuf, void *buf, int len)
{
    if (NULL == ringbuf || len <= 0 || NULL == buf) return 0;

    if (!ringbuf_is_read_able(ringbuf, len)) {
        //LOGD(AIUI_TAG, "has no enough data to read");
        return 0;
    }
    if (ringbuf->read_pos + len >= ringbuf->capacity) {
        int space = ringbuf->capacity - ringbuf->read_pos;
        memcpy(buf, ringbuf->head + ringbuf->read_pos, space);
        char *buf_pos = (char *)buf + space;
        int left = len - space;
        memcpy(buf_pos, ringbuf->head, left);
        ringbuf->read_pos = left;

    } else {
        memcpy(buf, ringbuf->head + ringbuf->read_pos, len);
        ringbuf->read_pos += len;
        //LOGD(AIUI_TAG, "read_pos = %d, len = %d", ringbuf->read_pos, len);
    }
    ringbuf->size -= len;
    return len;
}

bool ringbuf_is_write_able(ringbuf_t *ringbuf, int len)
{
    //LOGD(AIUI_TAG, "size = %d, len = %d", ringbuf->size, len);
    if (ringbuf->capacity - ringbuf->size < len) {
        return false;
    }
    return true;
}

bool ringbuf_is_read_able(ringbuf_t *ringbuf, int len)
{
    //LOGD(AIUI_TAG, "size = %d, len = %d", ringbuf->size, len);
    if (len > ringbuf->size) {
        //LOGD(AIUI_TAG, "has no enough data to read");
        return false;
    }
    return true;
}

void ringbuf_write_update(ringbuf_t *ringbuf, int len)
{
    if (ringbuf_is_write_able(ringbuf, len)) return;
    ringbuf->read_pos =
        (ringbuf->read_pos + (len - (ringbuf->capacity - ringbuf->size))) % ringbuf->capacity;
    ringbuf->size -= (len - (ringbuf->capacity - ringbuf->size));
}

void ringbuf_clear(ringbuf_t *ringbuf)
{
    memset(ringbuf->head, 0, ringbuf->capacity);
    ringbuf->read_pos = 0;
    ringbuf->write_pos = 0;
    ringbuf->size = 0;
}
