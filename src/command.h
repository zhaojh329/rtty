/*
 * Copyright (C) 2017 Jianhui Zhao <jianhuizhao329@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef _COMMAND_H
#define _COMMAND_H

#include <uwsc/uwsc.h>

#include "json.h"

#define RTTY_CMD_MAX_RUNNING     5
#define RTTY_CMD_EXEC_TIMEOUT    5

enum {
	RTTY_CMD_ERR_PERMIT = 1,
	RTTY_CMD_ERR_NOT_FOUND,
	RTTY_CMD_ERR_NOMEM,
	RTTY_CMD_ERR_SYSERR,
	RTTY_CMD_ERR_RESP_TOOBIG
};

struct task {
    struct list_head list;
    struct uwsc_client *ws;
    struct ev_child cw;
    struct ev_timer timer;
    struct ev_io ioo;   /* Watch stdout of child */
    struct ev_io ioe;   /* Watch stderr of child */
    struct buffer ob;   /* buffer for stdout */
    struct buffer eb;   /* buffer for stderr */
    const json_value *msg;  /* message from server */
    const json_value *attrs;
    int id;
    char cmd[0];
};

void run_command(struct uwsc_client *ws, const json_value *msg);

#endif
