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
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <netdb.h>

#include "net.h"
#include "log.h"

struct net_context {
    struct ev_timer tmr;
    struct ev_io iow;
    int sock;
    void *arg;

    void (*on_connected)(int sock, void *arg);
};

static const char *port2str(int port)
{
    static char buffer[sizeof("65535\0")];

    if (port < 0 || port > 65535)
        return NULL;

    snprintf(buffer, sizeof(buffer), "%u", port);

    return buffer;
}


static void sock_write_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct net_context *ctx = w->data;
    int err = 0;
    socklen_t len = sizeof(err);
    int ret;

    ev_io_stop(loop, w);
    ev_timer_stop(loop, &ctx->tmr);

    ret = getsockopt(w->fd, SOL_SOCKET, SO_ERROR, &err, &len);
    if (ret < 0) {
        log_err("getsockopt: %s\n", strerror(errno));
        goto err;
    }

    if (err > 0) {
        log_err("network connect failed: %s\n", strerror(err));
        goto err;
    }

    ctx->on_connected(w->fd, ctx->arg);
    return;

err:
    close(w->fd);
    ctx->on_connected(-1, ctx->arg);
}

static void timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    struct net_context *ctx = w->data;

    log_err("network connect timeout\n");

    ev_io_stop(loop, &ctx->iow);
    close(ctx->sock);
    ctx->on_connected(-1, ctx->arg);
}

static void wait_connect(struct ev_loop *loop, int sock, int timeout,
                         void (*on_connected)(int sock, void *arg), void *arg)
{
    static struct net_context ctx;

    ctx.sock = sock;
    ctx.arg = arg;
    ctx.on_connected = on_connected;

    ev_timer_init(&ctx.tmr, timer_cb, timeout, 0);
    ctx.tmr.data = &ctx;
    ev_timer_start(loop, &ctx.tmr);

    ev_io_init(&ctx.iow, sock_write_cb, sock, EV_WRITE);
    ctx.iow.data = &ctx;
    ev_io_start(loop, &ctx.iow);
}

int tcp_connect(struct ev_loop *loop, const char *host, int port,
                void (*on_connected)(int sock, void *arg), void *arg)
{
    struct sockaddr *addr = NULL;
    struct addrinfo *result, *rp;
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_ADDRCONFIG
    };
    int sock = -1;
    int addr_len;
    int ret;

    ret = getaddrinfo(host, port2str(port), &hints, &result);
    if (ret) {
        if (ret == EAI_SYSTEM) {
            log_err("getaddrinfo failed: %s\n", strerror(errno));
            return -1;
        }

        log_err("getaddrinfo failed: %s\n", gai_strerror(ret));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            addr = rp->ai_addr;
            addr_len = rp->ai_addrlen;
            break;
        }
    }

    if (!addr) {
        log_err("getaddrinfo failed: Not found addr\n");
        goto free_addrinfo;
    }

    sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sock < 0) {
        log_err("create socket failed: %s\n", strerror(errno));
        goto free_addrinfo;
    }

    if (connect(sock, addr, addr_len) < 0) {
        if (errno != EINPROGRESS) {
            log_err("connect failed: %s\n", strerror(errno));
            close(sock);
            sock = -1;
            goto free_addrinfo;
        }
        wait_connect(loop, sock, 3, on_connected, arg);
    } else {
        on_connected(sock, arg);
    }

free_addrinfo:
    freeaddrinfo(result);
    return sock;
}
