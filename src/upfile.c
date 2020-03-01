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
#include <stdlib.h>

#include "file.h"

static ev_tstamp start_time;
static char abspath[PATH_MAX];
static uint32_t file_size;
static struct buffer b;
static int sock;

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
    cancel_file_operation(loop, sock);
}

static void on_socket_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    int type = read_file_msg(w->fd, &b);

    switch (type) {
    case RTTY_FILE_MSG_REQUEST_ACCEPT:
        buffer_put_u8(&b, RTTY_FILE_MSG_INFO);
        buffer_put_u32(&b, file_size);
        buffer_put_string(&b, abspath);
        buffer_put_zero(&b, 1);
        buffer_pull_to_fd(&b, w->fd, -1);
        start_time = ev_now(loop);
        break;
    case RTTY_FILE_MSG_PROGRESS:
        update_progress(loop, start_time, &b);
        break;
    case RTTY_FILE_MSG_BUSY:
        fprintf(stderr, "\033[31mRtty is busy\033[0m\n");
        ev_break(loop, EVBREAK_ALL);
        break;
    default:
        break;
    }
}

void upload_file(const char *path)
{
    struct ev_loop *loop = EV_DEFAULT;
    struct ev_signal sw;
    struct ev_io ior;
    struct stat st;
    int fd;

    if (getuid() > 0) {
        fprintf(stderr, "Operation not permitted, must be run as root\n");
        return;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "open '%s' failed: ", path);
        if (errno == ENOENT)
            fprintf(stderr, "No such file\n");
        else
            fprintf(stderr, "%s\n", strerror(errno));
        return;
    }

    fstat(fd, &st);
    if (!(st.st_mode & S_IFREG)) {
        fprintf(stderr, "'%s' is not a regular file\n", path);
        close(fd);
        return;
    }
    close(fd);

    file_size = st.st_size;

    sock = connect_rtty_file_service();
    if (sock < 0)
        return;

    if (!realpath(path, abspath))
        return;

    request_transfer_file();

    ev_io_init(&ior, on_socket_read, sock, EV_READ);
    ev_io_start(loop, &ior);

    ev_signal_init(&sw, signal_cb, SIGINT);
    ev_signal_start(loop, &sw);

    printf("Transferring '%s'...Press Ctrl+C to cancel\n", basename(path));

    ev_run(loop, 0);

    buffer_free(&b);
    close(sock);
}
