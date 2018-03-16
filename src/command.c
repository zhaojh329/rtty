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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <libubox/ulog.h>
#include <libubox/ustream.h>
#include <libubox/blobmsg_json.h>
#include <libubox/runqueue.h>

#include "command.h"

#define FILE_MAX_SIZE   (4096 * 64)

#define ustream_declare(us, fd, name)                     \
    us.stream.string_data   = true;                       \
    us.stream.r.buffer_len  = 4096;                       \
    us.stream.r.max_buffers = FILE_MAX_SIZE / 4096;   \
    us.stream.notify_read   = process_##name##_read_cb;  \
    us.stream.notify_state  = process_##name##_state_cb; \
    ustream_fd_init(&us, fd);

#define ustream_for_each_read_buffer(stream, ptr, len) \
    for (ptr = ustream_get_read_buf(stream, &len);     \
         ptr != NULL && len > 0;                       \
         ustream_consume(stream, len), ptr = ustream_get_read_buf(stream, &len))

struct command {
    struct uwsc_client *ws;
    struct ustream_fd opipe;
    struct ustream_fd epipe;
    struct runqueue_process proc;
    struct blob_attr *arg;
    struct blob_attr *env;
    uint32_t id;
    int code;   /* The exit code of the child */
    char cmd[0];
};

static struct runqueue q;

void command_init()
{
    runqueue_init(&q);
    q.max_running_tasks = 5;
}

static void free_command(struct command *c)
{
    ustream_free(&c->opipe.stream);
    ustream_free(&c->epipe.stream);

    close(c->opipe.fd.fd);
    close(c->epipe.fd.fd);

    free(c->arg);
    free(c->env);
    free(c);
}

static void ustream_to_blobmsg(struct ustream *s, struct blob_buf *buf, const char *name)
{
    int len;
    char *rbuf, *wbuf;

    if ((len = ustream_pending_data(s, false)) > 0) {
        wbuf = blobmsg_alloc_string_buffer(buf, name, len + 1);

        if (!wbuf)
            return;

        ustream_for_each_read_buffer(s, rbuf, len) {
            memcpy(wbuf, rbuf, len);
            wbuf += len;
        }

        *wbuf = 0;
        blobmsg_add_string_buffer(buf);
    }
}

struct blob_buf *make_command_reply(uint32_t id, int err)
{
    static struct blob_buf buf;

    blob_buf_init(&buf, 0);

    blobmsg_add_u32(&buf, "id", id);
    blobmsg_add_u32(&buf, "err", err);

    return &buf;
}

void send_command_reply(struct blob_buf *buf, struct uwsc_client *ws)
{
    char *str = blobmsg_format_json(buf->head, true);

    ws->send(ws, str, strlen(str), WEBSOCKET_OP_TEXT);
    free(str);
    blob_buf_free(buf);
}

static void command_reply(struct command *c, int err)
{
    struct blob_buf *buf = make_command_reply(c->id, err);

    if (!err) {
        blobmsg_add_u32(buf, "code", c->code);
        ustream_to_blobmsg(&c->opipe.stream, buf, "stdout");
        ustream_to_blobmsg(&c->epipe.stream, buf, "stderr");
    }

    send_command_reply(buf, c->ws);

    free_command(c);
}

static void process_opipe_read_cb(struct ustream *s, int bytes)
{
    struct command *c = container_of(s, struct command, opipe.stream);

    if (ustream_read_buf_full(s))
        command_reply(c, COMMAND_ERR_READ);
}

static void process_epipe_read_cb(struct ustream *s, int bytes)
{
    struct command *c = container_of(s, struct command, epipe.stream);

    if (ustream_read_buf_full(s))
        command_reply(c, COMMAND_ERR_READ);
}

static void process_opipe_state_cb(struct ustream *s)
{
    struct command *c = container_of(s, struct command, opipe.stream);

    if (c->opipe.stream.eof && c->epipe.stream.eof)
        command_reply(c, 0);
}

static void process_epipe_state_cb(struct ustream *s)
{
    struct command *c = container_of(s, struct command, epipe.stream);

    if (c->opipe.stream.eof && c->epipe.stream.eof)
        command_reply(c, 0);
}

static const char *cmd_lookup(const char *cmd)
{
    struct stat s;
    int plen = 0, clen = strlen(cmd) + 1;
    char *search, *p;
    static char path[PATH_MAX];

    if (!stat(cmd, &s) && S_ISREG(s.st_mode))
        return cmd;

    search = getenv("PATH");

    if (!search)
        search = "/bin:/usr/bin:/sbin:/usr/sbin";

    p = search;

    do {
        if (*p != ':' && *p != '\0')
            continue;

        plen = p - search;

        if ((plen + clen) >= sizeof(path))
            continue;

        strncpy(path, search, plen);
        sprintf(path + plen, "/%s", cmd);

        if (!stat(path, &s) && S_ISREG(s.st_mode))
            return path;

        search = p + 1;
    } while (*p++);

    return NULL;
}

static void runqueue_proc_cb(struct uloop_process *p, int ret)
{
    struct runqueue_process *t = container_of(p, struct runqueue_process, proc);
    struct command *c = container_of(t, struct command, proc);

    if (c->code == -1) {
        /* The server will response timeout to user */
        ULOG_ERR("Run `%s` timeout\n", c->cmd);
        free_command(c);
        goto TIMEOUT;
    }

    c->code = WEXITSTATUS(ret);
    ustream_poll(&c->opipe.stream);
    ustream_poll(&c->epipe.stream);

TIMEOUT:
    runqueue_task_complete(&t->task);
}

static void q_cmd_run(struct runqueue *q, struct runqueue_task *t)
{
    struct command *c = container_of(t, struct command, proc.task);
    pid_t pid;
    int opipe[2];
    int epipe[2];
    char arglen;
    char **args;
    int rem;
    struct blob_attr *cur;

    if (pipe(opipe) || pipe(epipe)) {
        ULOG_ERR("pipe:%s\n", strerror(errno));
        runqueue_task_complete(t);
        send_command_reply(make_command_reply(c->id, COMMAND_ERR_SYS), c->ws);
        return;
    }

    switch ((pid = fork())) {
    case -1:
        ULOG_ERR("fork: %s\n", strerror(errno));
        runqueue_task_complete(t);
        send_command_reply(make_command_reply(c->id, COMMAND_ERR_SYS), c->ws);
        return;

    case 0:
        uloop_done();

        dup2(opipe[1], 1);
        dup2(epipe[1], 2);

        close(0);
        close(opipe[0]);
        close(epipe[0]);

        arglen = 2;
        args = malloc(sizeof(char *) * arglen);
        if (!args) {
            ULOG_ERR("malloc failed: No mem\n");
            return;
        }

        args[0] = (char *)c->cmd;
        args[1] = NULL;

        if (c->arg) {
            blobmsg_for_each_attr(cur, c->arg, rem) {
                if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
                    continue;

                arglen++;

                if (!(args = realloc(args, sizeof(char *) * arglen))) {
                    ULOG_ERR("realloc failed: No mem\n");
                    return;
                }

                args[arglen-2] = blobmsg_data(cur);
                args[arglen-1] = NULL;
            }
        }

        if (c->env) {
            blobmsg_for_each_attr(cur, c->env, rem) {
                if (blobmsg_type(cur) == BLOBMSG_TYPE_STRING)
                    setenv(blobmsg_name(cur), blobmsg_data(cur), 1);
            }
        }

        execv(c->cmd, args);
        break;

    default:
        close(opipe[1]);
        close(epipe[1]);

        ustream_declare(c->opipe, opipe[0], opipe);
        ustream_declare(c->epipe, epipe[0], epipe);

        runqueue_process_add(q, &c->proc, pid);
        c->proc.proc.cb = runqueue_proc_cb;
    }
}

static void run_command_cancel(struct runqueue *q, struct runqueue_task *t, int type)
{
    struct command *c = container_of(t, struct command, proc.task);

    c->code = -1;   /* Timeout */
    runqueue_process_cancel_cb(q, t, type);
}

void run_command(struct uwsc_client *ws, uint32_t id, const char *cmd,
    const struct blob_attr *arg, const struct blob_attr *env)
{
    static const struct runqueue_task_type cmd_type = {
        .run = q_cmd_run,
        .cancel = run_command_cancel,
        .kill = runqueue_process_kill_cb
    };
    struct command *c;

    cmd = cmd_lookup(cmd);
    if (!cmd) {
        ULOG_ERR("Not found cmd\n");
        send_command_reply(make_command_reply(id, COMMAND_ERR_NOTFOUND), ws);
        return;
    }

    c = calloc(1, sizeof(struct command) + strlen(cmd) + 1);
    c->proc.task.type = &cmd_type;
    c->proc.task.run_timeout = 5000;
    c->id = id;
    c->ws = ws;
    strcpy(c->cmd, cmd);

    if (arg) {
        c->arg = blob_memdup((struct blob_attr *)arg);
        if (!c->arg) {
            ULOG_ERR("blob_memdup arg failed: No mem\n");
            goto err;
        }
    }

    if (env) {
        c->env = blob_memdup((struct blob_attr *)env);
        if (!c->env) {
            ULOG_ERR("blob_memdup env failed: No mem\n");
            goto err;
        }
    }

    runqueue_task_add(&q, &c->proc.task, false);
    return;

err:
    free(c->arg);
    free(c->env);
    free(c);
    send_command_reply(make_command_reply(c->id, COMMAND_ERR_SYS), c->ws);
}
