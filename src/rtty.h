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
#include "list.h"

#ifdef SSL_SUPPORT
#include "ssl/ssl.h"
#endif

#define RTTY_PROTO_VER              6
#define RTTY_MAX_TTY                10
#define RTTY_HEARTBEAT_TIMEOUT      3.0
#define RTTY_TTY_ACK_BLOCK          4096

enum {
    MSG_TYPE_REGISTER,
    MSG_TYPE_TERM_OPEN,
    MSG_TYPE_LOGOUT,
    MSG_TYPE_TERMDATA,
    MSG_TYPE_WINSIZE,
    MSG_TYPE_CMD,
    MSG_TYPE_HEARTBEAT,
    MSG_TYPE_FILE,
    MSG_TYPE_HTTP,
    MSG_TYPE_ACK,
    MSG_TYPE_SERIAL_PORTS,
    MSG_TYPE_SERIAL_OPEN,
    MSG_TYPE_TCP,
    MSG_TYPE_MAX = MSG_TYPE_TCP
};

enum {
    MSG_REG_ATTR_HEARTBEAT,
    MSG_REG_ATTR_DEVID,
    MSG_REG_ATTR_DESCRIPTION,
    MSG_REG_ATTR_TOKEN,
    MSG_REG_ATTR_GROUP,
};

enum {
    MSG_HEARTBEAT_ATTR_UPTIME,
};

enum {
    RTTY_STATE_DISCONNECTED,
    RTTY_STATE_CONNECTED
};

struct rtty;

enum {
    TTY_TERM,
    TTY_SERIAL
};

struct tty {
    char sid[33];
    struct list_head node;
    struct rtty *rtty;
    struct ev_io ior;
    struct ev_io iow;
    struct buffer wb;
    uint32_t wait_ack;
    int type;
};

struct rtty {
    char login_path[128]; /* /bin/login */
    const char *host;
    int port;
    int sock;
    const char *group;
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
    int heartbeat;
    int http_timeout;
    double last_heartbeat;
    bool wait_heartbeat;
    bool registered;
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
    struct list_head tcp_conns;
    bool tcp_read_paused;
};

struct tty *find_tty(struct rtty *rtty, const char *sid);
void del_tty(struct tty *tty);
void tty_on_write(struct ev_loop *loop, struct ev_io *w, int revents);
void tty_wait_ack(struct tty *tty, uint32_t len);
void tty_ack(struct tty *tty, uint16_t ack);
int rtty_start(struct rtty *rtty);
void rtty_exit(struct rtty *rtty);
void rtty_send_msg(struct rtty *rtty, int type, void *data, int len);

#endif
