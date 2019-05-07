/*
 * MIT License
 *
 * Copyright (c) 2019 Jianhui Zhao <jianhuizhao329@gmail.com>
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <uwsc/log.h>

#include "file.h"

static void set_stdin(bool raw)
{
    static struct termios ots;
    static bool current_raw;
    struct termios nts;

    if (raw) {
        if (current_raw)
            return;

        current_raw = true;

        tcgetattr(STDIN_FILENO, &ots);

        nts = ots;

        nts.c_iflag = IGNBRK;
        /* No echo, crlf mapping, INTR, QUIT, delays, no erase/kill */
        nts.c_lflag &= ~(ECHO | ICANON | ISIG);
        nts.c_oflag = 0;
        nts.c_cc[VMIN] = 1;
        nts.c_cc[VTIME] = 1;
        tcsetattr(STDIN_FILENO, TCSADRAIN, &nts);
        fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
    } else {
        if (!current_raw)
            return;
        current_raw = false;
        tcsetattr(STDIN_FILENO, TCSADRAIN, &ots);
        fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & ~O_NONBLOCK);
    }
}

static void rf_write(int fd, void *buf, int len)
{
    if (write(fd, buf, len) < 0) {
        uwsc_log_err("Write failed: %s\n", strerror(errno));
        exit(0);
    }
}

static bool parse_file_info(struct transfer_context *tc)
{
    struct buffer *b = &tc->b;
    int len;

    if (buffer_length(b) < 3)
        return false;

    len = buffer_get_u8(b, 1);

    if (buffer_length(b) < len + 2)
        return false;

    buffer_pull(b, NULL, 2);
    buffer_pull(b, tc->name, len);

    tc->size = ntohl(buffer_pull_u32(b));

    return true;
}

static bool parse_file_data(struct transfer_context *tc)
{
    struct buffer *b = &tc->b;
    ev_tstamp n = ev_time();
    char unit = 'K';
    float offset;
    int len;

    if (buffer_length(b) < 3)
        return false;

    len = ntohs(buffer_get_u16(b, 1));

    if (buffer_length(b) < len + 3)
        return false;

    buffer_pull(b, NULL, 3);

    if (tc->fd > 0) {
        buffer_pull_to_fd(b, tc->fd, len, NULL, NULL);
    } else {
        /* skip */
        buffer_pull(b, NULL, len);
        return true;
    }

    tc->offset += len;
    offset = tc->offset / 1024.0;

    if ((int)offset / 1024 > 0) {
        offset = offset / 1024;
        unit = 'M';
    }

    printf("  %d%%   %.3f %cB    %.3fs\r",
        (int)(tc->offset * 1.0 / tc->size * 100), offset, unit, n - tc->ts);
    fflush(stdout);

    return true;
}

static int parse_file(struct transfer_context *tc)
{
    struct buffer *b = &tc->b;
    int type;

    while (buffer_length(b) > 0) {
        type = buffer_get_u8(b, 0);

        switch (type) {
        case 0x01:  /* file info */
            if (!parse_file_info(tc))
                return false;

            tc->ts = ev_time();

            printf("Transferring '%s'...\r\n", tc->name);

            tc->fd = open(tc->name, O_WRONLY | O_TRUNC | O_CREAT, 0644);
            if (tc->fd < 0) {
                char magic_err[] = {0xB6, 0xBC, 'e'};
                printf("Create '%s' failed: %s\r\n", tc->name, strerror(errno));

                usleep(1000);

                rf_write(STDOUT_FILENO, magic_err, 3);
            }

            break;
        case 0x02:  /* file data */
            if (!parse_file_data(tc))
                return false;
            break;
        case 0x03:  /* file eof */
            if (tc->fd > 0) {
                close(tc->fd);
                tc->fd = -1;

                if (tc->mode == RF_RECV)
                    printf("\r\n");
            }
            return true;
        default:
            printf("error type\r\n");
            exit(1);
        }
    }

    return false;
}

static void stdin_read_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct transfer_context *tc = w->data;
    bool eof = false;

    buffer_put_fd(&tc->b, w->fd, -1, &eof, NULL, NULL);

    if (parse_file(tc))
        ev_io_stop(loop, w);
}

static void timer_cb(struct ev_loop *loop, ev_timer *w, int revents)
{
    struct transfer_context *tc = w->data;
    static uint8_t buf[RF_BLK_SIZE + 3];
    int len;

    /* Canceled by user */
    if (tc->fd < 0) {
        buf[0] = 0x03;
        rf_write(STDOUT_FILENO, buf, 1);
        ev_break(loop, EVBREAK_ALL);
        return;
    }

    len = read(tc->fd, buf + 3, RF_BLK_SIZE);
    if (len == 0) {
        buf[0] = 0x03;
        rf_write(STDOUT_FILENO, buf, 1);
        ev_break(loop, EVBREAK_ALL);
        return;
    }

    buf[0] = 0x02;
    *(uint16_t *)&buf[1] = htons(len);
    
    rf_write(STDOUT_FILENO, buf, len + 3);
}

void transfer_file(const char *name)
{
    struct ev_loop *loop = EV_DEFAULT;
    char magic[3] = {0xB6, 0xBC};
    struct transfer_context tc = {};
    const char *bname = "";
    struct ev_timer t;
    struct ev_io w;

    if (name) {
        struct stat st;

        bname = basename(name);
        magic[2] = tc.mode = RF_SEND;

        printf("Transferring '%s'...Press Ctrl+C to cancel\r\n", bname);

        tc.fd = open(name, O_RDONLY);
        if (tc.fd < 0) {
            if (errno == ENOENT) {
                printf("Open '%s' failed: No such file\r\n", name);
                exit(0);
            }

            printf("Open '%s' failed: %s\r\n", name, strerror(errno));
            exit(0);
        }

        fstat(tc.fd, &st);
        tc.size = st.st_size;

        if (!(st.st_mode & S_IFREG)) {
            printf("'%s' is not a regular file\r\n", name);
            exit(0);
        }
    } else {
        magic[2] = tc.mode = RF_RECV;

        printf("rtty waiting to receive. Press Ctrl+C to cancel\n");
    }

    set_stdin(true);

    ev_io_init(&w, stdin_read_cb, STDIN_FILENO, EV_READ);
    ev_io_start(loop, &w);
    w.data = &tc;

    rf_write(STDOUT_FILENO, magic, 3);

    if (tc.mode == RF_SEND) {
        uint8_t info[512] = {0x01};

        ev_timer_init(&t, timer_cb, 0.01, 0.01);
	    ev_timer_start(loop, &t);
        t.data = &tc;

        info[1] = strlen(bname);
        memcpy(info + 2, bname, strlen(bname));
        *(uint32_t *)&info[2 + strlen(bname)] = htonl(tc.size);

        rf_write(STDOUT_FILENO, info, 6 + strlen(bname));
    }

    ev_run(loop, 0);

    set_stdin(false);

    exit(0);
}

