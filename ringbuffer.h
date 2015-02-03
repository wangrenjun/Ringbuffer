/*
 * wangrenjun <wangrj1981@gmail.com>
 * No license
 */

#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#include <sys/uio.h>

typedef struct rb_t{
    unsigned int size;
    unsigned int in;
    unsigned int out;
    unsigned int mask;
    unsigned char buffer[0];
} rb_t;

int rb_init(rb_t **rb, unsigned int size);
void rb_reinit(rb_t *rb);
void rb_deinit(rb_t *rb);
unsigned int rb_gets(rb_t *rb, unsigned char *buf, unsigned int size);
int rb_get_all(rb_t *rb, unsigned char **buf, unsigned int *buf_size);
unsigned int rb_puts(rb_t *rb, const unsigned char *buf, unsigned int size);
unsigned int rb_consumer_peek_at(rb_t *rb, unsigned int offset, unsigned int size, unsigned char **buf);
unsigned int rb_producer_peek_at(rb_t *rb, unsigned int offset, unsigned int size, unsigned char **buf);
#define rb_consumer_peek(rb, size, buf) rb_consumer_peek_at(rb, 0, size, buf)
#define rb_producer_peek(rb, size, buf) rb_producer_peek_at(rb, 0, size, buf)
#define rb_consumed(rb, size)   ((rb)->out += (size))
#define rb_produced(rb, size)   ((rb)->in += (size))
typedef int(*rb_read_pt)(void *, void *, unsigned int);
typedef int(*rb_write_pt)(void *, const void *, unsigned int);
int rb_read(rb_t *rb, rb_read_pt read_cb, void *ptr, unsigned int *read);
int rb_write(rb_t *rb, rb_write_pt write_cb, void *ptr, unsigned int *wrote);
#define rb_size(rb)             ((rb)->size)
#define rb_used_size(rb)        ((rb)->in - (rb)->out)
#define rb_avail_size(rb)       (rb_size(rb) - rb_used_size(rb))
#define rb_is_empty(rb)         ((rb)->in == (rb)->out)
#define rb_is_full(rb)          (rb_used_size(rb) > (rb)->mask)

typedef struct rbvec_t{
    unsigned int max_num;
    unsigned int cnt_bit_offset;
    unsigned int ele_size;
    unsigned int in;
    unsigned int out;
    unsigned int mask;
    rb_t *vec[0];
} rbvec_t;

int rbvec_init(rbvec_t **rbv, unsigned int max_num, unsigned int ele_size);
void rbvec_reinit(rbvec_t *rbv);
void rbvec_deinit(rbvec_t *rbv);
unsigned int rbvec_gets(rbvec_t *rbv, unsigned char *buf, unsigned int size);
int rbvec_get_all(rbvec_t *rbv, unsigned char **buf, unsigned int *buf_size);
unsigned int rbvec_puts(rbvec_t *rbv, const unsigned char *buf, unsigned int size);
int rbvec_consumer_peek_at(rbvec_t *rbv, 
    unsigned int offset, 
    unsigned int size, 
    struct iovec **vecbuf, 
    unsigned int *vecbuf_cnt, 
    unsigned int *vecbuf_size);
int rbvec_producer_peek_at(rbvec_t *rbv, 
    unsigned int offset, 
    unsigned int size, 
    struct iovec **vecbuf, 
    unsigned int *vecbuf_cnt, 
    unsigned int *vecbuf_size);
int rbvec_producer_force_peek_at(rbvec_t *rbv, 
    unsigned int offset, 
    unsigned int size, 
    struct iovec **vecbuf, 
    unsigned int *vecbuf_cnt, 
    unsigned int *vecbuf_size);
#define rbvec_consumer_peek(rbv, size, vecbuf, vecbuf_cnt, vecbuf_size)         \
    rbvec_consumer_peek_at(rbv, 0, size, vecbuf, vecbuf_cnt, vecbuf_size)
#define rbvec_producer_peek(rbv, size, vecbuf, vecbuf_cnt, vecbuf_size)         \
    rbvec_producer_peek_at(rbv, 0, size, vecbuf, vecbuf_cnt, vecbuf_size)
#define rbvec_producer_force_peek(rbv, size, vecbuf, vecbuf_cnt, vecbuf_size)   \
    rbvec_producer_force_peek_at(rbv, 0, size, vecbuf, vecbuf_cnt, vecbuf_size)
unsigned int rbvec_consumed(rbvec_t *rbv, unsigned int size);
unsigned int rbvec_produced(rbvec_t *rbv, unsigned int size);
int rbvec_read(rbvec_t *rbv, rb_read_pt read_cb, void *ptr, unsigned int *read);
int rbvec_write(rbvec_t *rbv, rb_write_pt write_cb, void *ptr, unsigned int *wrote);
#define rbvec_max_num(rbv)      ((rbv)->max_num)
#define rbvec_num(rbv)          (1 << (rbv)->cnt_bit_offset)
#define rbvec_used_num(rbv)     ((rbv)->in - (rbv)->out)
#define rbvec_avail_num(rbv)    (rbvec_num(rbv) - rbvec_used_num(rbv))
#define rbvec_max_size(rbv)     ((rbv)->ele_size * rbvec_max_num(rbv))
#define rbvec_size(rbv)         ((rbv)->ele_size * rbvec_num(rbv))
#define rbvec_used_size(rbv)                    \
    (rbvec_is_empty(rbv) ? 0 :                  \
    (rbvec_is_full(rbv) ? rbvec_max_size(rbv) : \
    ((rbv)->ele_size * rbvec_used_num(rbv) - rb_avail_size((rbv)->vec[(rbv)->out & (rbv)->mask]) + rb_used_size((rbv)->vec[(rbv)->in & (rbv)->mask]))   \
    ))
#define rbvec_avail_size(rbv)   (rbvec_size(rbv) - rbvec_used_size(rbv))
#define rbvec_is_empty(rbv)     ((rbv)->in == (rbv)->out && rb_is_empty((rbv)->vec[(rbv)->in & (rbv)->mask]))
#define rbvec_is_full(rbv)      ((rbvec_used_num(rbv) == rbvec_max_num(rbv))                \
                                && rb_is_full((rbv)->vec[((rbv)->out - 1) & (rbv)->mask])   \
                                && rb_is_full((rbv)->vec[(rbv)->in & (rbv)->mask]))

#endif /* __RINGBUFFER_H__ */
