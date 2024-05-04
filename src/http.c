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

    if (rtty->sock == -1)
        return;

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

#ifdef SSL_SUPPORT
    if (conn->ssl)
        ssl_session_free(conn->ssl);
#endif

    free(conn);
}

#ifdef SSL_SUPPORT
/* -1 error, 0 pending, 1 ok */
static int ssl_negotiated(struct http_connection *conn)
{
    char err_buf[128];
    int ret;

    ret = ssl_connect(conn->ssl, NULL, NULL);
    if (ret == SSL_WANT_READ || ret == SSL_WANT_WRITE)
        return 0;

    if (ret == SSL_ERROR) {
        log_err("ssl connect error: %s\n", ssl_last_error_string(conn->ssl, err_buf, sizeof(err_buf)));
        return -1;
    }

    conn->ssl_negotiated = true;

    return 1;
}
#endif

static void on_net_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct http_connection *conn = container_of(w, struct http_connection, ior);
    uint8_t buf[4096];
    int ret;

#ifdef SSL_SUPPORT
    if (conn->ssl) {
        static char err_buf[128];

        if (unlikely(!conn->ssl_negotiated)) {
            ret = ssl_negotiated(conn);
            if (ret < 0)
                goto done;
            if (ret == 0)
                return;
        }

        ret = ssl_read(conn->ssl, buf, sizeof(buf));
        if (ret == SSL_ERROR) {
            log_err("ssl_read: %s\n", ssl_last_error_string(conn->ssl, err_buf, sizeof(err_buf)));
            goto done;
        }
        if (ret == SSL_WANT_READ || ret == SSL_WANT_WRITE)
            return;

    } else {
#endif
        ret = read(w->fd, buf, sizeof(buf));
        if (ret <= 0)
            goto done;
#ifdef SSL_SUPPORT
    }
#endif

    send_http_msg(conn, ret, buf);

    return;

done:
    http_conn_free(conn);
}

static void on_net_write(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct http_connection *conn = container_of(w, struct http_connection, iow);
    struct buffer *b = &conn->wb;

#ifdef SSL_SUPPORT
    if (conn->ssl) {
        static char err_buf[128];
        int ret;

        if (unlikely(!conn->ssl_negotiated)) {
            ret = ssl_negotiated(conn);
            if (ret < 0)
                goto err;
            if (ret == 0)
                return;
        }

        ret = ssl_write(conn->ssl, buffer_data(b), buffer_length(b));
        if (ret == SSL_ERROR) {
            log_err("ssl_write: %s\n", ssl_last_error_string(conn->ssl, err_buf, sizeof(err_buf)));
            goto err;
        }

        if (ret == SSL_WANT_READ || ret == SSL_WANT_WRITE)
            return;

        buffer_pull(b, NULL, ret);

    } else {
#endif
        if (buffer_pull_to_fd(b, w->fd, -1) < 0)
            goto err;
#ifdef SSL_SUPPORT
    }
#endif

    if (buffer_length(b) > 0)
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
    struct rtty *rtty = conn->rtty;
    struct ev_loop *loop = rtty->loop;

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

    if (conn->https) {
#ifdef SSL_SUPPORT
        conn->ssl = ssl_session_new(rtty->ssl_ctx, sock);
        if (!conn->ssl) {
            log_err("SSL session create fail\n");
            send_http_msg(conn, 0, NULL);
        }
#endif
    }
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
    uint8_t addr[18];
    void *data;
    bool https;
    int sock;

    https = buffer_pull_u8(&rtty->rb);
    len -= 1;

#ifndef SSL_SUPPORT
    if (https) {
        buffer_pull(&rtty->rb, NULL, len);
        log_err("SSL not supported\n");
        return;
    }
#endif

    buffer_pull(&rtty->rb, addr, 18);
    len -= 18;

    if (len == 0)
        return;

    conn = find_exist_connection(&rtty->http_conns, addr);
    if (conn) {
        buffer_pull(&rtty->rb, NULL, 6);
        len -= 6;

        data = buffer_put(&conn->wb, len);
        buffer_pull(&rtty->rb, data, len);

        if (conn->sock > 0)
            ev_io_start(rtty->loop, &conn->iow);
        return;
    }

    addrin.sin_addr.s_addr = buffer_pull_u32(&rtty->rb);
    addrin.sin_port = buffer_pull_u16(&rtty->rb);
    len -= 6;

    conn = (struct http_connection *)calloc(1, sizeof(struct http_connection));
    conn->rtty = rtty;
    conn->https = https;
    conn->active = ev_now(rtty->loop);

    memcpy(conn->addr, addr, 18);

    data = buffer_put(&conn->wb, len);
    buffer_pull(&rtty->rb, data, len);

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
