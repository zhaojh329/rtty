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
#include "fileipc.h"

enum {
    RTTY_FILE_MSG_SEND,
    RTTY_FILE_MSG_RECV,
    RTTY_FILE_MSG_INFO,
    RTTY_FILE_MSG_DATA,
    RTTY_FILE_MSG_ACK,
    RTTY_FILE_MSG_ABORT
};

enum file_state {
    FILE_IDLE,
    FILE_HANDSHAKE,
    FILE_SEND_ACK,
    FILE_RECV_INFO,
    FILE_RECV_DATA
};

struct file_context {
    struct ev_io listener;
    struct ev_io ior;
    struct ev_timer timer;
    int listenfd;
    int ctlfd;
    int fd;
    int dirfd;
    uid_t uid;
    gid_t gid;
    mode_t create_mode;
    enum file_state state;
    uint32_t total;
    uint32_t remaining;
    bool progress_pending;
    char name[NAME_MAX + 1];
    char temporary[80];
};

int request_transfer_file(char type, const char *path);
void file_context_init(struct file_context *ctx);
void file_context_close(struct file_context *ctx);
void parse_file_msg(struct file_context *ctx, const uint8_t *data, size_t len);

#endif
