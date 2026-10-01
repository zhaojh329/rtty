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

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <poll.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <termios.h>
#include <time.h>

#include "file.h"
#include "term.h"
#include "log/log.h"

#define FILE_BLOCK_SIZE (63 * 1024)
#define FILE_HANDSHAKE_TIMEOUT 5

static void reset_transfer(struct file_context *ctx)
{
    struct tty_term *term = container_of(ctx, struct tty_term, file);
    struct ev_loop *loop = term->tty.rtty->loop;

    ev_timer_stop(loop, &ctx->timer);

    if (ctx->fd >= 0)
        close(ctx->fd);

    if (ctx->temporary[0] && unlinkat(ctx->dirfd, ctx->temporary, 0) < 0)
        log_err("unlinkat: %s\n", strerror(errno));

    if (ctx->dirfd >= 0)
        close(ctx->dirfd);

    if (ctx->ctlfd >= 0) {
        ev_io_stop(loop, &ctx->ior);
        close(ctx->ctlfd);
    }

    ctx->fd = ctx->dirfd = ctx->ctlfd = -1;
    ctx->temporary[0] = 0;
    ctx->state = FILE_IDLE;
    ctx->progress_pending = false;
}

static int send_file_msg(struct file_context *ctx, uint8_t type, const void *data, size_t len)
{
    struct tty_term *term = container_of(ctx, struct tty_term, file);
    struct tty *tty = &term->tty;
    struct rtty *rtty = tty->rtty;
    uint8_t *msg;

    msg = buffer_put(&rtty->wb, 36 + len);
    if (!msg)
        return -1;

    msg[0] = MSG_TYPE_FILE;
    msg[1] = (33 + len) >> 8;
    msg[2] = 33 + len;
    memcpy(msg + 3, tty->sid, 32);
    msg[35] = type;
    if (len)
        memcpy(msg + 36, data, len);

    ev_io_start(rtty->loop, &rtty->iow);
    return 0;
}

static void finish_transfer(struct file_context *ctx, int error, bool notify_remote)
{
    uint32_t code = htonl(error);
    uint8_t byte;

    if (ctx->ctlfd < 0)
        return;

    if (notify_remote && ctx->state != FILE_HANDSHAKE)
        send_file_msg(ctx, RTTY_FILE_MSG_ABORT, NULL, 0);

    /* Stop new acknowledgments and discard pending ones before closing.
     * Closing with unread packets would reset the CLI before it reads DONE. */
    shutdown(ctx->ctlfd, SHUT_RD);
    while (recv(ctx->ctlfd, &byte, sizeof(byte), MSG_TRUNC) > 0)
        ;

    if (error)
        file_ipc_send(ctx->ctlfd, FILE_IPC_ERROR, &code, sizeof(code), -1);
    else
        file_ipc_send(ctx->ctlfd, FILE_IPC_DONE, NULL, 0, -1);

    reset_transfer(ctx);
}

static int notify_info(struct file_context *ctx)
{
    uint8_t data[4 + NAME_MAX];
    uint32_t size = htonl(ctx->total);
    size_t len = strlen(ctx->name);

    memcpy(data, &size, sizeof(size));
    memcpy(data + 4, ctx->name, len);
    return file_ipc_send(ctx->ctlfd, FILE_IPC_INFO, data, 4 + len, -1);
}

static int notify_progress(struct file_context *ctx)
{
    uint32_t remaining = htonl(ctx->remaining);

    /* Only one unacknowledged update: a stopped CLI cannot fill the socket. */
    if (ctx->progress_pending)
        return 0;

    if (file_ipc_send(ctx->ctlfd, FILE_IPC_PROGRESS, &remaining, sizeof(remaining), -1) < 0)
        return -1;

    ctx->progress_pending = true;
    return 0;
}

static int prepare_source(struct file_context *ctx)
{
    struct stat st;
    char link[64];
    char resolved[PATH_MAX + 1];
    const char *name;
    ssize_t len;

    if (fstat(ctx->fd, &st) < 0)
        return -1;

    if (!S_ISREG(st.st_mode)) {
        errno = EINVAL;
        return -1;
    }

    if ((uint64_t)st.st_size > UINT32_MAX) {
        errno = EFBIG;
        return -1;
    }

    /* Preserve the existing download name, without reopening the resolved path. */
    snprintf(link, sizeof(link), "/proc/self/fd/%d", ctx->fd);
    len = readlink(link, resolved, sizeof(resolved) - 1);
    if (len < 0)
        return -1;

    if (len == sizeof(resolved) - 1) {
        errno = ENAMETOOLONG;
        return -1;
    }

    resolved[len] = 0;
    name = strrchr(resolved, '/');
    name = name ? name + 1 : resolved;
    if (!file_valid_name(name, strlen(name))) {
        errno = EINVAL;
        return -1;
    }

    strcpy(ctx->name, name);
    ctx->total = ctx->remaining = st.st_size;
    return 0;
}

static int check_space(struct file_context *ctx)
{
    struct statfs fs;
    struct statvfs space;
    uint64_t available;

    if (fstatfs(ctx->dirfd, &fs) < 0)
        return -1;

    if (fs.f_type == RAMFS_MAGIC) {
        struct sysinfo info;

        if (sysinfo(&info) < 0)
            return -1;

        available = (uint64_t)info.freeram * info.mem_unit;
    } else {
        if (fstatvfs(ctx->dirfd, &space) < 0)
            return -1;

        available = (uint64_t)space.f_bavail * space.f_frsize;
    }

    if (ctx->total > available) {
        errno = ENOSPC;
        return -1;
    }

    return 0;
}

static int prepare_receive_file(struct file_context *ctx, const uint8_t *data, size_t len)
{
    struct stat st;
    char temporary[sizeof(ctx->temporary)];
    struct timespec now;
    uint32_t size;

    if (len < 5 || !file_valid_name(data + 4, len - 4)) {
        errno = EPROTO;
        return -1;
    }

    memcpy(&size, data, 4);
    ctx->total = ctx->remaining = ntohl(size);
    memcpy(ctx->name, data + 4, len - 4);
    ctx->name[len - 4] = 0;

    if (!fstatat(ctx->dirfd, ctx->name, &st, AT_SYMLINK_NOFOLLOW)) {
        errno = EEXIST;
        return -1;
    }

    if (errno != ENOENT || check_space(ctx) < 0)
        return -1;

    clock_gettime(CLOCK_MONOTONIC, &now);
    snprintf(temporary, sizeof(temporary), ".rtty-%ld-%ld.part", (long)getpid(), now.tv_nsec);

    ctx->fd = openat(ctx->dirfd, temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (ctx->fd < 0)
        return -1;

    strcpy(ctx->temporary, temporary);
    return 0;
}

static int check_connection(struct file_context *ctx)
{
    struct pollfd pfd = { .fd = ctx->ctlfd, .events = POLLRDHUP };

    if (poll(&pfd, 1, 0) < 0)
        return -1;

    if (pfd.revents & (POLLRDHUP | POLLHUP | POLLERR)) {
        errno = ECANCELED;
        return -1;
    }

    return 0;
}

static int commit_receive_file(struct file_context *ctx)
{
    int fd;

    if (check_connection(ctx) < 0)
        return -1;

    if (fchown(ctx->fd, ctx->uid, ctx->gid) < 0)
        log_err("fchown '%s': %s\n", ctx->name, strerror(errno));

    if (fchmod(ctx->fd, ctx->create_mode) < 0)
        return -1;

    if (fsync(ctx->fd) < 0)
        return -1;

    fd = ctx->fd;
    ctx->fd = -1;

    if (close(fd) < 0)
        return -1;

    if (check_connection(ctx) < 0)
        return -1;

#ifdef SYS_renameat2
    if (!syscall(SYS_renameat2, ctx->dirfd, ctx->temporary, ctx->dirfd, ctx->name, 1 /* RENAME_NOREPLACE */)) {
        ctx->temporary[0] = 0;
        return 0;
    }

    if (errno != ENOSYS && errno != EINVAL && errno != EOPNOTSUPP)
        return -1;
#endif

    if (linkat(ctx->dirfd, ctx->temporary, ctx->dirfd, ctx->name, 0) < 0)
        return -1;

    return 0;
}

static int send_file_data(struct file_context *ctx)
{
    uint8_t data[FILE_BLOCK_SIZE];
    size_t want = ctx->remaining < sizeof(data) ? ctx->remaining : sizeof(data);
    ssize_t len = read(ctx->fd, data, want);

    if (len < 0)
        return -1;

    if (!len && want) {
        errno = EIO;
        return -1;
    }

    if (send_file_msg(ctx, RTTY_FILE_MSG_DATA, data, len) < 0)
        return -1;

    ctx->remaining -= len;

    if (!len) {
        finish_transfer(ctx, 0, false);
        return 0;
    }

    return notify_progress(ctx);
}

static int receive_data(struct file_context *ctx, const uint8_t *data, size_t len)
{
    size_t offset = 0;

    while (offset < len) {
        ssize_t ret = write(ctx->fd, data + offset, len - offset);

        if (ret <= 0) {
            if (!ret)
                errno = EIO;
            return -1;
        }

        offset += ret;
    }

    ctx->remaining -= len;

    if (!ctx->remaining) {
        if (commit_receive_file(ctx) < 0)
            return -1;

        finish_transfer(ctx, 0, false);
        return 0;
    }

    if (notify_progress(ctx) < 0)
        return -1;

    return send_file_msg(ctx, RTTY_FILE_MSG_ACK, NULL, 0);
}

static int start_transfer(struct file_context *ctx, struct file_packet *packet)
{
    struct tty_term *term = container_of(ctx, struct tty_term, file);
    struct stat st;

    if (packet->len || packet->fd < 0)
        goto invalid;

    switch (packet->type) {
    case FILE_IPC_SEND:
        ctx->fd = packet->fd;
        packet->fd = -1;
        if (prepare_source(ctx) < 0 || notify_info(ctx) < 0)
            return -1;

        ctx->state = FILE_SEND_ACK;
        if (send_file_msg(ctx, RTTY_FILE_MSG_SEND, ctx->name, strlen(ctx->name)) < 0)
            return -1;
        break;

    case FILE_IPC_RECV:
        if (fstat(packet->fd, &st) < 0)
            return -1;

        if (!S_ISDIR(st.st_mode))
            goto invalid;

        ctx->dirfd = packet->fd;
        packet->fd = -1;
        ctx->state = FILE_RECV_INFO;
        if (send_file_msg(ctx, RTTY_FILE_MSG_RECV, NULL, 0) < 0)
            return -1;
        break;

    default:
        goto invalid;
    }

    ev_timer_stop(term->tty.rtty->loop, &ctx->timer);
    return 0;

invalid:
    errno = EPROTO;
    return -1;
}

static void on_local_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct file_context *ctx = container_of(w, struct file_context, ior);
    struct tty_term *term = container_of(ctx, struct tty_term, file);
    struct file_packet packet;
    int ret;
    int error = 0;

    ret = file_ipc_recv(ctx->ctlfd, &packet);
    if (ret <= 0) {
        finish_transfer(ctx, ret < 0 ? errno : ECANCELED, true);
        return;
    }

    ev_timer_again(loop, &term->tmr);
    if (ctx->state == FILE_HANDSHAKE) {
        if (start_transfer(ctx, &packet) < 0)
            error = errno;
    } else if (packet.type == FILE_IPC_PROGRESS && !packet.len && packet.fd < 0 && ctx->progress_pending) {
        ctx->progress_pending = false;
    } else {
        error = EPROTO;
    }

    if (packet.fd >= 0)
        close(packet.fd);

    if (error)
        finish_transfer(ctx, error, true);
}

static void on_timeout(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    struct file_context *ctx = container_of(w, struct file_context, timer);

    finish_transfer(ctx, ETIMEDOUT, false);
}

static void on_accept(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct file_context *ctx = container_of(w, struct file_context, listener);
    struct tty_term *term = container_of(ctx, struct tty_term, file);
    struct ucred cred;
    socklen_t len = sizeof(cred);
    uint32_t code;
    pid_t session;
    int fd;

    fd = accept4(ctx->listenfd, NULL, NULL, SOCK_CLOEXEC);
    if (fd < 0)
        return;

    session = tcgetsid(term->pty);
    if (session <= 0)
        goto err;

    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0)
        goto err;

    if (cred.pid <= 0 || getsid(cred.pid) != session)
        goto err;

    if (ctx->ctlfd >= 0) {
        code = htonl(EBUSY);
        file_ipc_send(fd, FILE_IPC_ERROR, &code, sizeof(code), -1);
        goto err;
    }

    ctx->ctlfd = fd;
    ctx->uid = cred.uid;
    ctx->gid = cred.gid;
    ctx->state = FILE_HANDSHAKE;

    ev_io_init(&ctx->ior, on_local_read, fd, EV_READ);
    ev_io_start(loop, &ctx->ior);

    ev_timer_set(&ctx->timer, FILE_HANDSHAKE_TIMEOUT, 0);
    ev_timer_start(loop, &ctx->timer);

    if (file_ipc_send(fd, FILE_IPC_ACCEPT, NULL, 0, -1) < 0)
        reset_transfer(ctx);

    return;

err:
    close(fd);
}

void file_context_init(struct file_context *ctx)
{
    struct tty_term *term = container_of(ctx, struct tty_term, file);
    struct sockaddr_un addr;
    mode_t mask = umask(0);
    int len;
    int fd;

    umask(mask);
    memset(ctx, 0, sizeof(*ctx));

    ctx->listenfd = -1;
    ctx->ctlfd = ctx->fd = ctx->dirfd = -1;
    ctx->create_mode = 0644 & ~mask;

    len = file_socket_address(&addr, term->pty);
    if (len < 0)
        return;

    fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        log_err("socket: %s\n", strerror(errno));
        return;
    }

    if (bind(fd, (struct sockaddr *)&addr, len) < 0) {
        log_err("bind: %s\n", strerror(errno));
        goto err;
    }

    if (listen(fd, 4) < 0) {
        log_err("listen: %s\n", strerror(errno));
        goto err;
    }

    ctx->listenfd = fd;
    ev_io_init(&ctx->listener, on_accept, fd, EV_READ);
    ev_io_start(term->tty.rtty->loop, &ctx->listener);
    ev_timer_init(&ctx->timer, on_timeout, 0, 0);
    return;

err:
    close(fd);
}

void file_context_close(struct file_context *ctx)
{
    struct tty_term *term = container_of(ctx, struct tty_term, file);

    reset_transfer(ctx);

    if (ctx->listenfd >= 0) {
        ev_io_stop(term->tty.rtty->loop, &ctx->listener);
        close(ctx->listenfd);
    }

    ctx->listenfd = -1;
}

void parse_file_msg(struct file_context *ctx, const uint8_t *data, size_t len)
{
    struct tty_term *term = container_of(ctx, struct tty_term, file);
    int type;

    if (len < 1) {
        finish_transfer(ctx, EPROTO, true);
        return;
    }

    type = *data++;
    len--;

    if (ctx->state == FILE_IDLE || ctx->state == FILE_HANDSHAKE)
        return;

    ev_timer_again(term->tty.rtty->loop, &term->tmr);

    switch (type) {
    case RTTY_FILE_MSG_INFO:
        if (ctx->state != FILE_RECV_INFO)
            break;

        if (prepare_receive_file(ctx, data, len) < 0) {
            finish_transfer(ctx, errno, true);
            return;
        }

        ctx->state = FILE_RECV_DATA;
        if (notify_info(ctx) < 0)
            finish_transfer(ctx, errno, true);
        return;

    case RTTY_FILE_MSG_DATA:
        if (ctx->state != FILE_RECV_DATA || len > ctx->remaining || (!len && ctx->remaining))
            break;

        if (receive_data(ctx, data, len) < 0)
            finish_transfer(ctx, errno, true);
        return;

    case RTTY_FILE_MSG_ACK:
        if (ctx->state != FILE_SEND_ACK || len)
            break;

        if (send_file_data(ctx) < 0)
            finish_transfer(ctx, errno, true);
        return;

    case RTTY_FILE_MSG_ABORT:
        if (len)
            break;

        finish_transfer(ctx, ECANCELED, false);
        return;
    }

    finish_transfer(ctx, EPROTO, true);
}
