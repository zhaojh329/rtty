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

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "fileipc.h"
#include "log/log.h"

int file_socket_address(struct sockaddr_un *addr, int ttyfd)
{
    unsigned int device;
    int len;

    /* The PTY survives login's setsid(); its original session ID does not. */
    if (ioctl(ttyfd, TIOCGDEV, &device) < 0) {
        log_err("ioctl TIOCGDEV: %s\n", strerror(errno));
        return -1;
    }

    memset(addr, 0, sizeof(*addr));
    addr->sun_family = AF_UNIX;
    len = snprintf(addr->sun_path + 1, sizeof(addr->sun_path) - 1,
                   "rtty.file.tty.%u", device);

    return offsetof(struct sockaddr_un, sun_path) + 1 + len;
}

bool file_valid_name(const void *name, size_t len)
{
    if (!len || len > NAME_MAX || memchr(name, 0, len) || memchr(name, '/', len))
        return false;

    if ((len == 1 && !memcmp(name, ".", 1)) || (len == 2 && !memcmp(name, "..", 2)))
        return false;

    return true;
}

int file_ipc_send(int fd, uint8_t type, const void *data, size_t len, int passed_fd)
{
    uint8_t header[4] = {2, type, len >> 8, len};
    struct iovec iov[] = {
        { .iov_base = header, .iov_len = sizeof(header) },
        { .iov_base = (void *)data, .iov_len = len }
    };
    union {
        struct cmsghdr align;
        char data[CMSG_SPACE(sizeof(int))];
    } control = {};
    struct msghdr msg = { .msg_iov = iov, .msg_iovlen = len ? 2 : 1 };

    if (passed_fd >= 0) {
        struct cmsghdr *cmsg;

        msg.msg_control = control.data;
        msg.msg_controllen = sizeof(control.data);
        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(cmsg), &passed_fd, sizeof(passed_fd));
    }

    return sendmsg(fd, &msg, MSG_NOSIGNAL) < 0 ? -1 : 0;
}

int file_ipc_recv(int fd, struct file_packet *packet)
{
    uint8_t header[4];
    struct iovec iov[] = {
        { .iov_base = header, .iov_len = sizeof(header) },
        { .iov_base = packet->data, .iov_len = sizeof(packet->data) }
    };
    union {
        struct cmsghdr align;
        char data[CMSG_SPACE(sizeof(int))];
    } control;
    struct msghdr msg = {
        .msg_iov = iov, .msg_iovlen = 2,
        .msg_control = control.data, .msg_controllen = sizeof(control.data)
    };
    struct cmsghdr *cmsg;
    bool invalid = false;
    ssize_t ret;

    packet->fd = -1;
    ret = recvmsg(fd, &msg, MSG_CMSG_CLOEXEC);
    if (ret < 0)
        return ret;

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
            invalid = true;
            continue;
        }

        for (size_t offset = 0; offset + sizeof(int) <= cmsg->cmsg_len - CMSG_LEN(0); offset += sizeof(int)) {
            int received;

            memcpy(&received, CMSG_DATA(cmsg) + offset, sizeof(received));
            if (packet->fd < 0) {
                packet->fd = received;
            } else {
                close(received);
                invalid = true;
            }
        }
    }

    if (!ret && packet->fd < 0 && !invalid && !(msg.msg_flags & MSG_CTRUNC))
        return 0;

    if (invalid || ret < sizeof(header) || (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) || header[0] != 2)
        goto invalid;

    packet->type = header[1];
    packet->len = ((unsigned)header[2] << 8) | header[3];
    if (packet->len != ret - sizeof(header))
        goto invalid;

    return 1;

invalid:
    if (packet->fd >= 0)
        close(packet->fd);
    packet->fd = -1;
    errno = EPROTO;
    return -1;
}
