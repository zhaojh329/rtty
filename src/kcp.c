/*
 * MIT License
 *
 * Copyright (c) 2019 Jianhui Zhao <zhaojh329@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sys/socket.h>
#include <endian.h>
#include <errno.h>

#include "log/log.h"
#include "crc32.h"
#include "crypt.h"
#include "rtty.h"

#define NONCE_SIZE 16
#define CRC_SIZE 4
#define CRYPT_HEADER_SIZE (NONCE_SIZE + CRC_SIZE)

#ifdef KCP_SUPPORT

void rtty_kcp_init_cipher(struct rtty_kcp *kcp)
{
    if (!kcp->password)
        return;

    kcp->cipher = cipher_init_ctx(kcp->password);
}

static uint32_t rtty_kcp_now(struct rtty *rtty)
{
    long now = ev_now(rtty->loop) * 1000;
    return now & 0xffffffff;
}

void rtty_kcp_check(struct rtty *rtty)
{
    uint32_t now = rtty_kcp_now(rtty);
    uint32_t next;
    long delay;

    next = ikcp_check(rtty->kcp.kcp, now);
    delay = next - now;

    if (delay <= 0)
        delay = 1;

    ev_timer_set(&rtty->kcp.tmr, 0, delay / 1000.0);
    ev_timer_again(rtty->loop, &rtty->kcp.tmr);
}

static int rtty_kcp_encrypt(struct rtty_kcp *kcp, uint8_t *dst, const uint8_t *src, int srclen)
{
    uint32_t crc32;

    get_random_bytes(dst, NONCE_SIZE);

    crc32 = crc32_checksum_ieee(src, srclen);

    crc32 = htole32(crc32);

    memcpy(dst + NONCE_SIZE, &crc32, CRC_SIZE);
    memcpy(dst + CRYPT_HEADER_SIZE, src, srclen);

    encrypt16(kcp->cipher, dst, dst, srclen + CRYPT_HEADER_SIZE);

    return CRYPT_HEADER_SIZE + srclen;
}

static int rtty_kcp_decrypt(struct rtty_kcp *kcp, uint8_t *dst, const uint8_t *src, int srclen)
{
    uint32_t crc32_your, crc32_our;

    if (srclen < CRYPT_HEADER_SIZE) {
        log_debug("data length too short\n");
        return -1;
    }

    decrypt16(kcp->cipher, dst, src, srclen);

    crc32_our = crc32_checksum_ieee(src + CRYPT_HEADER_SIZE, srclen - CRYPT_HEADER_SIZE);

    memcpy(&crc32_your, dst + NONCE_SIZE, CRC_SIZE);

    if (le32toh(crc32_your) !=crc32_our) {
        log_debug("crc32 error\n");
        return -1;
    }

    return srclen;
}

static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    struct rtty *rtty = user;

    if (rtty->kcp.password) {
        uint8_t dst[1500];
        int ret= rtty_kcp_encrypt(&rtty->kcp, dst, (const uint8_t *)buf, len);
        return send(rtty->sock, dst, ret, 0);
    }

    return send(rtty->sock, buf, len, 0);
}

static void kcp_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    struct rtty *rtty = container_of(w, struct rtty, kcp.tmr);
    uint32_t now = rtty_kcp_now(rtty);

    ikcp_update(rtty->kcp.kcp, now);
    rtty_kcp_check(rtty);
}

int rtty_kcp_init(struct rtty *rtty)
{
    ikcpcb *kcp;

    if (!rtty->kcp.on)
        return 0;

    kcp = ikcp_create(random(), rtty);
    if (!kcp) {
        log_err("malloc: %s\n", strerror(errno));
        return -1;
    }

    ikcp_setoutput(kcp, udp_output);

    ikcp_nodelay(kcp, rtty->kcp.nodelay, rtty->kcp.interval,
            rtty->kcp.resend, rtty->kcp.nc);

    ikcp_wndsize(kcp, rtty->kcp.sndwnd, rtty->kcp.rcvwnd);

    ikcp_setmtu(kcp, rtty->kcp.mtu);

    ev_timer_init(&rtty->kcp.tmr, kcp_timer_cb, 1.0, 0);

    rtty->kcp.kcp = kcp;

    return 0;
}

void rtty_kcp_release(struct rtty *rtty)
{
    if (rtty->kcp.kcp)
        return;

    ikcp_release(rtty->kcp.kcp);
    rtty->kcp.kcp = NULL;
}

int rtty_kcp_read(struct rtty *rtty, int fd)
{
    char buf[1500];
    char *in = buf;
    int ret;

    ret = read(fd, buf, sizeof(buf));
    if (ret < 0) {
        log_err("socket read error: %s\n", strerror(errno));
        return -1;
    }

    if (rtty->kcp.password) {
        ret = rtty_kcp_decrypt(&rtty->kcp, (uint8_t *)buf, (const uint8_t *)buf, ret);
        if (ret < 0)
            return -1;
        in = buf + CRYPT_HEADER_SIZE;
        ret -= CRYPT_HEADER_SIZE;
    }

    ret = ikcp_input(rtty->kcp.kcp, in, ret);
    if (ret < 0) {
        log_err("ikcp_input fail: %d\n", ret);
        return -1;
    }

    while (true) {
        ret = ikcp_recv(rtty->kcp.kcp, buf, sizeof(buf));
        if (ret < 0)
            break;
        buffer_put_data(&rtty->rb, buf, ret);
    }

    rtty_kcp_check(rtty);

    return 0;
}

int rtty_kcp_write(struct rtty *rtty, int fd)
{
    struct buffer *b = &rtty->wb;
    ikcpcb *kcp = rtty->kcp.kcp;
    size_t mss = kcp->mss;
    int ret;

    if (rtty->kcp.password)
        mss -= CRYPT_HEADER_SIZE;

    while (true) {
        size_t len = buffer_length(b);
        if (len == 0)
            break;

        if (len > mss)
            len = mss;

        ret = ikcp_send(kcp, buffer_data(b), len);
        if (ret < 0) {
            log_err("ikcp_send fail: %d\n", ret);
            return -1;
        }

        buffer_pull(b, NULL, ret);
    }

    rtty_kcp_check(rtty);

    return 0;
}

#endif
