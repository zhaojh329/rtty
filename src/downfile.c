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
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "file.h"
#include "utils.h"

static struct buffer rb;
static struct buffer wb;
static uint32_t remain;
static uint32_t file_size;
static ev_tstamp start_time;
static struct ev_io iow;
static struct ev_io ior;
static struct ev_signal sw;
static char *file_name;
static bool canceled;

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
    canceled = true;
    ev_signal_stop (loop, w);
}

static void on_file_write(struct ev_loop *loop, struct ev_io *w, int revents)
{
    int ret = buffer_pull_to_fd (&wb, w->fd, -1, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr,"socket write error: %s\n", strerror(errno));
        return;
    }

    remain -= ret;

    fprintf (stdout, "%100c\r", ' ');
    fprintf(stdout,"   %lu%%    %s     %.3lfs\r",
            (file_size - remain) * 100UL / file_size,
            format_size(file_size - remain), ev_now(loop) - start_time);
    fflush (stdout);

    if (buffer_length (&wb) < 1)
        ev_io_stop (loop, w);
}

static void start_write_file(struct ev_loop *loop, int msglen)
{
    int fd;

    remain = file_size = buffer_pull_u32be (&rb);
    file_name = strndup (buffer_data (&rb), msglen - 4);
    buffer_pull (&rb, NULL, msglen - 4);

    printf ("Transferring '%s'...\n", file_name);

    fd = open (file_name, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd < 0) {
        printf ("%s\n", strerror (errno));
        return;
    }

    ev_io_init(&iow, on_file_write, fd, EV_WRITE);
}

static void parse_msg(struct ev_loop *loop)
{
    int type;
    int len;

    while (true) {
        if (buffer_length (&rb) < 3)
            return;

        len = buffer_get_u16 (&rb, 1);
        if (buffer_length (&rb) < len + 3)
            return;

        type = buffer_pull_u8 (&rb);
        buffer_pull (&rb, NULL, 2);

        switch (type) {
        case RTTY_FILE_MSG_INFO:
            start_write_file(loop, len);
            break;

        case RTTY_FILE_MSG_DATA:
            if (len == 0) {
                ev_io_stop (loop, &ior);
                ev_signal_stop (loop, &sw);
                return;
            }
            buffer_put_data (&wb, buffer_data (&rb), len);
            buffer_pull (&rb, NULL, len);
            ev_io_start (loop, &iow);
            break;

        case RTTY_FILE_MSG_CANCELED:
            canceled = true;
            ev_break (loop, EVBREAK_ALL);
            break;

        default:
            break;
        }
    }
}

static void on_net_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    bool eof;
    int ret;

    if (buffer_length (&wb) > 0)
        return;

    if (canceled) {
        ev_io_stop (loop, w);
        return;
    }

    ret = buffer_put_fd(&rb, w->fd, -1, &eof);
    if (ret < 0) {
        fprintf (stderr, "socket read error: %s\n", strerror (errno));
        return;
    }

    parse_msg(loop);

    if (eof) {
        printf("busy\n");
        ev_break (loop, EVBREAK_ALL);
    }
}

void download_file()
{
    struct ev_loop *loop = EV_DEFAULT;
    int sock = -1;

    sock = connect_rtty_file_service();
    if (sock < 0)
        goto done;

    detect_sid('d');

    ev_signal_init(&sw, signal_cb, SIGINT);
    ev_signal_start(loop, &sw);

    ev_io_init(&ior, on_net_read, sock, EV_READ);
    ev_io_start (loop, &ior);

    start_time = ev_now (loop);

    printf("Waiting to receive. Press Ctrl+C to cancel\n");

    ev_run(loop, 0);

    puts("");

    if (iow.fd > 0) {
        close (iow.fd);

        if (canceled) {
            unlink (file_name);
            buffer_free(&wb);
            buffer_put_u8 (&wb, RTTY_FILE_MSG_CANCELED);
            buffer_put_u16 (&wb, 0);
            buffer_pull_to_fd (&wb, sock, -1, NULL, NULL);
        }
    }

done:
    if (sock > -1)
        close(sock);
}
