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

#ifndef _COMMAND_H
#define _COMMAND_H

#include "rtty.h"

#define RTTY_CMD_MAX_RUNNING     5
#define RTTY_CMD_EXEC_TIMEOUT    30

enum {
    RTTY_CMD_ERR_PERMIT = 1,
    RTTY_CMD_ERR_NOT_FOUND,
    RTTY_CMD_ERR_NOMEM,
    RTTY_CMD_ERR_SYSERR,
    RTTY_CMD_ERR_RESP_TOOBIG
};

struct task {
    struct list_head list;
    struct rtty *rtty;
    struct ev_child cw;
    struct ev_timer timer;
    struct ev_io ioo;   /* Watch stdout of child */
    struct ev_io ioe;   /* Watch stderr of child */
    struct buffer ob;   /* buffer for stdout */
    struct buffer eb;   /* buffer for stderr */
    uid_t uid;
    int nparams;
    char **params;
    char token[33];
    char cmd[0];
};

void run_command(struct rtty *rtty, const char *data);

#endif
