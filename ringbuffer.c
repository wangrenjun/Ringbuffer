/*
 * wangrenjun <wangrj1981@gmail.com>
 * No license
 */

/* inspired by linux kfifo */

#include "ringbuffer.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif

static int is_power_of_2(unsigned long n);

int rb_init(rb_t **rb, unsigned int size)
{
    rb_t *_rb;

    /* alloc_size must be a power of 2 */
    if (!is_power_of_2(size))
        return -EINVAL;
    _rb = (rb_t *)malloc(sizeof(*_rb) + size);
    if (!_rb)
        return -ENOMEM;
    _rb->size = size;
    _rb->in = _rb->out = 0;
    _rb->mask = size - 1;
    *rb = _rb;

    return 0;
}

void rb_reinit(rb_t *rb)
{
    rb->in = rb->out = 0;
}

void rb_deinit(rb_t *rb)
{
    free(rb);
}

/* Not Zerocopy */
unsigned int rb_gets(rb_t *rb, unsigned char *buf, unsigned int size)
{
    unsigned int used_size, roll_size, s;

    if (rb_is_empty(rb))
        return 0;
    used_size = rb->in - rb->out;
    roll_size = rb->size - (rb->out & rb->mask);
    size = min(size, used_size);
    s = min(size, roll_size);
    memcpy(buf, rb->buffer + (rb->out & rb->mask), s);
    memcpy(buf + s, rb->buffer, size - s);
    rb->out += size;

    return size;
}

/* Not Zerocopy */
int rb_get_all(rb_t *rb, unsigned char **buf, unsigned int *buf_size)
{
    unsigned char *_buf;
    unsigned int size, used_size, roll_size, s;

    if (rb_is_empty(rb))
        return 0;
    size = rb->size;
    used_size = rb->in - rb->out;
    roll_size = rb->size - (rb->out & rb->mask);
    size = min(size, used_size);
    _buf = (unsigned char *)malloc(size);
    if (!_buf)
        return -ENOMEM;
    s = min(size, roll_size);
    memcpy(_buf, rb->buffer + (rb->out & rb->mask), s);
    memcpy(_buf + s, rb->buffer, size - s);
    rb->out += size;
    *buf = _buf;
    *buf_size = size;

    return 0;
}

/* Not Zerocopy */
unsigned int rb_puts(rb_t *rb, const unsigned char *buf, unsigned int size)
{
    unsigned int avail_size, roll_size, s;

    if (rb_is_full(rb))
        return 0;
    avail_size = rb->size - rb->in + rb->out;
    roll_size = rb->size - (rb->in & rb->mask);
    size = min(size, avail_size);
    s = min(size, roll_size);
    memcpy(rb->buffer + (rb->in & rb->mask), buf, s);
    memcpy(rb->buffer, buf + s, size - s);
    rb->in += size;

    return size;
}

unsigned int rb_consumer_peek_at(rb_t *rb, unsigned int offset, unsigned int size, unsigned char **buf)
{
    unsigned int offset_out, used_size, roll_size, s;

    if (rb_is_empty(rb) || offset >= rb_used_size(rb))
        return 0;
    offset_out = rb->out + offset;
    used_size = rb->in - offset_out;
    roll_size = rb->size - (offset_out & rb->mask);
    size = min(size, used_size);
    s = min(size, roll_size);
    *buf = rb->buffer + (offset_out & rb->mask);

    return s;
}

unsigned int rb_producer_peek_at(rb_t *rb, unsigned int offset, unsigned int size, unsigned char **buf)
{
    unsigned int offset_in, avail_size, roll_size, s;

    if (rb_is_full(rb) || offset >= rb_avail_size(rb))
        return 0;
    offset_in = rb->in + offset;
    avail_size = rb->size - offset_in + rb->out;
    roll_size = rb->size - (offset_in & rb->mask);
    size = min(size, avail_size);
    s = min(size, roll_size);
    *buf = rb->buffer + (offset_in & rb->mask);

    return s;
}

int rb_read(rb_t *rb, rb_read_pt read_cb, void *ptr, unsigned int *read)
{
    unsigned char *buf;
    unsigned int size;
    int rv;

    do {
        rv = 0;
        size = rb_producer_peek(rb, rb->size, &buf);
        if (size) {
            rv = read_cb(ptr, buf, size);
            if (rv > 0) {
                rb_produced(rb, (unsigned int)rv);
                *read += (unsigned int)rv;
            }
        }
    } while (rv > 0 && (unsigned int)rv == size);

    return rv;
}

int rb_write(rb_t *rb, rb_write_pt write_cb, void *ptr, unsigned int *wrote)
{
    unsigned char *buf;
    unsigned int size;
    int rv;

    do {
        rv = 0;
        size = rb_consumer_peek(rb, rb->size, &buf);
        if (size) {
            rv = write_cb(ptr, buf, size);
            if (rv > 0) {
                rb_consumed(rb, (unsigned int)rv);
                *wrote += (unsigned int)rv;
            }
        }
    } while (rv > 0 && (unsigned int)rv == size);

    return rv;
}

static int is_power_of_2(unsigned long n)
{
    return (n != 0 && ((n & (n - 1)) == 0));
}

int rbvec_init(rbvec_t **rbv, unsigned int max_num, unsigned int ele_size)
{
    rb_t *rb;
    rbvec_t *_rbv;
    int rv;

    /* alloc_size must be a power of 2 */
    if (!is_power_of_2(max_num))
        return -EINVAL;
    rv = rb_init(&rb, ele_size);
    if (rv < 0)
        return rv;
    _rbv = (rbvec_t *)malloc(sizeof(*_rbv) + sizeof(void *) * max_num);
    if (_rbv == NULL)
        return -ENOMEM;
    _rbv->max_num = max_num;
    _rbv->cnt_bit_offset = 0;
    _rbv->ele_size = ele_size;
    _rbv->in = _rbv->out = 0;
    _rbv->mask = rbvec_num(_rbv) - 1;
    _rbv->vec[0] = rb;
    *rbv = _rbv;

    return 0;
}

void rbvec_reinit(rbvec_t *rbv)
{
    unsigned int i;

    for (i = 0; i < rbvec_num(rbv); i++)
        rb_reinit(rbv->vec[i]);
    rbv->in = rbv->out = 0;
}

void rbvec_deinit(rbvec_t *rbv)
{
    unsigned int i;

    for (i = 0; i < rbvec_num(rbv); i++)
        rb_deinit(rbv->vec[i]);
    free(rbv);
}

unsigned int rbvec_gets(rbvec_t *rbv, unsigned char *buf, unsigned int size)
{
    struct iovec *vecbuf = NULL;
    unsigned int vecbuf_cnt = 0, vecbuf_size = 0;
    int rv;

    if (rbvec_is_empty(rbv))
        return 0;
    rv = rbvec_consumer_peek(rbv, size, &vecbuf, &vecbuf_cnt, &vecbuf_size);
    if (rv < 0)
        return rv;
    if (vecbuf_size) {
        unsigned int s, i;

        for (s = 0, i = 0; i < vecbuf_cnt; i++) {
            memcpy(buf + s, vecbuf[i].iov_base, vecbuf[i].iov_len);
            s += vecbuf[i].iov_len;
        }
        assert(s == vecbuf_size);
        rbvec_consumed(rbv, vecbuf_size);
        free(vecbuf);
    }

    return vecbuf_size;
}

int rbvec_get_all(rbvec_t *rbv, unsigned char **buf, unsigned int *buf_size)
{
    struct iovec *vecbuf = NULL;
    unsigned int vecbuf_cnt = 0, vecbuf_size = 0;
    int rv;

    if (rbvec_is_empty(rbv))
        return 0;
    rv = rbvec_consumer_peek(rbv, rbvec_size(rbv), &vecbuf, &vecbuf_cnt, &vecbuf_size);
    if (rv < 0)
        return rv;
    if (vecbuf_size) {
        unsigned char *_buf;
        unsigned int s, i;

        _buf = (unsigned char *)malloc(vecbuf_size);
        if (!_buf)
            return -ENOMEM;
        for (s = 0, i = 0; i < vecbuf_cnt; i++) {
            memcpy(_buf + s, vecbuf[i].iov_base, vecbuf[i].iov_len);
            s += vecbuf[i].iov_len;
        }
        assert(s == vecbuf_size);
        rbvec_consumed(rbv, vecbuf_size);
        free(vecbuf);
        *buf = _buf;
        *buf_size = vecbuf_size;
    }

    return 0;
}

unsigned int rbvec_puts(rbvec_t *rbv, const unsigned char *buf, unsigned int size)
{
    struct iovec *vecbuf = NULL;
    unsigned int vecbuf_cnt = 0, vecbuf_size = 0;
    int rv;

    if (rbvec_is_full(rbv))
        return 0;
    rv = rbvec_producer_force_peek(rbv, size, &vecbuf, &vecbuf_cnt, &vecbuf_size);
    if (rv < 0)
        return rv;
    if (vecbuf_size) {
        unsigned int s, i;

        for (s = 0, i = 0; i < vecbuf_cnt; i++) {
            memcpy(vecbuf[i].iov_base, buf + s, vecbuf[i].iov_len);
            s += vecbuf[i].iov_len;
        }
        assert(s == vecbuf_size);
        rbvec_produced(rbv, vecbuf_size);
        free(vecbuf);
    }

    return vecbuf_size;
}

#define new_size(rbv)       ((rbv)->ele_size)
#define can_expand(rbv)     (rbvec_num(rbv) < rbvec_max_num(rbv))

int rbvec_consumer_peek_at(rbvec_t *rbv,
    unsigned int offset,
    unsigned int size,
    struct iovec **vecbuf,
    unsigned int *vecbuf_cnt,
    unsigned int *vecbuf_size)
{
    rb_t *rb;
    unsigned char *buf;
    unsigned int buf_size, remaining;
    unsigned int idx, end;
    struct iovec *vbuf = NULL;
    unsigned int vbuf_cnt = 0, vbuf_size = 0;

    if (!size)
        return 0;
    idx = rbv->out;
    if (rbvec_is_full(rbv))
        end = rbv->in;
    else
        end = rbv->in + 1;
    remaining = size;
    for (; remaining && idx < end; idx++) {
        unsigned int of;

        rb = rbv->vec[idx & rbv->mask];
        of = offset;
        do {
            buf_size = rb_consumer_peek_at(rb, of, remaining, &buf);
            if (buf_size) {
                assert(buf_size <= remaining);
                vbuf = (struct iovec *)realloc(vbuf, sizeof(*vbuf) * (vbuf_cnt + 1));
                if (!vbuf)
                    return -ENOMEM;
                vbuf[vbuf_cnt].iov_base = buf;
                vbuf[vbuf_cnt].iov_len = buf_size;
                vbuf_cnt++;
                vbuf_size += buf_size;
                if (buf_size == remaining)
                    goto LOOP_EXIT;
                offset = 0;
                of += buf_size;
                remaining -= buf_size;
            } else if (buf_size == 0) {
                if (offset >= rb_used_size(rb))
                    offset -= rb_used_size(rb);
            }
        } while (buf_size);
    }

LOOP_EXIT:
    assert(vbuf_size <= size);
    *vecbuf = vbuf;
    *vecbuf_cnt = vbuf_cnt;
    *vecbuf_size = vbuf_size;

    return 0;
}

static producer_peek(rbvec_t *rbv, 
    unsigned int offset, 
    unsigned int size, 
    struct iovec **vecbuf, 
    unsigned int *vecbuf_cnt, 
    unsigned int *vecbuf_size, 
    int forced)
{
    rb_t *rb;
    unsigned char *buf = NULL;
    unsigned int buf_size, remaining;
    unsigned int idx, end;
    struct iovec *vbuf = NULL;
    unsigned int vbuf_cnt = 0, vbuf_size = 0;
    int expanded = 0;

    if (!size)
        return 0;
    idx = rbv->in;
    end = rbv->out + rbvec_num(rbv);
    remaining = size;
    do {
        for (; remaining && idx < end; idx++) {
            unsigned int of;

            rb = rbv->vec[idx & rbv->mask];
            of = offset;
            do {
                buf_size = rb_producer_peek_at(rb, of, remaining, &buf);
                if (buf_size) {
                    assert(buf_size <= remaining);
                    vbuf = (struct iovec *)realloc(vbuf, sizeof(*vbuf) * (vbuf_cnt + 1));
                    if (!vbuf)
                        return -ENOMEM;
                    vbuf[vbuf_cnt].iov_base = buf;
                    vbuf[vbuf_cnt].iov_len = buf_size;
                    vbuf_cnt++;
                    vbuf_size += buf_size;
                    if (buf_size == remaining)
                        goto LOOP_EXIT;
                    offset = 0;
                    of += buf_size;
                    remaining -= buf_size;
                } else if (buf_size == 0) {
                    if (offset >= rb_avail_size(rb))
                        offset -= rb_avail_size(rb);
                }
            } while (buf_size);
        }

        if (can_expand(rbv) && (forced || !expanded)) {
            unsigned int i, j, n;

            n = rbvec_num(rbv);
            for (i = 0, j = n; i < n; i++, j++) {
                int rv;

                rv = rb_init(&rb, new_size(rbv));
                if (rv < 0)
                    return rv;
                rbv->vec[j] = rb;
            }
            idx = n;
            rbv->in = rbv->in & rbv->mask;
            rbv->out = rbv->out & rbv->mask;
            rbv->cnt_bit_offset++;
            rbv->mask = rbvec_num(rbv) - 1;
            end = rbvec_num(rbv);
            expanded = 1;
        } else
            break;
    } while (1);

LOOP_EXIT:
    assert(vbuf_size <= size);
    *vecbuf = vbuf;
    *vecbuf_cnt = vbuf_cnt;
    *vecbuf_size = vbuf_size;

    return 0;
}

int rbvec_producer_peek_at(rbvec_t *rbv,
    unsigned int offset,
    unsigned int size,
    struct iovec **vecbuf,
    unsigned int *vecbuf_cnt,
    unsigned int *vecbuf_size)
{
    return producer_peek(rbv, offset, size, vecbuf, vecbuf_cnt, vecbuf_size, 0);
}

int rbvec_producer_force_peek_at(rbvec_t *rbv, 
    unsigned int offset, 
    unsigned int size, 
    struct iovec **vecbuf, 
    unsigned int *vecbuf_cnt, 
    unsigned int *vecbuf_size)
{
    return producer_peek(rbv, offset, size, vecbuf, vecbuf_cnt, vecbuf_size, 1);
}

unsigned int rbvec_consumed(rbvec_t *rbv, unsigned int size)
{
    unsigned int idx, end, remaining;

    idx = rbv->out;
    if (rbvec_is_full(rbv))
        end = rbv->in;
    else
        end = rbv->in + 1;
    remaining = size;
    for (; remaining && idx < end; idx++) {
        rb_t *rb;
        unsigned int s;

        rb = rbv->vec[idx & rbv->mask];
        s = min(remaining, rb_used_size(rb));
        rb_consumed(rb, s);
        remaining -= s;
        if (rb_is_empty(rb) && rbv->out < rbv->in)
            rbv->out++;
    }

    return size - remaining;
}

unsigned int rbvec_produced(rbvec_t *rbv, unsigned int size)
{
    unsigned int idx, end, remaining;

    idx = rbv->in;
    end = rbv->out + rbvec_num(rbv);
    remaining = size;
    for (; remaining && idx < end; idx++) {
        rb_t *rb;
        unsigned int s;

        rb = rbv->vec[idx & rbv->mask];
        s = min(remaining, rb_avail_size(rb));
        rb_produced(rb, s);
        remaining -= s;
        if (rb_is_full(rb))
            rbv->in++;
    }

    return size - remaining;
}

int rbvec_read(rbvec_t *rbv, rb_read_pt read_cb, void *ptr, unsigned int *read)
{
    struct iovec *vecbuf;
    unsigned int vecbuf_cnt, vecbuf_size;
    int rv;

    do {
        vecbuf = NULL;
        vecbuf_cnt = vecbuf_size = 0;
        rv = rbvec_producer_force_peek(rbv, rbv->ele_size, &vecbuf, &vecbuf_cnt, &vecbuf_size);
        if (rv < 0)
            return rv;
        if (vecbuf_size) {
            rv = read_cb(ptr, vecbuf, vecbuf_cnt);
            if (rv > 0) {
                unsigned int p;

                p = rbvec_produced(rbv, (unsigned int)rv);
                assert(p == (unsigned int)rv);
                *read += (unsigned int)rv;
            }
        }
    } while (rv > 0 && (unsigned int)rv == vecbuf_size);

    return rv;
}

int rbvec_write(rbvec_t *rbv, rb_write_pt write_cb, void *ptr, unsigned int *wrote)
{
    struct iovec *vecbuf;
    unsigned int vecbuf_cnt, vecbuf_size;
    int rv;

    do {
        vecbuf = NULL;
        vecbuf_cnt = vecbuf_size = 0;
        rv = rbvec_consumer_peek(rbv, rbv->ele_size, &vecbuf, &vecbuf_cnt, &vecbuf_size);
        if (rv < 0)
            return rv;
        if (vecbuf_size) {
            rv = write_cb(ptr, vecbuf, vecbuf_cnt);
            if (rv > 0) {
                unsigned int c;

                c = rbvec_consumed(rbv, (unsigned int)rv);
                assert(c == (unsigned int)rv);
                *wrote += (unsigned int)rv;
            }
        }
    } while (rv > 0 && (unsigned int)rv == vecbuf_size);

    return rv;
}
