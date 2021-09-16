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

#ifndef RTTY_RTTY_H
#define RTTY_RTTY_H

#include <stdbool.h>
#include <ev.h>

#include "config.h"
#include "buffer.h"
#include "file.h"
#include "list.h"

#ifdef SSL_SUPPORT
#include "ssl/ssl.h"
#endif

#define RTTY_PROTO_VER              3
#define RTTY_MAX_TTY                10
#define RTTY_HEARTBEAT_INTEVAL      5.0
#define RTTY_TTY_TIMEOUT            600
#define RTTY_TTY_ACK_BLOCK          4096

enum {
    MSG_TYPE_REGISTER,
    MSG_TYPE_LOGIN,
    MSG_TYPE_LOGOUT,
    MSG_TYPE_TERMDATA,
    MSG_TYPE_WINSIZE,
    MSG_TYPE_CMD,
    MSG_TYPE_HEARTBEAT,
    MSG_TYPE_FILE,
    MSG_TYPE_HTTP,
    MSG_TYPE_ACK,
    MSG_TYPE_MAX = MSG_TYPE_ACK
};

struct rtty;

struct tty {
    pid_t pid;
    int pty;
    char sid[33];
    struct ev_io ior;
    struct ev_io iow;
    struct ev_child cw;
    struct buffer wb;
    struct rtty *rtty;
    ev_tstamp active;
    uint32_t wait_ack;
    struct ev_timer tmr;
    struct list_head node;
    struct file_context file;
};

struct rtty {
    const char *host;
    int port;
    int sock;
    const char *devid;
    const char *token;        /* authorization token */
    const char *description;
    const char *username;
    bool ssl_on;
    struct buffer rb;
    struct buffer wb;
    struct ev_io iow;
    struct ev_io ior;
    struct ev_timer tmr;
    struct ev_loop *loop;
    int ninactive;
    ev_tstamp active;
    ev_tstamp last_heartbeat;
    bool reconnect;
#ifdef SSL_SUPPORT
    struct ssl_context *ssl_ctx;
    bool insecure;
    bool ssl_negotiated;
    void *ssl;
#endif
    int ntty;   /* tty number */
    struct list_head ttys;
    struct list_head http_conns;
};

int rtty_start(struct rtty *rtty);
void rtty_exit(struct rtty *rtty);
void rtty_send_msg(struct rtty *rtty, int type, void *data, int len);

#endif
