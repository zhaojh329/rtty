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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <shadow.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <uwsc/log.h>

#include "list.h"
#include "command.h"

static int nrunning;
static LIST_HEAD(task_pending);

static void run_task(struct task *t);

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

static const char *cmderr2str(int err)
{
    switch (err) {
    case RTTY_CMD_ERR_PERMIT:
        return "operation not permitted";
    case RTTY_CMD_ERR_NOT_FOUND:
        return "not found";
    case RTTY_CMD_ERR_NOMEM:
        return "no mem";
    case RTTY_CMD_ERR_SYSERR:
        return "sys error";
    default:
        return "";
    }
}

static void task_free(struct task *t)
{
    /* stdout watcher */
    if (t->ioo.fd > 0) {
        close(t->ioo.fd);
        ev_io_stop(t->ws->loop, &t->ioo);
    }

    /* stderr watcher */
    if (t->ioe.fd > 0) {
        close(t->ioe.fd);
        ev_io_stop(t->ws->loop, &t->ioe);
    }

    ev_child_stop(t->ws->loop, &t->cw);
    ev_timer_stop(t->ws->loop, &t->timer);

    buffer_free(&t->ob);
    buffer_free(&t->eb);

    json_value_free((json_value *)t->msg);

    free(t);
}

static void cmd_err_reply(struct uwsc_client *ws, int id, int err)
{
    char str[128] = "";

    snprintf(str, sizeof(str) - 1, "{\"type\":\"cmd\",\"id\":%d,"
            "\"attrs\":{\"err\":%d,\"msg\":\"%s\"}}", id, err, cmderr2str(err));
    ws->send(ws, str, strlen(str), UWSC_OP_TEXT);
}

static void cmd_reply(struct task *t, int code)
{
    char str[4096];

    snprintf(str, sizeof(str) - 1, "{\"type\":\"cmd\",\"id\":%d,"
            "\"attrs\":{\"stdout\":\"%.*s\",\"stderr\":\"%.*s\",\"code\":%d}}", t->id,
            (int)buffer_length(&t->ob), (char *)buffer_data(&t->ob),
            (int)buffer_length(&t->eb), (char *)buffer_data(&t->eb),
            code);
    t->ws->send(t->ws, str, strlen(str), UWSC_OP_TEXT);
}

static void ev_child_exit(struct ev_loop *loop, struct ev_child *w, int revents)
{
    struct task *t = container_of(w, struct task, cw);

    cmd_reply(t, WEXITSTATUS(w->rstatus));
    task_free(t);

    nrunning--;

    if (list_empty(&task_pending))
        return;

    t = list_first_entry(&task_pending, struct task, list);
    if (t) {
        list_del(&t->list);
        run_task(t);
    }
}

static void ev_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    struct task *t = container_of(w, struct task, timer);

    task_free(t);
    nrunning--;

    uwsc_log_err("exec '%s' timeout\n", t->cmd);
}

static void ev_io_stdout_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct task *t = container_of(w, struct task, ioo);
    bool eof;

    buffer_put_fd(&t->ob, w->fd, -1, &eof, NULL, NULL);
}

static void ev_io_stderr_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct task *t = container_of(w, struct task, ioe);
    bool eof;

    buffer_put_fd(&t->eb, w->fd, -1, &eof, NULL, NULL);
}

static void run_task(struct task *t)
{
    int opipe[2];
    int epipe[2];
    pid_t pid;
    int err;

    if (pipe2(opipe, O_CLOEXEC | O_NONBLOCK) < 0 ||
        pipe2(epipe, O_CLOEXEC | O_NONBLOCK) < 0) {
        uwsc_log_err("pipe2 failed: %s\n", strerror(errno));
        err = RTTY_CMD_ERR_SYSERR;
        goto ERR;
    }

    pid = fork();
    switch (pid) {
    case -1:
        uwsc_log_err("fork: %s\n", strerror(errno));
        err = RTTY_CMD_ERR_SYSERR;
        goto ERR;

    case 0: {
        const json_value *params = json_get_value(t->attrs, "params");
        const json_value *env = json_get_value(t->attrs, "env");
        int i, arglen;
        char **args;

        /* Close unused read end */
        close(opipe[0]);
        close(epipe[0]);

        /* Redirect */
        dup2(opipe[1], STDOUT_FILENO);
        dup2(epipe[1], STDERR_FILENO);
        close(opipe[1]);
        close(epipe[1]);

        arglen = 2 + params->u.array.length;
        args = calloc(1, sizeof(char *) * arglen);
        if (!args)
            exit(1);

        args[0] = t->cmd;
        for (i = 0; i < params->u.array.length; i++)
            args[i + 1] = (char *)json_get_array_string(params, i);

        if (env->type == json_object) {
            for (i = 0; i < env->u.object.length; i++) {
                json_value *v = env->u.object.values[i].value;
                if (v->type == json_string)
                    setenv(env->u.object.values[i].name, v->u.string.ptr, 1);
            }
        }

        execv(t->cmd, args);
    }
    default:
        /* Close unused write end */
        close(opipe[1]);
        close(epipe[1]);

        /* Watch child's status */
        ev_child_init(&t->cw, ev_child_exit, pid, 0);
        ev_child_start(t->ws->loop, &t->cw);

        ev_io_init(&t->ioo, ev_io_stdout_cb, opipe[0], EV_READ);
        ev_io_start(t->ws->loop, &t->ioo);

        ev_io_init(&t->ioe, ev_io_stderr_cb, epipe[0], EV_READ);
        ev_io_start(t->ws->loop, &t->ioe);

        ev_timer_init(&t->timer, ev_timer_cb, RTTY_CMD_EXEC_TIMEOUT, 0);
        ev_timer_start(t->ws->loop, &t->timer);

        nrunning++;
        return;
    }

ERR:
    cmd_err_reply(t->ws, t->id, err);
    task_free(t);
}

static void add_task(struct uwsc_client *ws, int id, const char *cmd,
    const json_value *msg, const json_value *attrs)
{
    struct task *t;

    t = calloc(1, sizeof(struct task) + strlen(cmd) + 1);
    if (!t) {
        cmd_err_reply(ws, id, RTTY_CMD_ERR_NOMEM);
        return;
    }


    t->id = id;
    t->ws = ws;
    t->msg = msg;
    t->attrs = attrs;
    strcpy(t->cmd, cmd);

    if (nrunning < RTTY_CMD_MAX_RUNNING)
        run_task(t);
    else
        list_add_tail(&t->list, &task_pending);
}

void run_command(struct uwsc_client *ws, const json_value *msg)
{
    const json_value *attrs = json_get_value(msg, "attrs");
    const char *username = json_get_string(attrs, "username");
    const char *password = json_get_string(attrs, "password");
    int id = json_get_int(msg, "id");
    const char *cmd;
    int err = 0;

    if (!username || !username[0] || !login_test(username, password)) {
        err = RTTY_CMD_ERR_PERMIT;
        goto ERR;
    }

    cmd = cmd_lookup(json_get_string(attrs, "cmd"));
    if (!cmd) {
        err = RTTY_CMD_ERR_NOT_FOUND;
        goto ERR;
    }

    add_task(ws, id, cmd, msg, attrs);
    return;

ERR:
    cmd_err_reply(ws, id, err);
    json_value_free((json_value *)msg);
}
