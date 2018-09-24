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

#include <stdlib.h>
#include <string.h>
#include <uwsc/log.h>

#include "message.h"

RttyMessage *rtty_message_init(RttyMessage__Type type, const char *sid)
{
    static RttyMessage msg;

    rtty_message__init(&msg);

    msg.has_version = true;
    msg.version = 2;

    msg.has_type = true;
    msg.type = type;

    msg.sid = (char *)sid;

    return &msg;
}

void rtty_message_send(struct uwsc_client *cl, RttyMessage *msg)
{
    int len = rtty_message__get_packed_size(msg);
    void *buf;

    buf = malloc(len);
    if (!buf) {
        uwsc_log_err("malloc failed\n");
        return;
    }

    rtty_message__pack(msg, buf);

    cl->send(cl, buf, len, UWSC_OP_BINARY);

    free(buf);
}
