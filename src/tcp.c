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

#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "rtty.h"
#include "tcp.h"

#define TCP_WINDOW_SIZE (256 * 1024)
#define TCP_MAX_DATA_SIZE (32 * 1024)
#define TCP_QUEUE_HIGH TCP_WINDOW_SIZE
#define TCP_QUEUE_LOW (TCP_QUEUE_HIGH / 2)

enum {
    TCP_OPEN,
    TCP_OPEN_RESULT,
    TCP_DATA,
    TCP_CLOSE_WRITE,
    TCP_CLOSE,
    TCP_ACK
};

enum {
    TCP_OPEN_OK,
    TCP_OPEN_FAILED
};

struct tcp_connection {
    uint8_t id[32];
    struct list_head node;
    struct rtty *rtty;
    struct ev_io ior;
    struct ev_io iow;
    struct ev_timer timer;
    struct buffer wb;
    uint32_t unacked;
    bool connecting;
    bool read_eof;
    bool half;
    bool write_closed;
};

void tcp_update_readers(struct rtty *rtty)
{
    struct tcp_connection *conn;
    size_t queued = buffer_length(&rtty->wb);

    if (queued >= TCP_QUEUE_HIGH)
        rtty->tcp_read_paused = true;
    else if (queued <= TCP_QUEUE_LOW)
        rtty->tcp_read_paused = false;

    list_for_each_entry(conn, &rtty->tcp_conns, node) {
        if (rtty->tcp_read_paused || conn->connecting || conn->read_eof ||
            conn->unacked == TCP_WINDOW_SIZE)
            ev_io_stop(rtty->loop, &conn->ior);
        else
            ev_io_start(rtty->loop, &conn->ior);
    }
}

static int tcp_send(struct rtty *rtty, const uint8_t *id, uint8_t op,
                    const void *data, size_t len)
{
    uint8_t *msg;
    uint16_t size = htobe16(33 + len);

    msg = buffer_put(&rtty->wb, 36 + len);
    if (!msg)
        return -1;

    msg[0] = MSG_TYPE_TCP;
    memcpy(msg + 1, &size, sizeof(size));
    memcpy(msg + 3, id, 32);
    msg[35] = op;
    if (len)
        memcpy(msg + 36, data, len);

    ev_io_start(rtty->loop, &rtty->iow);
    tcp_update_readers(rtty);

    return 0;
}

static int tcp_close(struct tcp_connection *conn, bool notify)
{
    struct rtty *rtty = conn->rtty;
    int ret = 0;

    ev_io_stop(rtty->loop, &conn->ior);
    ev_io_stop(rtty->loop, &conn->iow);
    ev_timer_stop(rtty->loop, &conn->timer);
    close(conn->ior.fd);
    list_del(&conn->node);
    buffer_free(&conn->wb);

    if (notify)
        ret = tcp_send(rtty, conn->id, TCP_CLOSE, NULL, 0);

    free(conn);

    return ret;
}

static void tcp_on_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct tcp_connection *conn = container_of(w, struct tcp_connection, ior);
    struct rtty *rtty = conn->rtty;
    uint8_t data[TCP_MAX_DATA_SIZE];
    size_t len = TCP_WINDOW_SIZE - conn->unacked;
    ssize_t n;

    tcp_update_readers(rtty);
    if (!ev_is_active(w))
        return;

    if (len > sizeof(data))
        len = sizeof(data);

    n = recv(w->fd, data, len, 0);
    if (n < 0) {
        if (errno == EINTR)
            return;

        if (tcp_close(conn, true) < 0)
            rtty_exit(rtty);
        return;
    }

    if (n == 0) {
        conn->read_eof = true;
        ev_io_stop(loop, w);

        if (tcp_send(rtty, conn->id, TCP_CLOSE_WRITE, NULL, 0) < 0) {
            rtty_exit(rtty);
            return;
        }

        if (conn->write_closed)
            tcp_close(conn, false);
        return;
    }

    conn->unacked += n;
    if (tcp_send(rtty, conn->id, TCP_DATA, data, n) < 0)
        rtty_exit(rtty);
}

static void tcp_connected(struct tcp_connection *conn, bool ok)
{
    struct rtty *rtty = conn->rtty;
    uint8_t result = ok ? TCP_OPEN_OK : TCP_OPEN_FAILED;
    int ret;

    ev_io_stop(rtty->loop, &conn->iow);
    ev_timer_stop(rtty->loop, &conn->timer);
    conn->connecting = !ok;

    ret = tcp_send(rtty, conn->id, TCP_OPEN_RESULT, &result, 1);
    if (!ok)
        tcp_close(conn, false);

    if (ret < 0)
        rtty_exit(rtty);
}

static void tcp_on_write(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct tcp_connection *conn = container_of(w, struct tcp_connection, iow);
    struct rtty *rtty = conn->rtty;
    size_t len = buffer_length(&conn->wb);
    uint32_t ack;
    ssize_t n;

    if (conn->connecting) {
        int error = 0;
        socklen_t size = sizeof(error);
        bool ok = getsockopt(w->fd, SOL_SOCKET, SO_ERROR, &error, &size) == 0 && !error;

        tcp_connected(conn, ok);
        return;
    }

    if (len) {
        n = send(w->fd, buffer_data(&conn->wb), len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                return;
            goto error;
        }
        if (!n)
            goto error;

        buffer_pull(&conn->wb, NULL, n);
        ack = htobe32(n);
        if (tcp_send(rtty, conn->id, TCP_ACK, &ack, sizeof(ack)) < 0) {
            rtty_exit(rtty);
            return;
        }
    }

    if (buffer_length(&conn->wb))
        return;

    ev_io_stop(loop, w);
    if (!conn->half)
        return;

    if (shutdown(w->fd, SHUT_WR) < 0)
        goto error;

    conn->write_closed = true;
    if (conn->read_eof)
        tcp_close(conn, false);
    return;

error:
    if (tcp_close(conn, true) < 0)
        rtty_exit(rtty);
}

static void tcp_on_timeout(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    struct tcp_connection *conn = container_of(w, struct tcp_connection, timer);

    tcp_connected(conn, false);
}

static int tcp_open(struct rtty *rtty, const uint8_t *data)
{
    struct sockaddr_in addr = { .sin_family = AF_INET };
    struct tcp_connection *conn;
    uint8_t result = TCP_OPEN_FAILED;
    int sock;
    int ret;

    sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sock < 0)
        return tcp_send(rtty, data, TCP_OPEN_RESULT, &result, 1);

    conn = calloc(1, sizeof(*conn));
    if (!conn) {
        close(sock);
        return tcp_send(rtty, data, TCP_OPEN_RESULT, &result, 1);
    }

    conn->rtty = rtty;
    conn->connecting = true;
    memcpy(conn->id, data, sizeof(conn->id));
    ev_io_init(&conn->ior, tcp_on_read, sock, EV_READ);
    ev_io_init(&conn->iow, tcp_on_write, sock, EV_WRITE);
    ev_timer_init(&conn->timer, tcp_on_timeout, 3, 0);
    buffer_set_limit(&conn->wb, TCP_WINDOW_SIZE);
    list_add_tail(&conn->node, &rtty->tcp_conns);

    memcpy(&addr.sin_addr, data + 33, 4);
    memcpy(&addr.sin_port, data + 37, 2);
    ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        ret = tcp_send(rtty, conn->id, TCP_OPEN_RESULT, &result, 1);
        tcp_close(conn, false);
        return ret;
    }

    /* Complete even an immediate connection through the event loop. */
    ev_io_start(rtty->loop, &conn->iow);
    ev_timer_start(rtty->loop, &conn->timer);

    return 0;
}

int tcp_handle_msg(struct rtty *rtty, const uint8_t *data, size_t len)
{
    struct tcp_connection *conn = NULL;
    struct tcp_connection *entry;
    size_t pending;
    uint32_t ack;
    uint8_t op;

    if (len < 33)
        return -1;

    op = data[32];
    list_for_each_entry(entry, &rtty->tcp_conns, node) {
        if (!memcmp(entry->id, data, 32)) {
            conn = entry;
            break;
        }
    }

    if (!conn && op != TCP_OPEN)
        return 0;

    if (conn && conn->connecting && op != TCP_OPEN && op != TCP_CLOSE)
        return tcp_close(conn, true);

    len -= 33;

    switch (op) {
    case TCP_OPEN:
        if (len != 6 || conn)
            return -1;

        return tcp_open(rtty, data);

    case TCP_CLOSE:
        if (len)
            return -1;

        return tcp_close(conn, false);

    case TCP_DATA:
        pending = buffer_length(&conn->wb);
        if (!len || len > TCP_MAX_DATA_SIZE || conn->half || len > TCP_WINDOW_SIZE - pending)
            return tcp_close(conn, true);

        /* Reuse consumed space before growing the bounded receive buffer. */
        if (buffer_tailroom(&conn->wb) < len && buffer_headroom(&conn->wb)) {
            memmove(conn->wb.head, buffer_data(&conn->wb), pending);
            conn->wb.data = conn->wb.head;
            conn->wb.tail = conn->wb.data + pending;
        }

        if (!buffer_put_data(&conn->wb, data + 33, len))
            return tcp_close(conn, true);

        ev_io_start(rtty->loop, &conn->iow);
        break;

    case TCP_CLOSE_WRITE:
        if (len || conn->half)
            return tcp_close(conn, true);

        conn->half = true;
        ev_io_start(rtty->loop, &conn->iow);
        break;

    case TCP_ACK:
        if (len != sizeof(ack))
            return tcp_close(conn, true);

        memcpy(&ack, data + 33, sizeof(ack));
        ack = be32toh(ack);
        if (!ack || ack > conn->unacked)
            return tcp_close(conn, true);

        conn->unacked -= ack;
        tcp_update_readers(rtty);
        break;

    default:
        return tcp_close(conn, true);
    }

    return 0;
}

void tcp_conns_free(struct rtty *rtty)
{
    struct tcp_connection *conn, *next;

    list_for_each_entry_safe(conn, next, &rtty->tcp_conns, node)
        tcp_close(conn, false);

    rtty->tcp_read_paused = false;
}
