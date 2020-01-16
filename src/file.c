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

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "log.h"
#include "file.h"
#include "utils.h"

static uint8_t RTTY_FILE_MAGIC[] = {0xb6, 0xbc, 0xbd};
struct rtty_file_context file_context;

static void clear_file_context()
{
    file_context.running = false;
    close(file_context.sock);

    buffer_free (&file_context.rb);
    buffer_free (&file_context.wb);

    ev_io_stop(file_context.rtty->loop, &file_context.ior);
    ev_io_stop(file_context.rtty->loop, &file_context.iow);
}

static void on_file_msg_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct buffer *rb = &file_context.rb;
    struct rtty *rtty = file_context.rtty;
    struct buffer *wb = &rtty->wb;
    int msglen;
    bool eof;
    int ret;

    ret = buffer_put_fd(rb, w->fd, -1, &eof, NULL, NULL);
    if (ret < 0) {
        log_err("socket read error: %s\n", strerror (errno));
        return;
    }

    while (true) {
        if (buffer_length (rb) < 3)
            break;

        msglen = buffer_get_u16 (rb, 1);
        if (buffer_length (rb) < msglen + 3)
            break;

        buffer_put_u8 (wb, MSG_TYPE_FILE);
        buffer_put_u16 (wb, htons(2 + msglen));
        buffer_put_u8 (wb, file_context.sid);
        buffer_put_u8 (wb, buffer_pull_u8 (rb));

        buffer_pull (rb, NULL, 2);

        buffer_put_data (wb, buffer_data (rb), msglen);
        buffer_pull (rb, NULL, msglen);

        ev_io_start(loop, &rtty->iow);
    }

    if (eof)
        clear_file_context ();
}

static void on_file_msg_write(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct buffer *wb = &file_context.wb;
    int ret = buffer_pull_to_fd (wb, w->fd, -1, NULL, NULL);
    if (ret < 0) {
            log_err ("socket write error: %s\n", strerror(errno));
            clear_file_context ();
            return;
        }

    if (buffer_length (wb) < 1)
        ev_io_stop (loop, w);
}

static void on_net_accept(struct ev_loop *loop, struct ev_io *w, int revents)
{
    int sock = accept(w->fd, NULL, NULL);
    if (sock < 0) {
        log_err("accept fail: %s\n", strerror(errno));
        return;
    }

    if (file_context.running) {
        log_err("up/down file busy\n");
        close(sock);
        return;
    }

    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);

    file_context.sock = sock;
    file_context.running = true;

    ev_io_init(&file_context.ior, on_file_msg_read, sock, EV_READ);
    ev_io_start (loop, &file_context.ior);

    ev_io_init(&file_context.iow, on_file_msg_write, sock, EV_WRITE);
}

int start_file_service(struct rtty *rtty)
{
    struct sockaddr_un sun = {
        .sun_family = AF_UNIX
    };
    const int on = 1;
    int sock;

    if (strlen(RTTY_FILE_UNIX_SOCKET) >= sizeof(sun.sun_path)) {
        log_err("unix socket path too long\n");
        return -1;
    }

    sock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sock < 0) {
        log_err("create socket fail: %s\n", strerror(errno));
        return -1;
    }

    strcpy(sun.sun_path, RTTY_FILE_UNIX_SOCKET);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    unlink(RTTY_FILE_UNIX_SOCKET);

    if (bind(sock, (struct sockaddr*)&sun, sizeof(sun)) < 0) {
        log_err("bind socket fail: %s\n", strerror(errno));
        goto err;
    }

    if (listen(sock, SOMAXCONN) < 0) {
        log_err("listen socket fail: %s\n", strerror(errno));
        goto err;
    }

    ev_io_init(&rtty->iof, on_net_accept, sock, EV_READ);
    ev_io_start (rtty->loop, &rtty->iof);

    file_context.rtty = rtty;

    return sock;

err:
    if (sock > -1)
        close (sock);
    return -1;
}

bool detect_file_msg(uint8_t *buf, int len, int sid, int *type)
{
    if (len != 4)
        return false;

    if (memcmp (buf, RTTY_FILE_MAGIC, sizeof (RTTY_FILE_MAGIC)))
        return false;

    if (buf[3] != 'u' && buf[3] != 'd')
        return false;

    file_context.sid = sid;
    *type = -1;

    if (buf[3] == 'd')
        *type = RTTY_FILE_MSG_START_DOWNLOAD;

    return true;
}

void recv_file(struct buffer *b, int len)
{
    struct buffer *wb = &file_context.wb;

    buffer_put_u8 (wb, buffer_pull_u8 (b));
    buffer_put_u16 (wb, len - 1);
    buffer_put_data (wb, buffer_data (b), len - 1);
    buffer_pull (b, NULL, len - 1);

    ev_io_start (file_context.rtty->loop, &file_context.iow);
}

int connect_rtty_file_service()
{
    struct sockaddr_un sun = {
        .sun_family = AF_UNIX
    };
    int sock = -1;

    sock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sock < 0) {
        fprintf(stderr, "create socket fail: %s\n", strerror(errno));
        return -1;
    }

    strcpy(sun.sun_path, RTTY_FILE_UNIX_SOCKET);

    if (connect(sock, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
        fprintf(stderr,"connect to rtty fail: %s\n", strerror(errno));
        goto err;
    }

    return sock;

err:
    if (sock > -1)
        close(sock);
    return -1;
}

void detect_sid(char type)
{
    uint8_t buf[4];

    memcpy(buf, RTTY_FILE_MAGIC, 3);
    buf[3] = type;

    fwrite(buf, 4, 1, stdout);
    fflush(stdout);
    usleep (10000);
}