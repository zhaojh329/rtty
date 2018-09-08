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

int rtty_message_file_init(RttyMessage__File **file, const char *name,
    bool dir, uint64_t mtime, uint64_t size)
{
    RttyMessage__File *n_file;
    char *name_buf;

    n_file = calloc(1, sizeof(RttyMessage__File) + strlen(name) + 1);
    if (!n_file) {
        uwsc_log_err("calloc_a failed\n");
        return -1;
    }

    rtty_message__file__init(n_file);

    name_buf = (char *)n_file + sizeof(RttyMessage__File);
    n_file->name = strcpy(name_buf, name);

    n_file->has_dir = true;
    n_file->dir = dir;

    n_file->has_mtime = true;
    n_file->mtime = mtime;

    if (!dir) {
        n_file->has_size = true;
        n_file->size = size;
    }

    *file = n_file;

    return 0;
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
