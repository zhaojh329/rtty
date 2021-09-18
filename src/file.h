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

#include "buffer.h"

enum {
    RTTY_FILE_MSG_SEND,
    RTTY_FILE_MSG_RECV,
    RTTY_FILE_MSG_INFO,
    RTTY_FILE_MSG_DATA,
    RTTY_FILE_MSG_ACK,
    RTTY_FILE_MSG_ABORT,
    RTTY_FILE_MSG_BUSY,
    RTTY_FILE_MSG_PROGRESS,
    RTTY_FILE_MSG_REQUEST_ACCEPT,
    RTTY_FILE_MSG_NO_SPACE,
    RTTY_FILE_MSG_ERR_EXIST,
    RTTY_FILE_MSG_ERR
};

#define UPLOAD_FILE_BUF_SIZE (1024 * 16)

struct file_control_msg {
    int type;
    uint8_t buf[128];
};

struct file_context {
    int fd;
    bool busy;
    int ctlfd;
    uid_t uid;
    gid_t gid;
    char sid[33];
    uint8_t *buf;
    uint32_t total_size;
    uint32_t remain_size;
};

void request_transfer_file(char type, const char *path);

bool detect_file_operation(uint8_t *buf, int len, const char *sid, struct file_context *ctx);

void parse_file_msg(struct file_context *ctx, struct buffer *data, int len);

void file_context_reset(struct file_context *ctx);

#endif

