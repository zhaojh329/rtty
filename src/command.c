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
#include <shadow.h>
#include <sys/stat.h>
#include <uwsc/log.h>
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
    RttyMessage *msg;
    int code;   /* The exit code of the child */
    char cmd[0];
};

static struct runqueue q;

void command_init()
{
    runqueue_init(&q);
    q.max_running_tasks = 5;
}

/* For execute command */
static bool login_test(const char *username, const char *password)
{
    struct spwd *sp;

    if (!username || *username == 0)
        return false;

    sp = getspnam(username);
    if (!sp)
        return false;

    if (!password)
        password = "";

    return !strcmp(crypt(password, sp->sp_pwdp), sp->sp_pwdp);
}

static void command_reply_error(struct uwsc_client *ws, int id, int err)
{
    RttyMessage *msg = rtty_message_init(RTTY_MESSAGE__TYPE__COMMAND, NULL);

    rtty_message_set_id(msg, id);
    rtty_message_set_code(msg, 1);
    rtty_message_set_err(msg, err);
    rtty_message_send(ws, msg);
}

static void free_command(struct command *c)
{
    ustream_free(&c->opipe.stream);
    ustream_free(&c->epipe.stream);

    close(c->opipe.fd.fd);
    close(c->epipe.fd.fd);

    rtty_message__free_unpacked(c->msg, NULL);
    free(c);
}

static void ustream_to_string(struct ustream *s, char **str)
{
    int len;
    char *buf, *p;

    if ((len = ustream_pending_data(s, false)) > 0) {
        *str = calloc(1, len + 1);
        if (!*str) {
            uwsc_log_err("calloc failed\n");
            return;
        }

        p = *str;

        ustream_for_each_read_buffer(s, buf, len) {
            memcpy(p, buf, len);
            p += len;
        }
    }
}

static void command_reply(struct command *c, int err)
{
    RttyMessage *msg = rtty_message_init(RTTY_MESSAGE__TYPE__COMMAND, NULL);
    char *std_out = NULL, *std_err = NULL;

    rtty_message_set_id(msg, c->msg->id);
    rtty_message_set_err(msg, err);

    if (err == RTTY_MESSAGE__COMMAND_ERR__NONE) {
        rtty_message_set_code(msg, c->code);

        ustream_to_string(&c->opipe.stream, &std_out);
        ustream_to_string(&c->epipe.stream, &std_err);

        msg->std_out = std_out;
        msg->std_err = std_err;
    }

    rtty_message_send(c->ws, msg);

    free(std_out);
    free(std_err);
    free_command(c);
}

static void process_opipe_read_cb(struct ustream *s, int bytes)
{
    struct command *c = container_of(s, struct command, opipe.stream);

    if (ustream_read_buf_full(s))
        command_reply(c, RTTY_MESSAGE__COMMAND_ERR__READ);
}

static void process_epipe_read_cb(struct ustream *s, int bytes)
{
    struct command *c = container_of(s, struct command, epipe.stream);

    if (ustream_read_buf_full(s))
        command_reply(c, RTTY_MESSAGE__COMMAND_ERR__READ);
}

static void process_opipe_state_cb(struct ustream *s)
{
    struct command *c = container_of(s, struct command, opipe.stream);

    if (c->opipe.stream.eof && c->epipe.stream.eof)
        command_reply(c, RTTY_MESSAGE__COMMAND_ERR__NONE);
}

static void process_epipe_state_cb(struct ustream *s)
{
    struct command *c = container_of(s, struct command, epipe.stream);

    if (c->opipe.stream.eof && c->epipe.stream.eof)
        command_reply(c, RTTY_MESSAGE__COMMAND_ERR__NONE);
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
        uwsc_log_err("Run `%s` timeout\n", c->cmd);
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
    RttyMessage *msg = c->msg;
    pid_t pid;
    int opipe[2];
    int epipe[2];
    char arglen;
    char **args;
    int i;

    if (pipe(opipe) || pipe(epipe)) {
        uwsc_log_err("pipe:%s\n", strerror(errno));
        runqueue_task_complete(t);
        command_reply_error(c->ws, msg->id, RTTY_MESSAGE__COMMAND_ERR__SYSCALL);
        return;
    }

    switch ((pid = fork())) {
    case -1:
        uwsc_log_err("fork: %s\n", strerror(errno));
        runqueue_task_complete(t);
        command_reply_error(c->ws, msg->id, RTTY_MESSAGE__COMMAND_ERR__SYSCALL);
        return;

    case 0:
        uloop_done();

        dup2(opipe[1], 1);
        dup2(epipe[1], 2);

        close(0);
        close(opipe[0]);
        close(epipe[0]);

        arglen = 2 + msg->n_params;
        args = calloc(1, sizeof(char *) * arglen);
        if (!args)
            return;

        args[0] = (char *)c->cmd;

        for (i = 0; i < msg->n_params; i++)
            args[i + 1] = msg->params[i];

        for (i = 0; i < msg->n_env; i++)
            setenv(msg->env[i]->key, msg->env[i]->value, 1);

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

void run_command(struct uwsc_client *ws, RttyMessage *msg, void *data, uint32_t len)
{
    static const struct runqueue_task_type cmd_type = {
        .run = q_cmd_run,
        .cancel = run_command_cancel,
        .kill = runqueue_process_kill_cb
    };
    struct command *c;
    const char *cmd;

    if (!msg->username) {
        command_reply_error(ws, msg->id, RTTY_MESSAGE__COMMAND_ERR__PERMISSION);
        return;
    }

    if (!login_test(msg->username, msg->password)) {
        command_reply_error(ws, msg->id, RTTY_MESSAGE__COMMAND_ERR__PERMISSION);
        return;
    }

    cmd = cmd_lookup(msg->name);
    if (!cmd) {
        command_reply_error(ws, msg->id, RTTY_MESSAGE__COMMAND_ERR__NOTFOUND);
        return;
    }

    c = calloc(1, sizeof(struct command) + strlen(cmd) + 1);
    c->proc.task.type = &cmd_type;
    c->proc.task.run_timeout = 5000;
    c->ws = ws;
    strcpy(c->cmd, cmd);

    c->msg = rtty_message__unpack(NULL, len, data);

    runqueue_task_add(&q, &c->proc.task, false);
}
