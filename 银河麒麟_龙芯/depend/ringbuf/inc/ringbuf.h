#ifndef __RING_BUF_H__
#define __RING_BUF_H__

#include <stdbool.h>
typedef unsigned char u8;

typedef struct ringbuf_s
{
    u8 *head;
    u8 *tail;
    int read_pos;
    int write_pos;
    int size;
    int capacity;
} ringbuf_t;

ringbuf_t *init_ringbuf(int size);

void uninit_ringbuf(ringbuf_t *ringbuf);

int ringbuf_write(ringbuf_t *ringbuf, const void *buf, int len);

int ringbuf_read(ringbuf_t *ringbuf, void *buf, int len);

bool ringbuf_is_write_able(ringbuf_t *ringbuf, int len);

bool ringbuf_is_read_able(ringbuf_t *ringbuf, int len);

void ringbuf_write_update(ringbuf_t *ringbuf, int len);

void ringbuf_clear(ringbuf_t *ringbuf);

#endif    // __RING_BUF_H__
