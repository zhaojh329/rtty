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

#include "avl.h"
#include "command.h"

#define MAX_RUNNING   5

struct command {
	struct avl_node avl;
    struct uwsc_client *ws;
	struct ev_child cw;
	struct ev_io ioo;	/* Watch stdout of child */
	struct ev_io ioe;	/* Watch stderr of child */
	struct buffer ob;	/* buffer for stdout */
	struct buffer eb;	/* buffer for stderr */
    int code;   /* The exit code of the child */
	RttyMessage *msg;
    char cmd[0];
};

static int nrunning;
static struct avl_tree cmd_tree;

static int avl_strcmp(const void *k1, const void *k2, void *ptr)
{
	const RttyMessage *msg1 = k1;
	const RttyMessage *msg2 = k2;

	return msg1->id - msg2->id;
}

void command_init()
{
	avl_init(&cmd_tree, avl_strcmp, false, NULL);
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

static void command_free(struct command *c)
{
    close(c->ioo.fd);
    close(c->ioe.fd);

	ev_io_stop(c->ws->loop, &c->ioo);
	ev_io_stop(c->ws->loop, &c->ioe);
	ev_child_stop(c->ws->loop, &c->cw);

	buffer_free(&c->ob);
	buffer_free(&c->eb);

    rtty_message__free_unpacked(c->msg, NULL);

    free(c);
}

static void command_reply_error(struct uwsc_client *ws, int id, int err)
{
    RttyMessage *msg = rtty_message_init(RTTY_MESSAGE__TYPE__COMMAND, NULL);

    rtty_message_set_id(msg, id);
    rtty_message_set_code(msg, 1);
    rtty_message_set_err(msg, err);
    rtty_message_send(ws, msg);
}

static void command_reply(struct command *c, int err)
{
	RttyMessage *msg = rtty_message_init(RTTY_MESSAGE__TYPE__COMMAND, NULL);

	rtty_message_set_id(msg, c->msg->id);
	rtty_message_set_err(msg, err);

	if (err == RTTY_MESSAGE__COMMAND_ERR__NONE) {
		rtty_message_set_code(msg, c->code);

		buffer_put_zero(&c->ob, 1);
		buffer_put_zero(&c->eb, 1);

		msg->std_out = buffer_data(&c->ob);
		msg->std_err = buffer_data(&c->eb);
	}

	rtty_message_send(c->ws, msg);

	command_free(c);
}

static void do_run_command(struct command *c);

static void ev_command_exit(struct ev_loop *loop, struct ev_child *w, int revents)
{
	struct command *c = container_of(w, struct command, cw);

	c->code = WEXITSTATUS(w->rstatus);

	command_reply(c, 0);

	nrunning--;

	if (!avl_is_empty(&cmd_tree) && avl_first_element(&cmd_tree, c, avl)) {
		avl_delete(&cmd_tree, &c->avl);
		do_run_command(c);
	}
}

static void ev_io_stdout_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
	struct command *c = container_of(w, struct command, ioo);
	bool eof;

	buffer_put_fd(&c->ob, w->fd, -1, &eof, NULL, NULL);
}

static void ev_io_stderr_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
	struct command *c = container_of(w, struct command, ioe);
	bool eof;

	buffer_put_fd(&c->eb, w->fd, -1, &eof, NULL, NULL);
}

static void do_run_command(struct command *c)
{
	RttyMessage *msg = c->msg;
	int opipe[2];
    int epipe[2];
	pid_t pid;
	char arglen;
    char **args;
	int i;

	if (pipe2(opipe, O_CLOEXEC | O_NONBLOCK) < 0 ||
		pipe2(epipe, O_CLOEXEC | O_NONBLOCK) < 0) {
		uwsc_log_err("pipe2 failed: %s\n", strerror(errno));
		command_reply_error(c->ws, msg->id, RTTY_MESSAGE__COMMAND_ERR__SYSCALL);
		return;
	}

	pid = fork();
	switch (pid) {
	case -1:
		uwsc_log_err("fork: %s\n", strerror(errno));
		break;
	case 0:
		/* Close unused read end */
		close(opipe[0]);
		close(epipe[0]);

		dup2(opipe[1], 1);
		dup2(epipe[1], 2);

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
		/* Close unused write end */
		close(opipe[1]);
		close(epipe[1]);

		/* Watch child's status */
		ev_child_init(&c->cw, ev_command_exit, pid, 0);
		ev_child_start(c->ws->loop, &c->cw);

		ev_io_init(&c->ioo, ev_io_stdout_cb, opipe[0], EV_READ);
		ev_io_start(c->ws->loop, &c->ioo);

		ev_io_init(&c->ioe, ev_io_stderr_cb, epipe[0], EV_READ);
		ev_io_start(c->ws->loop, &c->ioe);

		nrunning++;
		break;
	}
}

static void add_command(struct command *c)
{
	if (nrunning < MAX_RUNNING) {
		do_run_command(c);
	} else {
		c->avl.key = c->msg;
		avl_insert(&cmd_tree, &c->avl);
	}
}

void run_command(struct uwsc_client *ws, RttyMessage *msg, void *data, uint32_t len)
{
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
    c->ws = ws;
    strcpy(c->cmd, cmd);

    c->msg = rtty_message__unpack(NULL, len, data);

	add_command(c);
}
