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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "web.h"
#include "net.h"
#include "log.h"

void web_request_free(struct web_request_ctx *ctx)
{
    struct rtty *rtty = ctx->rtty;
    struct ev_loop *loop = rtty->loop;

    if (ctx->sock > 0) {
        ev_io_stop(loop, &ctx->ior);
        ev_io_stop(loop, &ctx->iow);
        ev_timer_stop(loop, &ctx->tmr);
        close(ctx->sock);
    }

    buffer_free(&ctx->rb);
    buffer_free(&ctx->wb);

    list_del(&ctx->head);

    free(ctx);
}

static void on_net_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct web_request_ctx *ctx = container_of(w, struct web_request_ctx, ior);
    struct rtty *rtty = ctx->rtty;
    struct buffer *wb = &rtty->wb;
    uint8_t buf[4096];
    int ret;

    ret = read(w->fd, buf, 4096);
    if (ret <= 0)
        goto done;

    buffer_put_u8(wb, MSG_TYPE_WEB);
    buffer_put_u16be(wb, 18 + ret);
    buffer_put_data(wb, ctx->addr, 18);
    buffer_put_data(wb, buf, ret);
    ev_io_start(rtty->loop, &rtty->iow);

    ctx->active = ev_now(rtty->loop);

    return;

done:
    web_request_free(ctx);
}

static void on_net_write(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct web_request_ctx *ctx = container_of(w, struct web_request_ctx, iow);

    if (buffer_pull_to_fd(&ctx->wb, w->fd, -1) < 0)
        goto err;

    if (buffer_length(&ctx->wb) > 0)
        return;

err:
    ev_io_stop(loop, w);
}

static void on_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    struct web_request_ctx *ctx = container_of(w, struct web_request_ctx, tmr);
    ev_tstamp now = ev_now(loop);

    if (now - ctx->active < 30)
        return;

    web_request_free(ctx);
}

static void on_connected(int sock, void *arg)
{
    struct web_request_ctx *ctx = (struct web_request_ctx *)arg;
    struct ev_loop *loop = ctx->rtty->loop;

    if (sock < 0) {
        web_request_free(ctx);
        return;
    }

    ev_io_init(&ctx->ior, on_net_read, sock, EV_READ);
    ev_io_start(loop, &ctx->ior);

    ev_io_init(&ctx->iow, on_net_write, sock, EV_WRITE);
    ev_io_start(loop, &ctx->iow);

    ev_timer_init(&ctx->tmr, on_timer_cb, 1, 0);
    ev_timer_start(loop, &ctx->tmr);

    ctx->sock = sock;
}

static struct web_request_ctx *find_exist_ctx(struct list_head *reqs, uint8_t *addr)
{
    struct web_request_ctx *ctx;

    list_for_each_entry(ctx, reqs, head) {
        if (!memcmp(ctx->addr, addr, 18))
            return ctx;
    }

    return NULL;
}

void web_request(struct rtty *rtty, int len)
{
    struct web_request_ctx *ctx;
    struct sockaddr_in addrin = {
        .sin_family = AF_INET
    };
    int sock, req_len;
    uint8_t addr[18];
    void *data;

    if (len == 0) {
        web_reqs_free(&rtty->web_reqs);
        return;
    }

    buffer_pull(&rtty->rb, addr, 18);

    req_len = len - 18;

    if (req_len == 0)
        return;

    ctx = find_exist_ctx(&rtty->web_reqs, addr);
    if (ctx) {
        buffer_pull(&rtty->rb, NULL, 6);
        req_len -= 6;

        data = buffer_put(&ctx->wb, req_len);
        buffer_pull(&rtty->rb, data, req_len);

        if (ctx->sock > 0)
            ev_io_start(rtty->loop, &ctx->iow);
        return;
    }

    addrin.sin_addr.s_addr = buffer_pull_u32(&rtty->rb);
    addrin.sin_port = buffer_pull_u16(&rtty->rb);

    req_len -= 6;

    ctx = (struct web_request_ctx *)calloc(1, sizeof(struct web_request_ctx));
    ctx->rtty = rtty;
    ctx->active = ev_now(rtty->loop);

    memcpy(ctx->addr, addr, 18);

    data = buffer_put(&ctx->wb, req_len);
    buffer_pull(&rtty->rb, data, req_len);

    list_add(&ctx->head, &rtty->web_reqs);

    sock = tcp_connect_sockaddr(rtty->loop, (struct sockaddr *)&addrin, sizeof(addrin), on_connected, ctx);
    if (sock < 0)
        web_request_free(ctx);
}

void web_reqs_free(struct list_head *reqs)
{
    struct web_request_ctx *ctx, *tmp;

    list_for_each_entry_safe(ctx, tmp, reqs, head)
        web_request_free(ctx);
}
