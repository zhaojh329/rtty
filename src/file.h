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

#ifndef RTTY_FILE_H
#define RTTY_FILE_H

#include <ev.h>
#include <sys/un.h>

#include "buffer.h"

enum {
    RTTY_FILE_MSG_START_DOWNLOAD,
    RTTY_FILE_MSG_INFO,
    RTTY_FILE_MSG_DATA,
    RTTY_FILE_MSG_CANCELED,
    RTTY_FILE_MSG_BUSY,
    RTTY_FILE_MSG_PROGRESS,
    RTTY_FILE_MSG_REQUEST_ACCEPT,
    RTTY_FILE_MSG_SAVE_PATH,
    RTTY_FILE_MSG_NO_SPACE
};

struct file_context {
    int sid;
    int fd;
    int sock;
    bool busy;
    uint32_t total_size;
    uint32_t remain_size;
    struct sockaddr_un peer_sun;
    struct ev_io ios;  /* used for unix socket */
    struct ev_io iof;  /* used for upload file */
    ev_tstamp last_notify_progress;
};

int start_file_service(struct file_context *ctx);

void parse_file_msg(struct file_context *ctx, int type, struct buffer *data, int len);

void update_progress(struct ev_loop *loop, ev_tstamp start_time, struct buffer *info);

void cancel_file_operation(struct ev_loop *loop, int sock);

int read_file_msg(int sock, struct buffer *out);

int connect_rtty_file_service();

void request_transfer_file();
bool detect_file_operation(uint8_t *buf, int len, int sid, struct file_context *ctx);

#endif

