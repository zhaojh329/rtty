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

#include "rtty.h"

enum {
    RTTY_FILE_MSG_START_DOWNLOAD,
    RTTY_FILE_MSG_INFO,
    RTTY_FILE_MSG_DATA,
    RTTY_FILE_MSG_CANCELED
};

struct rtty_file_context {
    int sock;
    int sid;
    bool running;
    struct buffer rb;
    struct buffer wb;
    struct ev_io ior;
    struct ev_io iow;
    struct rtty *rtty;
};

int start_file_service(struct rtty *rtty);
bool detect_file_msg(uint8_t *buf, int len, int sid, int *type);
void recv_file(struct buffer *b, int len);
int connect_rtty_file_service();
void detect_sid(char type);

#endif

