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

#include "message.h"

void command_init();
void run_command(struct uwsc_client *ws, RttyMessage *msg, void *data, uint32_t len);
struct blob_buf *make_command_reply(uint32_t id, int err);
void send_command_reply(struct blob_buf *buf, struct uwsc_client *ws);

#endif
