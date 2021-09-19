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

#include "http.h"
#include "net.h"
#include "log/log.h"

static void send_http_msg(struct http_connection *conn, size_t len, uint8_t *data)
{
    struct rtty *rtty = conn->rtty;
    struct buffer *wb = &rtty->wb;

    buffer_put_u8(wb, MSG_TYPE_HTTP);
    buffer_put_u16be(wb, 18 + len);
    buffer_put_data(wb, conn->addr, 18);
    buffer_put_data(wb, data, len);
    ev_io_start(rtty->loop, &rtty->iow);

    conn->active = ev_now(rtty->loop);
}

void http_conn_free(struct http_connection *conn)
{
    struct rtty *rtty = conn->rtty;
    struct ev_loop *loop = rtty->loop;

    send_http_msg(conn, 0, NULL);

    if (conn->sock > 0) {
        ev_io_stop(loop, &conn->ior);
        ev_io_stop(loop, &conn->iow);
        ev_timer_stop(loop, &conn->tmr);
        close(conn->sock);
    }

    buffer_free(&conn->rb);
    buffer_free(&conn->wb);

    list_del(&conn->head);

    free(conn);
}

static void on_net_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct http_connection *conn = container_of(w, struct http_connection, ior);
    uint8_t buf[4096];
    int ret;

    ret = read(w->fd, buf, 4096);
    if (ret <= 0)
        goto done;

    send_http_msg(conn, ret, buf);

    return;

done:
    http_conn_free(conn);
}

static void on_net_write(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct http_connection *conn = container_of(w, struct http_connection, iow);

    if (buffer_pull_to_fd(&conn->wb, w->fd, -1) < 0)
        goto err;

    if (buffer_length(&conn->wb) > 0)
        return;

err:
    ev_io_stop(loop, w);
}

static void on_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    struct http_connection *conn = container_of(w, struct http_connection, tmr);
    ev_tstamp now = ev_now(loop);

    if (now - conn->active < 30)
        return;

    http_conn_free(conn);
}

static void on_connected(int sock, void *arg)
{
    struct http_connection *conn = (struct http_connection *)arg;
    struct ev_loop *loop = conn->rtty->loop;

    if (sock < 0) {
        http_conn_free(conn);
        return;
    }

    ev_io_init(&conn->ior, on_net_read, sock, EV_READ);
    ev_io_start(loop, &conn->ior);

    ev_io_init(&conn->iow, on_net_write, sock, EV_WRITE);
    ev_io_start(loop, &conn->iow);

    ev_timer_init(&conn->tmr, on_timer_cb, 3, 3);
    ev_timer_start(loop, &conn->tmr);

    conn->sock = sock;
}

static struct http_connection *find_exist_connection(struct list_head *conns, uint8_t *addr)
{
    struct http_connection *conn;

    list_for_each_entry(conn, conns, head) {
        if (!memcmp(conn->addr, addr, 18))
            return conn;
    }

    return NULL;
}

void http_request(struct rtty *rtty, int len)
{
    struct http_connection *conn;
    struct sockaddr_in addrin = {
        .sin_family = AF_INET
    };
    int sock, req_len;
    uint8_t addr[18];
    void *data;

    buffer_pull(&rtty->rb, addr, 18);

    req_len = len - 18;

    if (req_len == 0)
        return;

    conn = find_exist_connection(&rtty->http_conns, addr);
    if (conn) {
        buffer_pull(&rtty->rb, NULL, 6);
        req_len -= 6;

        data = buffer_put(&conn->wb, req_len);
        buffer_pull(&rtty->rb, data, req_len);

        if (conn->sock > 0)
            ev_io_start(rtty->loop, &conn->iow);
        return;
    }

    addrin.sin_addr.s_addr = buffer_pull_u32(&rtty->rb);
    addrin.sin_port = buffer_pull_u16(&rtty->rb);

    req_len -= 6;

    conn = (struct http_connection *)calloc(1, sizeof(struct http_connection));
    conn->rtty = rtty;
    conn->active = ev_now(rtty->loop);

    memcpy(conn->addr, addr, 18);

    data = buffer_put(&conn->wb, req_len);
    buffer_pull(&rtty->rb, data, req_len);

    list_add(&conn->head, &rtty->http_conns);

    sock = tcp_connect_sockaddr(rtty->loop, (struct sockaddr *)&addrin, sizeof(addrin), on_connected, conn);
    if (sock < 0)
        http_conn_free(conn);
}

void http_conns_free(struct list_head *reqs)
{
    struct http_connection *con, *tmp;

    list_for_each_entry_safe(con, tmp, reqs, head)
        http_conn_free(con);
}
