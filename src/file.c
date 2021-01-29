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
#include <mntent.h>
#include <inttypes.h>
#include <sys/statvfs.h>
#include <linux/limits.h>

#include "log.h"
#include "file.h"
#include "list.h"
#include "rtty.h"
#include "utils.h"

static uint8_t RTTY_FILE_MAGIC[] = {0xb6, 0xbc, 0xbd};
static char abspath[PATH_MAX];

static int send_file_control_msg(int fd, struct file_control_msg *msg)
{
    if (write(fd, msg, sizeof(struct file_control_msg)) < 0) {
        log_err("write fifo: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static void file_context_reset(struct file_context *ctx)
{
    if (ctx->fd > 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }

    close(ctx->ctlfd);

    ctx->busy = false;
}

static void notify_user_canceled(struct rtty *rtty)
{
    buffer_put_u8(&rtty->wb, MSG_TYPE_FILE);
    buffer_put_u16be(&rtty->wb, 2);
    buffer_put_u8(&rtty->wb, rtty->file_context.sid);
    buffer_put_u8(&rtty->wb, RTTY_FILE_MSG_CANCELED);
    ev_io_start(rtty->loop, &rtty->iow);
}

static int notify_progress(struct file_context *ctx)
{
    struct rtty *rtty = container_of(ctx, struct rtty, file_context);
    struct file_control_msg msg = {
        .type = RTTY_FILE_MSG_PROGRESS
    };
    ev_tstamp now = ev_now(rtty->loop);

    if (ctx->remain_size > 0 && now - ctx->last_notify_progress < 0.1)
        return 0;
    ctx->last_notify_progress = now;

    memcpy(msg.buf, &ctx->remain_size, 4);

    if (send_file_control_msg(ctx->ctlfd, &msg) < 0)
        return -1;

    return 0;
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
    if (ret < 0) {
        struct file_control_msg msg = {
            .type = RTTY_FILE_MSG_ERR
        };

        send_file_control_msg(ctx->ctlfd, &msg);
        goto err;
    }

    ctx->remain_size -= ret;

    buffer_put_u8(&rtty->wb, MSG_TYPE_FILE);
    buffer_put_u16be(&rtty->wb, 2 + ret);
    buffer_put_u8(&rtty->wb, ctx->sid);
    buffer_put_u8(&rtty->wb, RTTY_FILE_MSG_DATA);
    buffer_put_data(&rtty->wb, buf, ret);
    ev_io_start(rtty->loop, &rtty->iow);

    if (ret == 0) {
        ev_io_stop(loop, w);
        file_context_reset(ctx);
        return;
    }

    if (notify_progress(ctx) < 0)
        goto err;

    return;

err:
    ev_io_stop(loop, w);
    notify_user_canceled(rtty);
    file_context_reset(ctx);
}

static int start_upload_file(struct file_context *ctx, const char *path)
{
    struct rtty *rtty = container_of(ctx, struct rtty, file_context);
    const char *name = basename(path);
    struct stat st;
    int fd;

    fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        log_err("open '%s' fail\n", path, strerror(errno));
        return -1;
    }

    fstat(fd, &st);

    buffer_put_u8(&rtty->wb, MSG_TYPE_FILE);
    buffer_put_u16be(&rtty->wb, 2 + strlen(name));
    buffer_put_u8(&rtty->wb, ctx->sid);
    buffer_put_u8(&rtty->wb, RTTY_FILE_MSG_INFO);
    buffer_put_string(&rtty->wb, name);
    ev_io_start(rtty->loop, &rtty->iow);

    ev_io_init(&ctx->iof, on_file_read, fd, EV_READ);
    ev_io_start(rtty->loop, &ctx->iof);

    ctx->fd = fd;
    ctx->total_size = st.st_size;
    ctx->remain_size = st.st_size;

    log_info("upload file: %s, size: %" PRIu64 "\n", path, (uint64_t)st.st_size);

    return 0;
}

bool detect_file_operation(uint8_t *buf, int len, int sid, struct file_context *ctx)
{
    struct rtty *rtty = container_of(ctx, struct rtty, file_context);
    struct file_control_msg msg = {};
    char fifo_name[128];
    pid_t pid;
    int ctlfd;

    if (len != 12)
        return false;

    if (memcmp(buf, RTTY_FILE_MAGIC, 3))
        return false;

    memcpy(&pid, buf + 4, 4);

    memset(abspath, 0, sizeof(abspath));
    getcwd_pid(pid, abspath, sizeof(abspath) - 1);

    sprintf(fifo_name, "/tmp/rtty-file-%d.fifo", pid);

    ctlfd = open(fifo_name, O_WRONLY);
    if (ctlfd < 0) {
        log_err("Could not open fifo %s\n", fifo_name);
        kill(pid, SIGTERM);
        return true;
    }

    if (ctx->busy) {
        msg.type = RTTY_FILE_MSG_BUSY;

        send_file_control_msg(ctlfd, &msg);
        close(ctlfd);

        return true;
    }

    if (buf[3] == 'R') {
        msg.type = RTTY_FILE_MSG_REQUEST_ACCEPT;
        
        buffer_put_u8(&rtty->wb, MSG_TYPE_FILE);
        buffer_put_u16be(&rtty->wb, 2);
        buffer_put_u8(&rtty->wb, ctx->sid);
        buffer_put_u8(&rtty->wb, RTTY_FILE_MSG_START_DOWNLOAD);
        ev_io_start(rtty->loop, &rtty->iow);

        send_file_control_msg(ctlfd, &msg);
    } else {
        char path[PATH_MAX] = "";
        char link[128];
        int fd;

        memcpy(&fd, buf + 8, 4);

        sprintf(link, "/proc/%d/fd/%d", pid, fd);

        if (readlink(link, path, sizeof(path) - 1) < 0) {
            log_err("readlink: %s\n", strerror(errno));

            msg.type = RTTY_FILE_MSG_ERR;

            send_file_control_msg(ctlfd, &msg);
            close(ctlfd);

            return true;
        }

        msg.type = RTTY_FILE_MSG_REQUEST_ACCEPT;
        send_file_control_msg(ctlfd, &msg);

        if (start_upload_file(ctx, path) < 0) {
            msg.type = RTTY_FILE_MSG_ERR;

            send_file_control_msg(ctlfd, &msg);
            close(ctlfd);

            return true;
        }
    }

    ctx->sid = sid;
    ctx->ctlfd = ctlfd;
    ctx->busy = true;

    return true;
}

static void start_download_file(struct file_context *ctx, struct buffer *info, int len)
{
    struct rtty *rtty = container_of(ctx, struct rtty, file_context);
    struct file_control_msg msg = {};
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
            msg.type = RTTY_FILE_MSG_NO_SPACE;

            notify_user_canceled(rtty);

            send_file_control_msg(ctx->ctlfd, &msg);

            log_err("download file fail: no enough space\n");
            goto free_name;
        }
    }

    strcat(abspath, "/");
    strcat(abspath, name);

    fd = open(abspath, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd < 0) {
        log_err("create file '%s' fail: %s\n", name, strerror(errno));
        goto free_name;
    }

    log_info("download file: %s, size: %u\n", abspath, ctx->total_size);

    if (ctx->total_size == 0)
        close(fd);
    else
        ctx->fd = fd;

    msg.type = RTTY_FILE_MSG_INFO;
    memcpy(msg.buf, &ctx->total_size, 4);
    strcpy((char *)msg.buf + 4, name);

    send_file_control_msg(ctx->ctlfd, &msg);

free_name:
    free(name);
}

void parse_file_msg(struct file_context *ctx, struct buffer *data, int len)
{
    struct rtty *rtty = container_of(ctx, struct rtty, file_context);
    struct file_control_msg msg = {};
    int type = buffer_pull_u8(data);

    len--;

    switch (type) {
    case RTTY_FILE_MSG_INFO:
        start_download_file(ctx, data, len);
        break;

    case RTTY_FILE_MSG_DATA:
        if (len > 0) {
            if (ctx->fd > -1) {
                buffer_pull_to_fd(data, ctx->fd, len);
                ctx->remain_size -= len;

                if (notify_progress(ctx) < 0) {
                    notify_user_canceled(rtty);
                    file_context_reset(ctx);
                }
            } else {
                buffer_pull(data, NULL, len);
            }
        } else {
            file_context_reset(ctx);
        }
        break;

    case RTTY_FILE_MSG_CANCELED:
        if (ctx->fd > -1) {
            close(ctx->fd);
            ctx->fd = -1;
        }

        msg.type = RTTY_FILE_MSG_CANCELED;
        send_file_control_msg(ctx->ctlfd, &msg);
        close(ctx->ctlfd);

        ctx->busy = false;
        break;

    default:
        break;
    }
}
