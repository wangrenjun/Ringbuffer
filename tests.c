#include "ringbuffer.h"
#include "assert.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

static void test_rb();
static void test_rbvec();

int main()
{
    test_rb();
    test_rbvec();

    return 0;
}

#define RB_SIZE             512
#define BUF_SIZE            128
#define LARGE_BUF_SIZE      8192

static void test_rb()
{
    rb_t *rb;
    int rv;
    unsigned int rs;
    unsigned char buf1[BUF_SIZE], buf2[BUF_SIZE];
    unsigned char *bufp, *bufp2;
    unsigned int bufp_size;
    unsigned char large_buf[LARGE_BUF_SIZE];

    rv = rb_init(&rb, RB_SIZE);
    assert(!rv);

    rs = rb_size(rb);
    assert(rs == RB_SIZE);

    rs = rb_used_size(rb);
    assert(rs == 0);

    rs = rb_avail_size(rb);
    assert(rs == RB_SIZE);

    rv = rb_is_full(rb);
    assert(!rv);

    rv = rb_is_empty(rb);
    assert(rv);

    memset(buf1, 'Z', BUF_SIZE);
    rs = rb_puts(rb, buf1, BUF_SIZE);
    assert(rs == BUF_SIZE);

    rs = rb_used_size(rb);
    assert(rs == BUF_SIZE);

    rs = rb_avail_size(rb);
    assert(rs == RB_SIZE - BUF_SIZE);

    rv = rb_is_full(rb);
    assert(!rv);

    rv = rb_is_empty(rb);
    assert(!rv);

    rs = rb_gets(rb, buf2, BUF_SIZE);
    assert(rs == BUF_SIZE);
    rv = memcmp(buf1, buf2, BUF_SIZE);
    assert(rv == 0);

    rs = rb_puts(rb, buf1, BUF_SIZE);
    assert(rs == BUF_SIZE);

    rs = rb_puts(rb, buf1, BUF_SIZE);
    assert(rs == BUF_SIZE);

    bufp = NULL;
    bufp_size = 0;
    rv = rb_get_all(rb, &bufp, &bufp_size);
    assert(rv == 0);
    assert(bufp);
    assert(bufp_size == BUF_SIZE << 1);
    rv = memcmp(buf1, bufp, BUF_SIZE);
    assert(rv == 0);
    rv = memcmp(buf1, bufp + BUF_SIZE, BUF_SIZE);
    assert(rv == 0);
    free(bufp);

    rs = rb_used_size(rb);
    assert(rs == 0);

    rs = rb_avail_size(rb);
    assert(rs == RB_SIZE);

    rv = rb_is_full(rb);
    assert(!rv);

    rv = rb_is_empty(rb);
    assert(rv);

    bufp = NULL;
    rs = rb_consumer_peek_at(rb, 0, BUF_SIZE, &bufp);
    assert(rs == 0);
    assert(!bufp);

    bufp = NULL;
    rs = rb_producer_peek_at(rb, 0, LARGE_BUF_SIZE, &bufp);
    assert(rs == BUF_SIZE);
    assert(bufp);

    bufp2 = NULL;
    rs = rb_producer_peek_at(rb, BUF_SIZE, LARGE_BUF_SIZE, &bufp2);
    assert(rs == RB_SIZE - BUF_SIZE);
    assert(bufp2);

    memset(bufp, 'X', BUF_SIZE);
    rb_produced(rb, BUF_SIZE);

    rs = rb_used_size(rb);
    assert(rs == BUF_SIZE);

    rs = rb_avail_size(rb);
    assert(rs == RB_SIZE - BUF_SIZE);

    rv = rb_is_full(rb);
    assert(!rv);

    rv = rb_is_empty(rb);
    assert(!rv);

    bufp2 = NULL;
    rs = rb_consumer_peek_at(rb, 0, BUF_SIZE, &bufp2);
    assert(rs == BUF_SIZE);
    assert(bufp2);
    assert(bufp == bufp2);
    memset(large_buf, 'X', LARGE_BUF_SIZE);
    rv = memcmp(large_buf, bufp2, BUF_SIZE);
    assert(rv == 0);

    rb_reinit(rb);
    bufp = NULL;
    rs = rb_producer_peek_at(rb, 0, LARGE_BUF_SIZE, &bufp);
    assert(rs == RB_SIZE);
    assert(bufp);
    memset(bufp, 'Q', RB_SIZE);
    rb_produced(rb, RB_SIZE);

    rs = rb_used_size(rb);
    assert(rs == RB_SIZE);

    rs = rb_avail_size(rb);
    assert(rs == 0);

    rv = rb_is_full(rb);
    assert(rv);

    rv = rb_is_empty(rb);
    assert(!rv);

    rs = rb_consumer_peek_at(rb, 0, BUF_SIZE, &bufp2);
    rb_consumed(rb, BUF_SIZE);
    rs = rb_consumer_peek_at(rb, 0, BUF_SIZE, &bufp2);
    rb_consumed(rb, BUF_SIZE);

    rs = rb_used_size(rb);
    assert(rs == BUF_SIZE << 1);

    rs = rb_avail_size(rb);
    assert(rs == BUF_SIZE << 1);

    rs = rb_consumer_peek_at(rb, 0, BUF_SIZE, &bufp2);
    rb_consumed(rb, BUF_SIZE);
    rs = rb_consumer_peek_at(rb, 0, BUF_SIZE, &bufp2);
    rb_consumed(rb, BUF_SIZE);

    rs = rb_used_size(rb);
    assert(rs == 0);

    rs = rb_avail_size(rb);
    assert(rs == RB_SIZE);

    rv = rb_is_full(rb);
    assert(!rv);

    rv = rb_is_empty(rb);
    assert(rv);

    rb_deinit(rb);

    printf("rb done\n");
}

#define RBVEC_MAX_NUM       128
#define RBVEC_ELE_SIZE      512
#define HUGE_BUF_SIZE       65536   /* 128 * 512 */

static void test_rbvec()
{
    rbvec_t *rbv;
    int rv, i;
    unsigned int rs;
    unsigned char buf1[BUF_SIZE], buf2[BUF_SIZE];
    unsigned char *bufp;
    unsigned int bufp_size;
    unsigned char huge_buf[HUGE_BUF_SIZE];
    struct iovec *vecbuf;
    unsigned int vecbuf_cnt, vecbuf_size;

    rv = rbvec_init(&rbv, RBVEC_MAX_NUM, RBVEC_ELE_SIZE);
    assert(!rv);

    rs = rbvec_gets(rbv, buf1, BUF_SIZE);
    assert(rs == 0);

    bufp = NULL;
    bufp_size = 0;
    rv = rbvec_get_all(rbv, &bufp, &bufp_size);
    assert(rv == 0);
    assert(bufp == NULL);
    assert(bufp_size == 0);

    assert(rbvec_max_num(rbv) == RBVEC_MAX_NUM);
    assert(rbvec_num(rbv) == 1);
    assert(rbvec_used_num(rbv) == 0);
    assert(rbvec_max_size(rbv) == RBVEC_ELE_SIZE * RBVEC_MAX_NUM);
    assert(rbvec_size(rbv) == 1 * RBVEC_ELE_SIZE);
    assert(rbvec_is_empty(rbv));
    assert(!rbvec_is_full(rbv));

    memset(buf1, 'X', BUF_SIZE);
    rv = rbvec_puts(rbv, buf1, BUF_SIZE);
    assert(rv == BUF_SIZE);

    assert(rbvec_max_num(rbv) == RBVEC_MAX_NUM);
    assert(rbvec_num(rbv) == 1);
    assert(rbvec_used_num(rbv) == 0);
    assert(rbvec_max_size(rbv) == RBVEC_ELE_SIZE * RBVEC_MAX_NUM);
    assert(rbvec_size(rbv) == 1 * RBVEC_ELE_SIZE);
    assert(!rbvec_is_empty(rbv));
    assert(!rbvec_is_full(rbv));

    bufp = NULL;
    bufp_size = 0;
    rv = rbvec_get_all(rbv, &bufp, &bufp_size);
    assert(rv == 0);
    assert(bufp);
    assert(bufp_size == BUF_SIZE);
    rv = memcmp(buf1, bufp, bufp_size);
    assert(!rv);
    free(bufp);

    assert(rbvec_max_num(rbv) == RBVEC_MAX_NUM);
    assert(rbvec_num(rbv) == 1);
    assert(rbvec_used_num(rbv) == 0);
    assert(rbvec_max_size(rbv) == RBVEC_ELE_SIZE * RBVEC_MAX_NUM);
    assert(rbvec_size(rbv) == 1 * RBVEC_ELE_SIZE);
    assert(rbvec_is_empty(rbv));
    assert(!rbvec_is_full(rbv));

    rv = rbvec_puts(rbv, buf1, BUF_SIZE);
    assert(rv == BUF_SIZE);
    rv = rbvec_puts(rbv, buf1, BUF_SIZE);
    assert(rv == BUF_SIZE);
    rv = rbvec_puts(rbv, buf1, BUF_SIZE);
    assert(rv == BUF_SIZE);
    rv = rbvec_puts(rbv, buf1, BUF_SIZE);
    assert(rv == BUF_SIZE);
    rv = rbvec_puts(rbv, buf1, BUF_SIZE);

    assert(rv == BUF_SIZE);
    assert(rbvec_max_num(rbv) == RBVEC_MAX_NUM);
    assert(rbvec_num(rbv) == 2);
    assert(rbvec_used_num(rbv) == 1);
    assert(rbvec_max_size(rbv) == RBVEC_ELE_SIZE * RBVEC_MAX_NUM);
    assert(rbvec_size(rbv) == 2 * RBVEC_ELE_SIZE);
    assert(!rbvec_is_empty(rbv));
    assert(!rbvec_is_full(rbv));

    rbvec_reinit(rbv);
    memset(huge_buf, 'V', HUGE_BUF_SIZE);
    rv = rbvec_puts(rbv, huge_buf, HUGE_BUF_SIZE);
    assert(rv == HUGE_BUF_SIZE);
    assert(rbvec_max_num(rbv) == RBVEC_MAX_NUM);
    assert(rbvec_num(rbv) == RBVEC_MAX_NUM);
    assert(rbvec_used_num(rbv) == RBVEC_MAX_NUM);
    assert(rbvec_max_size(rbv) == RBVEC_ELE_SIZE * RBVEC_MAX_NUM);
    assert(rbvec_size(rbv) == RBVEC_MAX_NUM * RBVEC_ELE_SIZE);
    assert(!rbvec_is_empty(rbv));
    assert(rbvec_is_full(rbv));

    bufp = NULL;
    bufp_size = 0;
    rv = rbvec_get_all(rbv, &bufp, &bufp_size);
    assert(!rv && bufp && bufp_size == HUGE_BUF_SIZE);
    assert(memcmp(huge_buf, bufp, bufp_size) == 0);
    free(bufp);

    rbvec_deinit(rbv);

    /* ---- */

    rv = rbvec_init(&rbv, RBVEC_MAX_NUM, RBVEC_ELE_SIZE);
    assert(!rv);

    vecbuf = NULL;
    vecbuf_cnt = vecbuf_size = 0;
    rv = rbvec_producer_peek(rbv, RBVEC_ELE_SIZE * 20, &vecbuf, &vecbuf_cnt, &vecbuf_size);
    assert(!rv);
    assert(rbv->cnt_bit_offset == 1 && rbv->mask == 1 && rbvec_num(rbv) == 2 && rbv->out == 0 && rbv->in == 0);
    assert(vecbuf 
        && vecbuf_cnt == 2 
        && vecbuf_size == RBVEC_ELE_SIZE * 2 
        && vecbuf[0].iov_base 
        && vecbuf[0].iov_len == RBVEC_ELE_SIZE
        && vecbuf[1].iov_base
        && vecbuf[1].iov_len == RBVEC_ELE_SIZE);

    rv = rbvec_producer_force_peek(rbv, RBVEC_ELE_SIZE * 20, &vecbuf, &vecbuf_cnt, &vecbuf_size);
    assert(!rv);
    assert(rbv->cnt_bit_offset == 5 && rbv->mask == 31 && rbvec_num(rbv) == 32 && rbv->out == 0 && rbv->in == 0);
    assert(vecbuf
        && vecbuf_cnt == 20
        && vecbuf_size == RBVEC_ELE_SIZE * 20);

    rs = rbvec_produced(rbv, vecbuf_size);
    assert(rs == vecbuf_size);
    assert(rbv->cnt_bit_offset == 5 && rbv->mask == 31 && rbvec_num(rbv) == 32 && rbv->out == 0 && rbv->in == 20);

    rbvec_reinit(rbv);

    rv = rbvec_producer_force_peek(rbv, RBVEC_ELE_SIZE * 26, &vecbuf, &vecbuf_cnt, &vecbuf_size);
    assert(!rv);
    assert(rbv->cnt_bit_offset == 5 && rbv->mask == 31 && rbvec_num(rbv) == 32 && rbv->out == 0 && rbv->in == 0);
    assert(vecbuf && vecbuf_cnt == 26 && vecbuf_size == RBVEC_ELE_SIZE * 26);
    for (i = 0; i < 26; i++) {
        int c = (int)'A' + i;
        memset(vecbuf[i].iov_base, c, vecbuf[i].iov_len);
    }
    rs = rbvec_produced(rbv, vecbuf_size);
    assert(rbv->cnt_bit_offset == 5 && rbv->mask == 31 && rbvec_num(rbv) == 32 && rbv->out == 0 && rbv->in == 26);

    for (i = 0; i < 26 * 4; i++) {
        vecbuf = NULL;
        vecbuf_cnt = vecbuf_size = 0;

        rv = rbvec_consumer_peek(rbv, BUF_SIZE, &vecbuf, &vecbuf_cnt, &vecbuf_size);
        assert(!rv);
        assert(vecbuf && vecbuf_cnt == 1 && vecbuf_size == BUF_SIZE);

        int c = (int)'A' + i / 4;
        memset(buf1, c, BUF_SIZE);
        assert(rbvec_consumed(rbv, vecbuf_size) == vecbuf_size);
        assert(memcmp(vecbuf[0].iov_base, buf1, BUF_SIZE) == 0);
        assert(rbvec_used_size(rbv) == (RBVEC_ELE_SIZE * 26) - (BUF_SIZE * (i+1)));
        assert(rbvec_avail_size(rbv) == rbvec_size(rbv) - (RBVEC_ELE_SIZE * 26) + (BUF_SIZE * (i + 1)));
    }
    assert(rbvec_used_size(rbv) == 0);
    assert(rbvec_avail_size(rbv) == rbv->ele_size * 32);

    rbvec_deinit(rbv);
    
    printf("rbvec done\n");
}

/*
int read_cb(void *ptr, void *buf, int size)
{
    memset(buf, 'X', size);
    return size;
}

int write_cb(void *ptr, const const *buf, int size)
{
    printf("%s\n", buf);

    return size;
}
*/
