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

#ifndef RTTY_FILEIPC_H
#define RTTY_FILEIPC_H

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>

#define FILE_PACKET_MAX (4 + NAME_MAX)

/* Private local protocol v2: version(1), type(1), big-endian length(2).
 * SEND/RECV carry one source/directory descriptor via SCM_RIGHTS. */
enum {
    FILE_IPC_SEND = 1,
    FILE_IPC_RECV,
    FILE_IPC_ACCEPT,
    FILE_IPC_INFO,       /* size and basename */
    FILE_IPC_PROGRESS,   /* remaining bytes; empty reply acknowledges progress */
    FILE_IPC_DONE,
    FILE_IPC_ERROR       /* errno */
};

struct file_packet {
    int fd;
    uint8_t type;
    uint16_t len;
    uint8_t data[FILE_PACKET_MAX];
};

int file_socket_address(struct sockaddr_un *addr, int ttyfd);
bool file_valid_name(const void *name, size_t len);
int file_ipc_send(int fd, uint8_t type, const void *data, size_t len, int passed_fd);
/* 1: complete packet, 0: disconnected, -1: error (including malformed packets). */
int file_ipc_recv(int fd, struct file_packet *packet);

#endif
