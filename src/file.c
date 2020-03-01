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

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/statvfs.h>
#include <mntent.h>

#include "log.h"
#include "file.h"
#include "list.h"
#include "rtty.h"
#include "utils.h"

static uint8_t RTTY_FILE_MAGIC[] = {0xb6, 0xbc, 0xbd};
static char abspath[PATH_MAX];
static struct buffer b;

static void notify_progress(struct file_context *ctx)
{
    struct rtty *rtty = container_of(ctx, struct rtty, file_context);
    ev_tstamp now = ev_now(rtty->loop);

    if (ctx->remain_size > 0 && now - ctx->last_notify_progress < 0.1)
        return;
    ctx->last_notify_progress = now;

    buffer_truncate(&b, 0);
    buffer_put_u8(&b, RTTY_FILE_MSG_PROGRESS);
    buffer_put_u32(&b, ctx->remain_size);
    buffer_put_u32(&b, ctx->total_size);
    sendto(ctx->sock, buffer_data(&b), buffer_length(&b), 0,
           (struct sockaddr *)&ctx->peer_sun, sizeof(struct sockaddr_un));
}

static void on_file_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct file_context *ctx = container_of(w, struct file_context, iof);
    struct rtty *rtty = container_of(ctx, struct rtty, file_context);
    uint8_t buf[4096];
    int ret;

    if (buffer_length(&rtty->wb) > 4096)
        return;

    ret = read(w->fd, buf, sizeof(buf));
    ctx->remain_size -= ret;

    buffer_put_u8(&rtty->wb, MSG_TYPE_FILE);
    buffer_put_u16be(&rtty->wb, 2 + ret);
    buffer_put_u8(&rtty->wb, ctx->sid);
    buffer_put_u8(&rtty->wb, RTTY_FILE_MSG_DATA);
    buffer_put_data(&rtty->wb, buf, ret);
    ev_io_start(rtty->loop, &rtty->iow);

    if (ret == 0) {
        ctx->busy = false;
        ev_io_stop(loop, w);
        close(ctx->fd);
        ctx->fd = -1;
    } else {
        notify_progress(ctx);
    }
}

static void start_upload_file(struct file_context *ctx, struct buffer *info)
{
    struct rtty *rtty = container_of(ctx, struct rtty, file_context);
    uint32_t size = buffer_pull_u32(info);
    const char *path = buffer_data(info);
    const char *name = basename(path);
    int fd;

    fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        log_err("open '%s' fail\n", path, strerror(errno));
        return;
    }

    buffer_put_u8(&rtty->wb, MSG_TYPE_FILE);
    buffer_put_u16be(&rtty->wb, 2 + strlen(name));
    buffer_put_u8(&rtty->wb, ctx->sid);
    buffer_put_u8(&rtty->wb, RTTY_FILE_MSG_INFO);
    buffer_put_string(&rtty->wb, name);
    ev_io_start(rtty->loop, &rtty->iow);

    ev_io_init(&ctx->iof, on_file_read, fd, EV_READ);
    ev_io_start(rtty->loop, &ctx->iof);

    ctx->fd = fd;
    ctx->total_size = size;
    ctx->remain_size = size;

    log_info("upload file: %s\n", path);
}

static void send_canceled_msg(struct rtty *rtty)
{
    buffer_put_u8(&rtty->wb, MSG_TYPE_FILE);
    buffer_put_u16be(&rtty->wb, 2);
    buffer_put_u8(&rtty->wb, rtty->file_context.sid);
    buffer_put_u8(&rtty->wb, RTTY_FILE_MSG_CANCELED);
    ev_io_start(rtty->loop, &rtty->iow);
}

static void start_download_file(struct file_context *ctx, struct buffer *info, int len)
{
    struct rtty *rtty = container_of(ctx, struct rtty, file_context);
    struct mntent *ment;
    struct statvfs sfs;
    char *name;
    int fd;

    ctx->total_size = ctx->remain_size = buffer_pull_u32be(info);
    name = strndup(buffer_data(info), len - 4);
    buffer_pull(info, NULL, len - 4);

    ment = find_mount_point(abspath);
    if (ment) {
        if (statvfs(ment->mnt_dir, &sfs) == 0 && ctx->total_size > sfs.f_bavail * sfs.f_frsize) {
            int type = RTTY_FILE_MSG_NO_SPACE;

            send_canceled_msg(rtty);
            sendto(ctx->sock, &type, 1, 0, (struct sockaddr *) &ctx->peer_sun, sizeof(struct sockaddr_un));
            log_err("download file fail: no enough space\n");
            return;
        }
    }

    strcat(abspath, name);

    fd = open(abspath, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd < 0) {
        log_err("create file '%s' fail: %s\n", name, strerror(errno));
        return;
    }

    ctx->fd = fd;

    log_info("download file: %s, size: %u\n", abspath, ctx->total_size);

    buffer_truncate(&b, 0);
    buffer_put_u8(&b, RTTY_FILE_MSG_INFO);
    buffer_put_string(&b, name);
    buffer_put_zero(&b, 1);
    sendto(ctx->sock, buffer_data(&b), buffer_length(&b), 0,
           (struct sockaddr *)&ctx->peer_sun, sizeof(struct sockaddr_un));
    free(name);
}

static void notify_busy(struct file_context *ctx)
{
    int type = RTTY_FILE_MSG_BUSY;

    log_err("upload file is busy\n");
    sendto(ctx->sock, &type, 1, 0,
           (struct sockaddr *)&ctx->peer_sun, sizeof(struct sockaddr_un));
}

static void on_socket_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct file_context *ctx = container_of(w, struct file_context, ios);
    struct rtty *rtty = container_of(ctx, struct rtty, file_context);
    int type = read_file_msg(w->fd, &b);

    switch (type) {
    case RTTY_FILE_MSG_INFO:
        start_upload_file(ctx, &b);
        break;
    case RTTY_FILE_MSG_CANCELED:
        if (ctx->fd > -1) {
            close(ctx->fd);
            ctx->fd = -1;
            ev_io_stop(loop, &ctx->iof);
        }

        ctx->busy = false;
        send_canceled_msg(rtty);
        break;
    case RTTY_FILE_MSG_SAVE_PATH:
        strcpy(abspath, buffer_data(&b));
        strcat(abspath, "/");
        buffer_put_u8(&rtty->wb, MSG_TYPE_FILE);
        buffer_put_u16be(&rtty->wb, 2);
        buffer_put_u8(&rtty->wb, ctx->sid);
        buffer_put_u8(&rtty->wb, RTTY_FILE_MSG_START_DOWNLOAD);
        ev_io_start(loop, &rtty->iow);
        break;
    default:
        break;
    }
}

int start_file_service(struct file_context *ctx)
{
    struct rtty *rtty = container_of(ctx, struct rtty, file_context);
    struct sockaddr_un sun = {
        .sun_family = AF_UNIX
    };
    int sock;

    if (strlen(RTTY_FILE_UNIX_SOCKET_S) >= sizeof(sun.sun_path)) {
        log_err("unix socket path too long\n");
        return -1;
    }

    sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (sock < 0) {
        log_err("create socket fail: %s\n", strerror(errno));
        return -1;
    }

    strcpy(sun.sun_path, RTTY_FILE_UNIX_SOCKET_S);

    unlink(RTTY_FILE_UNIX_SOCKET_S);

    if (bind(sock, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
        log_err("bind socket fail: %s\n", strerror(errno));
        goto err;
    }

    ev_io_init(&ctx->ios, on_socket_read, sock, EV_READ);
    ev_io_start(rtty->loop, &ctx->ios);

    ctx->fd = -1;
    ctx->sock = sock;
    ctx->peer_sun.sun_family = AF_UNIX;
    strcpy(ctx->peer_sun.sun_path, RTTY_FILE_UNIX_SOCKET_C);

    return sock;

err:
    if (sock > -1)
        close(sock);
    return -1;
}

void parse_file_msg(struct file_context *ctx, int type, struct buffer *data, int len)
{
    switch (type) {
    case RTTY_FILE_MSG_INFO:
        start_download_file(ctx, data, len);
        break;
    case RTTY_FILE_MSG_DATA:
        if (len == 0) {
            close(ctx->fd);
            ctx->fd = -1;
            ctx->busy = false;
            return;
        }
        if (ctx->fd > -1) {
            buffer_pull_to_fd(data, ctx->fd, len);
            ctx->remain_size -= len;
            notify_progress(ctx);
        } else {
            buffer_pull(data, NULL, len);
        }
        break;
    case RTTY_FILE_MSG_CANCELED:
        if (ctx->fd > -1)
            close(ctx->fd);
        ctx->fd = -1;
        ctx->busy = false;
        sendto(ctx->sock, &type, 1, 0, (struct sockaddr *) &ctx->peer_sun, sizeof(struct sockaddr_un));
        break;
    default:
        break;
    }
}

int connect_rtty_file_service()
{
    struct sockaddr_un sun = { .sun_family = AF_UNIX };
    int sock = -1;

    sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (sock < 0) {
        fprintf(stderr, "create socket fail: %s\n", strerror(errno));
        return -1;
    }

    strcpy(sun.sun_path, RTTY_FILE_UNIX_SOCKET_C);

    unlink(RTTY_FILE_UNIX_SOCKET_C);

    if (bind(sock, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
        log_err("bind socket fail: %s\n", strerror(errno));
        goto err;
    }

    strcpy(sun.sun_path, RTTY_FILE_UNIX_SOCKET_S);

    if (connect(sock, (struct sockaddr *) &sun, sizeof(sun)) < 0) {
        fprintf(stderr, "connect to rtty fail: %s\n", strerror(errno));
        goto err;
    }

    return sock;

err:
    if (sock > -1)
        close(sock);
    return -1;
}

void request_transfer_file()
{
    fwrite(RTTY_FILE_MAGIC, sizeof(RTTY_FILE_MAGIC), 1, stdout);
    fflush(stdout);
    usleep(10000);
}

static void accept_file_request(struct file_context *ctx)
{
    int type = RTTY_FILE_MSG_REQUEST_ACCEPT;

    ctx->fd = -1;
    ctx->busy = true;
    ctx->last_notify_progress = 0;
    sendto(ctx->sock, &type, 1, 0,
           (struct sockaddr *)&ctx->peer_sun, sizeof(struct sockaddr_un));
}

bool detect_file_operation(uint8_t *buf, int len, int sid, struct file_context *ctx)
{
    if (len != 3)
        return false;

    if (memcmp(buf, RTTY_FILE_MAGIC, sizeof(RTTY_FILE_MAGIC)))
        return false;

    ctx->sid = sid;

    if (ctx->busy) {
        notify_busy(ctx);
        return true;
    }

    accept_file_request(ctx);

    return true;
}

void update_progress(struct ev_loop *loop, ev_tstamp start_time, struct buffer *info)
{
    uint32_t remain = buffer_pull_u32(info);
    uint32_t total = buffer_pull_u32(info);

    printf("%100c\r", ' ');
    printf("  %lu%%    %s     %.3lfs\r", (total - remain) * 100UL / total,
           format_size(total - remain), ev_now(loop) - start_time);
    fflush(stdout);

    if (remain == 0) {
        ev_break(loop, EVBREAK_ALL);
        puts("");
    }
}

void cancel_file_operation(struct ev_loop *loop, int sock)
{
    int type = RTTY_FILE_MSG_CANCELED;
    send(sock, &type, 1, 0);;
    ev_break(loop, EVBREAK_ALL);
    puts("");
}

int read_file_msg(int sock, struct buffer *out)
{
    uint8_t buf[1024];
    int ret = read(sock, buf, sizeof(buf));
    buffer_truncate(out, 0);
    buffer_put_data(out, buf, ret);
    return buffer_pull_u8(out);
}
