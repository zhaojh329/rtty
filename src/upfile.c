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

#include "file.h"
#include "utils.h"

static struct buffer b;
static uint32_t remain;
static uint32_t file_size;
static ev_tstamp start_time;
static struct ev_io iow;
static struct ev_io ior;
static struct ev_signal sw;
static bool canceled;

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
    canceled = true;
    ev_signal_stop (loop, w);
}

static void on_net_write(struct ev_loop *loop, struct ev_io *w, int revents)
{
    int ret = buffer_pull_to_fd (&b, w->fd, -1, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr,"socket write error: %s\n", strerror(errno));
        return;
    }

    if (buffer_length (&b) < 1)
        ev_io_stop (loop, w);
}

static void on_file_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    uint8_t buf[4096];
    int ret;

    if (canceled) {
        ev_io_stop (loop, w);

        buffer_put_u8 (&b, RTTY_FILE_MSG_CANCELED);
        buffer_put_u16 (&b, 0);
        ev_io_start (loop, &iow);
        return;
    }

    if (buffer_length (&b) > 4096 * 10)
        return;

    ret = read(w->fd, buf, sizeof (buf));

    buffer_put_u8 (&b, RTTY_FILE_MSG_DATA);
    buffer_put_u16 (&b, ret);
    buffer_put_data (&b, buf, ret);
    ev_io_start (loop, &iow);

    remain -= ret;

    fprintf (stdout, "%100c\r", ' ');
    fprintf(stdout,"   %lu%%    %s     %.3lfs\r",
            (file_size - remain) * 100UL / file_size,
            format_size(file_size - remain), ev_now(loop) - start_time);
    fflush (stdout);

    if (ret == 0) {
        ev_io_stop (loop, w);
        ev_signal_stop (loop, &sw);
    }
}

void upload_file(const char *name)
{
    struct ev_loop *loop = EV_DEFAULT;
    const char *bname = basename(name);
    struct stat st;
    int sock = -1;
    int fd = -1;

    fd = open(name, O_RDONLY);
    if (fd < 0) {
        fprintf (stderr, "open '%s' failed: ", name);
        if (errno == ENOENT)
            fprintf(stderr,"No such file\n");
        else
            fprintf(stderr,"%s\n", strerror (errno));
        return;
    }

    fstat(fd, &st);
    if (!(st.st_mode & S_IFREG)) {
        fprintf(stderr,"'%s' is not a regular file\n", name);
        goto done;
    }

    remain = file_size = st.st_size;

    sock = connect_rtty_file_service();
    if (sock < 0)
        goto done;

    detect_sid('u');

    buffer_put_u8 (&b, RTTY_FILE_MSG_INFO);
    buffer_put_u16 (&b, strlen(bname));
    buffer_put_string (&b, bname);

    ev_signal_init(&sw, signal_cb, SIGINT);
    ev_signal_start(loop, &sw);

    ev_io_init(&iow, on_net_write, sock, EV_WRITE);
    ev_io_start(loop, &iow);

    ev_io_init(&ior, on_file_read, fd, EV_READ);
    ev_io_start (loop, &ior);

    printf("Transferring '%s'...Press Ctrl+C to cancel\n", bname);

    start_time = ev_now (loop);

    ev_run(loop, 0);

    puts("");

done:
    if (sock > -1)
        close(sock);
    if (fd > -1)
        close(fd);
}
