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

#ifndef RTTY_KCP_H
#define RTTY_KCP_H

#include <stdbool.h>
#include <ev.h>

#include "kcp/ikcp.h"

struct rtty;

struct rtty_kcp {
    bool on;

    ikcpcb *kcp;

    struct ev_timer tmr;

    const char *password;
    void *cipher;

    bool nodelay;
    int interval;
    int resend;
    bool nc;
    int sndwnd;
    int rcvwnd;
    int mtu;
};

void rtty_kcp_init_cipher(struct rtty_kcp *kcp);
void rtty_kcp_check(struct rtty *rtty);
int rtty_kcp_init(struct rtty *rtty);
void rtty_kcp_release(struct rtty *rtty);
int rtty_kcp_read(struct rtty *rtty, int fd);
int rtty_kcp_write(struct rtty *rtty, int fd);

#endif
