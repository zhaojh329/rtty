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

#ifndef _MESSAGE_H
#define _MESSAGE_H

#include <uwsc/uwsc.h>

#include "rtty.pb-c.h"

static inline void rtty_message_set_data(RttyMessage *msg, void *data, int len)
{
    ProtobufCBinaryData *mdata = &msg->data;

    msg->has_data = true;
    mdata->data = data;
    mdata->len = len;
}

static inline void rtty_message_set_code(RttyMessage *msg, int code)
{
    msg->has_code = true;
    msg->code = code;
}

static inline void rtty_message_set_id(RttyMessage *msg, int id)
{
    msg->has_id = true;
    msg->id = id;
}

static inline void rtty_message_set_err(RttyMessage *msg, int err)
{
    msg->has_err = true;
    msg->err = err;
}

RttyMessage *rtty_message_init(RttyMessage__Type type, const char *sid);
void rtty_message_send(struct uwsc_client *cl, RttyMessage *msg);

#endif