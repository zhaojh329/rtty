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

#ifndef _FILE_H
#define _FILE_H

#include <uwsc/buffer.h>
#include <ev.h>

#define RF_BLK_SIZE 8912         /* 8KB */

enum {
    RF_SEND = 's',
    RF_RECV = 'r'
};

struct transfer_context {
    int size;
    int offset;
    int mode;
    int fd;
    char name[512];
    ev_tstamp ts;
    struct buffer b;
};

void transfer_file(const char *name);

#endif

