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

#include "log/log.h"
#include "file.h"
#include "list.h"
#include "rtty.h"
#include "utils.h"

static uint8_t RTTY_FILE_MAGIC[] = {0xb6, 0xbc, 0xbd};
static char savepath[PATH_MAX];

static int send_file_control_msg(int fd, int type, void *buf, int len)
{
    struct file_control_msg msg = {
        .type = type
    };

    if (len > sizeof(msg.buf)) {
        len = sizeof(msg.buf);
        log_err("file control msg too long\n");
    }

    if (buf)
        memcpy(msg.buf, buf, len);

    if (write(fd, &msg, sizeof(msg)) < 0)
        return -1;

    return 0;
}

void file_context_reset(struct file_context *ctx)
{
    if (ctx->fd > 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }

    if (ctx->ctlfd > 0) {
        close(ctx->ctlfd);
        ctx->ctlfd = -1;
    }

    ctx->busy = false;

    if (ctx->buf) {
        free(ctx->buf);
        ctx->buf = NULL;
    }
}

static void notify_user_canceled(struct tty *tty)
{
    struct rtty *rtty = tty->rtty;

    buffer_put_u8(&rtty->wb, MSG_TYPE_FILE);
    buffer_put_u16be(&rtty->wb, 33);
    buffer_put_data(&rtty->wb, tty->file.sid, 32);
    buffer_put_u8(&rtty->wb, RTTY_FILE_MSG_ABORT);
    ev_io_start(rtty->loop, &rtty->iow);
}

static int notify_progress(struct file_context *ctx)
{
    if (send_file_control_msg(ctx->ctlfd, RTTY_FILE_MSG_PROGRESS, &ctx->remain_size, 4) < 0)
        return -1;

    return 0;
}

static void send_file_data(struct file_context *ctx)
{
    struct tty *tty = container_of(ctx, struct tty, file);
    struct rtty *rtty = tty->rtty;
    int ret;

    if (!ctx->buf) {
        ctx->buf = malloc(UPLOAD_FILE_BUF_SIZE);
        if (!ctx->buf) {
            log_err("malloc: %s\n", strerror(errno));
            send_file_control_msg(ctx->ctlfd, RTTY_FILE_MSG_ERR, NULL, 0);
            goto err;
        }
    }

    if (ctx->fd < 0)
        return;

    ret = read(ctx->fd, ctx->buf, UPLOAD_FILE_BUF_SIZE);
    if (ret < 0) {
        send_file_control_msg(ctx->ctlfd, RTTY_FILE_MSG_ERR, NULL, 0);
        goto err;
    }

    ctx->remain_size -= ret;

    buffer_put_u8(&rtty->wb, MSG_TYPE_FILE);
    buffer_put_u16be(&rtty->wb, 33 + ret);
    buffer_put_data(&rtty->wb, ctx->sid, 32);
    buffer_put_u8(&rtty->wb, RTTY_FILE_MSG_DATA);
    buffer_put_data(&rtty->wb, ctx->buf, ret);
    ev_io_start(rtty->loop, &rtty->iow);

    if (ret == 0) {
        file_context_reset(ctx);
        return;
    }

    if (notify_progress(ctx) < 0)
        goto err;

    return;

err:
    notify_user_canceled(tty);
    file_context_reset(ctx);
}

static int start_upload_file(struct file_context *ctx, const char *path)
{
    struct tty *tty = container_of(ctx, struct tty, file);
    struct rtty *rtty = tty->rtty;
    const char *name = basename(path);
    struct stat st;
    int fd;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        log_err("open '%s' fail: %s\n", path, strerror(errno));
        return -1;
    }

    fstat(fd, &st);

    buffer_put_u8(&rtty->wb, MSG_TYPE_FILE);
    buffer_put_u16be(&rtty->wb, 33 + strlen(name));
    buffer_put_data(&rtty->wb, ctx->sid, 32);
    buffer_put_u8(&rtty->wb, RTTY_FILE_MSG_SEND);
    buffer_put_string(&rtty->wb, name);
    ev_io_start(rtty->loop, &rtty->iow);

    ctx->fd = fd;
    ctx->total_size = st.st_size;
    ctx->remain_size = st.st_size;

    log_info("upload file: %s, size: %" PRIu64 "\n", path, (uint64_t)st.st_size);

    return 0;
}

bool detect_file_operation(uint8_t *buf, int len, const char *sid, struct file_context *ctx)
{
    struct tty *tty = container_of(ctx, struct tty, file);
    struct rtty *rtty = tty->rtty;
    char fifo_name[128];
    pid_t pid;
    int ctlfd;
    uid_t uid;
    gid_t gid;

    if (len != 12)
        return false;

    if (memcmp(buf, RTTY_FILE_MAGIC, 3))
        return false;

    memcpy(&pid, buf + 4, 4);

    if (!getuid_by_pid(pid, &uid)) {
        kill(pid, SIGTERM);
        return true;
    }

    if (!getgid_by_pid(pid, &gid)) {
        kill(pid, SIGTERM);
        return true;
    }

    sprintf(fifo_name, "/tmp/rtty-file-%d.fifo", pid);

    ctlfd = open(fifo_name, O_WRONLY);
    if (ctlfd < 0) {
        log_err("Could not open fifo %s\n", fifo_name);
        kill(pid, SIGTERM);
        return true;
    }

    if (ctx->busy) {
        send_file_control_msg(ctlfd, RTTY_FILE_MSG_BUSY, NULL, 0);
        close(ctlfd);

        return true;
    }

    ctx->ctlfd = ctlfd;
    strcpy(ctx->sid, sid);

    if (buf[3] == 'R') {
        buffer_put_u8(&rtty->wb, MSG_TYPE_FILE);
        buffer_put_u16be(&rtty->wb, 33);
        buffer_put_data(&rtty->wb, ctx->sid, 32);
        buffer_put_u8(&rtty->wb, RTTY_FILE_MSG_RECV);
        ev_io_start(rtty->loop, &rtty->iow);

        send_file_control_msg(ctlfd, RTTY_FILE_MSG_REQUEST_ACCEPT, NULL, 0);

        memset(savepath, 0, sizeof(savepath));
        getcwd_by_pid(pid, savepath, sizeof(savepath) - 1);
        strcat(savepath, "/");

        ctx->uid = uid;
        ctx->gid = gid;
    } else {
        char path[PATH_MAX] = "";
        char link[128];
        int fd;

        memcpy(&fd, buf + 8, 4);

        sprintf(link, "/proc/%d/fd/%d", pid, fd);

        if (readlink(link, path, sizeof(path) - 1) < 0) {
            log_err("readlink: %s\n", strerror(errno));

            send_file_control_msg(ctlfd, RTTY_FILE_MSG_ERR, NULL, 0);
            close(ctlfd);

            return true;
        }

        send_file_control_msg(ctlfd, RTTY_FILE_MSG_REQUEST_ACCEPT, NULL, 0);

        if (start_upload_file(ctx, path) < 0) {
            send_file_control_msg(ctlfd, RTTY_FILE_MSG_ERR, NULL, 0);
            close(ctlfd);

            return true;
        }
    }

    ctx->busy = true;

    return true;
}

static void start_download_file(struct file_context *ctx, struct buffer *info, int len)
{
    char *name = savepath + strlen(savepath);
    struct mntent *ment;
    struct statvfs sfs;
    char buf[512];
    int fd;

    ctx->total_size = ctx->remain_size = buffer_pull_u32be(info);

    ment = find_mount_point(savepath);
    if (ment) {
        if (statvfs(ment->mnt_dir, &sfs) == 0 && ctx->total_size > sfs.f_bavail * sfs.f_frsize) {
            send_file_control_msg(ctx->ctlfd, RTTY_FILE_MSG_NO_SPACE, NULL, 0);
            log_err("download file fail: no enough space\n");
            goto check_space_fail;
        }
    } else {
        send_file_control_msg(ctx->ctlfd, RTTY_FILE_MSG_NO_SPACE, NULL, 0);
        log_err("download file fail: not found mount point of '%s'\n", savepath);
        goto check_space_fail;
    }

    buffer_pull(info, name, len - 4);

    if (!access(savepath, F_OK)) {
        send_file_control_msg(ctx->ctlfd, RTTY_FILE_MSG_ERR_EXIST, NULL, 0);
        log_err("the file '%s' already exists\n", name);
        goto open_fail;
    }

    fd = open(savepath, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd < 0) {
        send_file_control_msg(ctx->ctlfd, RTTY_FILE_MSG_ERR, NULL, 0);
        log_err("create file '%s' fail: %s\n", name, strerror(errno));
        goto open_fail;
    }

    log_info("download file: %s, size: %u\n", savepath, ctx->total_size);

    if (fchown(fd, ctx->uid, ctx->gid) < 0)
        log_err("fchown %s fail: %s\n", savepath, strerror(errno));

    if (ctx->total_size == 0)
        close(fd);
    else
        ctx->fd = fd;

    memcpy(buf, &ctx->total_size, 4);
    strcpy(buf + 4, name);

    send_file_control_msg(ctx->ctlfd, RTTY_FILE_MSG_INFO, buf, 4 + strlen(name));

    return;

check_space_fail:
    buffer_pull(info, name, len - 4);
open_fail:
    file_context_reset(ctx);
}

static void send_file_data_ack(struct tty *tty)
{
    struct rtty *rtty = tty->rtty;

    buffer_put_u8(&rtty->wb, MSG_TYPE_FILE);
    buffer_put_u16be(&rtty->wb, 33);
    buffer_put_data(&rtty->wb, tty->file.sid, 32);
    buffer_put_u8(&rtty->wb, RTTY_FILE_MSG_ACK);
    ev_io_start(rtty->loop, &rtty->iow);
}

void parse_file_msg(struct file_context *ctx, struct buffer *data, int len)
{
    struct tty *tty = container_of(ctx, struct tty, file);
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
                    file_context_reset(ctx);
                } else {
                    if (ctx->remain_size == 0)
                        file_context_reset(ctx);
                    else
                        send_file_data_ack(tty);
                }
            } else {
                buffer_pull(data, NULL, len);
            }
        } else {
            file_context_reset(ctx);
        }
        break;

    case RTTY_FILE_MSG_ACK:
        send_file_data(ctx);
        break;

    case RTTY_FILE_MSG_ABORT:
        send_file_control_msg(ctx->ctlfd, RTTY_FILE_MSG_ABORT, NULL, 0);
        file_context_reset(ctx);
        break;

    default:
        break;
    }
}
