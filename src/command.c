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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <shadow.h>
#include <sys/stat.h>
#include <math.h>
#include <pwd.h>

#include "log/log.h"
#include "list.h"
#include "utils.h"
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
    if (!sp || !sp->sp_pwdp[1])
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
    case RTTY_CMD_ERR_RESP_TOOBIG:
        return "stdout+stderr is too big";
    default:
        return "";
    }
}

static void task_free(struct task *t)
{
    int i;

    /* stdout watcher */
    if (t->ioo.fd > 0) {
        ev_io_stop(t->rtty->loop, &t->ioo);
        close(t->ioo.fd);
    }

    /* stderr watcher */
    if (t->ioe.fd > 0) {
        ev_io_stop(t->rtty->loop, &t->ioe);
        close(t->ioe.fd);
    }

    ev_child_stop(t->rtty->loop, &t->cw);
    ev_timer_stop(t->rtty->loop, &t->timer);

    buffer_free(&t->ob);
    buffer_free(&t->eb);

    for (i = 0; i < t->nparams; i++)
        free(t->params[i + 1]);
    free(t->params);

    free(t);
}

static void cmd_err_reply(struct rtty *rtty, const char *token, int err)
{
    char str[256] = "";

    snprintf(str, sizeof(str) - 1, "{\"token\":\"%s\","
             "\"attrs\":{\"err\":%d,\"msg\":\"%s\"}}", token, err, cmderr2str(err));

    rtty_send_msg(rtty, MSG_TYPE_CMD, str, strlen(str));
}

static void cmd_reply(struct task *t, int code)
{
    size_t len = buffer_length(&t->ob) + buffer_length(&t->eb);
    struct rtty *rtty = t->rtty;
    char *str, *pos;
    int ret;

    len = ceil(len * 4.0 / 3) + 200;

    str = calloc(1, len);
    if (!str) {
        cmd_err_reply(t->rtty, t->token, RTTY_CMD_ERR_NOMEM);
        return;
    }

    pos = str;

    ret = snprintf(pos, len, "{\"token\":\"%s\","
                   "\"attrs\":{\"code\":%d,\"stdout\":\"", t->token, code);

    len -= ret;
    pos += ret;

    ret = b64_encode(buffer_data(&t->ob), buffer_length(&t->ob), pos, len);
    len -= ret;
    pos += ret;

    ret = snprintf(pos, len, "\",\"stderr\":\"");
    len -= ret;
    pos += ret;

    ret = b64_encode(buffer_data(&t->eb), buffer_length(&t->eb), pos, len);
    len -= ret;
    pos += ret;

    ret = snprintf(pos, len, "\"}}");
    pos += ret;

    rtty_send_msg(rtty, MSG_TYPE_CMD, str, pos - str);

    free(str);
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

    log_err("exec '%s' timeout\n", t->cmd);
}

static void ev_io_stdout_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct task *t = container_of(w, struct task, ioo);
    bool eof;

    buffer_put_fd(&t->ob, w->fd, -1, &eof);
}

static void ev_io_stderr_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct task *t = container_of(w, struct task, ioe);
    bool eof;

    buffer_put_fd(&t->eb, w->fd, -1, &eof);
}

static void run_task(struct task *t)
{
    int opipe[2];
    int epipe[2];
    pid_t pid;
    int err;

    if (pipe2(opipe, O_CLOEXEC | O_NONBLOCK) < 0 ||
            pipe2(epipe, O_CLOEXEC | O_NONBLOCK) < 0) {
        log_err("pipe2 failed: %s\n", strerror(errno));
        err = RTTY_CMD_ERR_SYSERR;
        goto ERR;
    }

    pid = fork();
    if (pid < 0) {
        log_err("fork: %s\n", strerror(errno));
        err = RTTY_CMD_ERR_SYSERR;
        goto ERR;
    } else if (pid == 0) {
        /* Close unused read end */
        close(opipe[0]);
        close(epipe[0]);

        /* Redirect */
        dup2(opipe[1], STDOUT_FILENO);
        dup2(epipe[1], STDERR_FILENO);
        close(opipe[1]);
        close(epipe[1]);

        if (setuid(t->uid) < 0)
            exit(1);

        execv(t->cmd, t->params);
    } else {
        /* Close unused write end */
        close(opipe[1]);
        close(epipe[1]);

        /* Watch child's status */
        ev_child_init(&t->cw, ev_child_exit, pid, 0);
        ev_child_start(t->rtty->loop, &t->cw);

        ev_io_init(&t->ioo, ev_io_stdout_cb, opipe[0], EV_READ);
        ev_io_start(t->rtty->loop, &t->ioo);

        ev_io_init(&t->ioe, ev_io_stderr_cb, epipe[0], EV_READ);
        ev_io_start(t->rtty->loop, &t->ioe);

        ev_timer_init(&t->timer, ev_timer_cb, RTTY_CMD_EXEC_TIMEOUT, 0);
        ev_timer_start(t->rtty->loop, &t->timer);

        nrunning++;
        return;
    }

ERR:
    cmd_err_reply(t->rtty, t->token, err);
    task_free(t);
}

static void add_task(struct rtty *rtty, const char *token, uid_t uid, const char *cmd, const char *data)
{
    struct task *t;
    int i;

    t = calloc(1, sizeof(struct task) + strlen(cmd) + 1);
    if (!t) {
        cmd_err_reply(rtty, token, RTTY_CMD_ERR_NOMEM);
        return;
    }

    t->rtty = rtty;
    t->uid = uid;

    strcpy(t->cmd, cmd);
    strcpy(t->token, token);

    t->nparams = *data++;
    t->params = calloc(t->nparams + 2, sizeof(char *));

    t->params[0] = t->cmd;

    for (i = 0; i < t->nparams; i++) {
        t->params[i + 1] = strdup(data);
        data += strlen(t->params[i + 1]) + 1;
    }

    if (nrunning < RTTY_CMD_MAX_RUNNING)
        run_task(t);
    else
        list_add_tail(&t->list, &task_pending);
}

void run_command(struct rtty *rtty, const char *data)
{
    const char *username = data;
    const char *password = username + strlen(username) + 1;
    const char *cmd = password + strlen(password) + 1;
    const char *token = cmd + strlen(cmd) + 1;
    struct passwd *pw;
    int err = 0;

    data = token + strlen(token) + 1;

    if (!username[0] || !login_test(username, password)) {
        err = RTTY_CMD_ERR_PERMIT;
        goto ERR;
    }

    pw = getpwnam(username);
    if (!pw) {
        err = RTTY_CMD_ERR_PERMIT;
        goto ERR;
    }

    cmd = cmd_lookup(cmd);
    if (!cmd) {
        err = RTTY_CMD_ERR_NOT_FOUND;
        goto ERR;
    }

    add_task(rtty, token, pw->pw_uid, cmd, data);
    return;

ERR:
    cmd_err_reply(rtty, token, err);
}
