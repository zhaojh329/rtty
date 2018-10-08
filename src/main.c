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

#include <pty.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <uwsc/uwsc.h>

#include "list.h"
#include "json.h"
#include "config.h"
#include "utils.h"
#include "command.h"

#define RTTY_RECONNECT_INTERVAL  5
#define RTTY_MAX_SESSIONS        5
#define RTTY_BUFFER_PERSISTENT_SIZE 4096

struct tty_session {
    pid_t pid;
    int pty;
    int sid;

    struct ev_loop *loop;
    struct ev_timer timer;
    struct uwsc_client *cl;
    struct ev_io ior;
    struct ev_io iow;
    struct ev_child cw;
    struct buffer wb;
};

static char login[128];       /* /bin/login */
static char server_url[512];
static bool auto_reconnect;
static int keepalive = 5;       /* second */
static struct ev_timer reconnect_timer;
static struct tty_session *sessions[RTTY_MAX_SESSIONS + 1];

static void del_tty_session(struct tty_session *tty)
{
    ev_io_stop(tty->loop, &tty->ior);
    ev_io_stop(tty->loop, &tty->iow);
    ev_timer_stop(tty->loop, &tty->timer);
    ev_child_stop(tty->loop, &tty->cw);

    buffer_free(&tty->wb);

    close(tty->pty);
    kill(tty->pid, SIGTERM);

    sessions[tty->sid] = NULL;

    uwsc_log_info("Del session: %d\n", tty->sid);

    free(tty);
}

static inline struct tty_session *find_tty_session(int sid)
{
    if (sid > RTTY_MAX_SESSIONS)
        return NULL;

    return sessions[sid];
}

static inline void del_tty_session_by_sid(int sid)
{
    struct tty_session *tty = find_tty_session(sid);
    if (tty)
        del_tty_session(tty);
}

static void pty_read_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct tty_session *tty = container_of(w, struct tty_session, ior);
    struct uwsc_client *cl = tty->cl;
    static uint8_t buf[4096 + 1];
    int len;

    buf[0] = tty->sid;

    while (1) {
        len = read(w->fd, buf + 1, sizeof(buf) - 1);
        if (likely(len > 0))
            break;

        if (len < 0) {
            if (errno == EINTR)
                continue;
            if (errno != EIO)
                uwsc_log_err("Read from pty failed: %s\n", strerror(errno));
            return;
        }

        if (len == 0)
            return;
    }

    cl->send(cl, buf, len + 1, UWSC_OP_BINARY);
}

static void pty_write_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct tty_session *tty = container_of(w, struct tty_session, iow);
    struct buffer *wb = &tty->wb;
    int ret;

    ret = buffer_pull_to_fd(wb, w->fd, buffer_length(wb), NULL, NULL);
    if (ret < 0) {
        uwsc_log_err("Write to pty failed: %s\n", strerror(errno));
        return;
    }

    if (buffer_length(wb) < 1)
        ev_io_stop(loop, w);
}

static void pty_on_exit(struct ev_loop *loop, struct ev_child *w, int revents)
{
    struct tty_session *tty = container_of(w, struct tty_session, cw);
    char str[128] = "";

    snprintf(str, sizeof(str) - 1, "{\"type\":\"logout\",\"sid\":%d}", tty->sid);

    tty->cl->send(tty->cl, str, strlen(str), UWSC_OP_TEXT);

    del_tty_session(tty);
}

static void new_tty_session(struct uwsc_client *cl, int sid)
{
    struct tty_session *s;
    char str[128] = "";
    pid_t pid;
    int pty;

    s = calloc(1, sizeof(struct tty_session));
    if (!s)
        return;

    pid = forkpty(&pty, NULL, NULL, NULL);
    if (pid == 0)
        execl(login, login, NULL);

    s->cl = cl;
    s->sid = sid;
    s->pid = pid;
    s->pty = pty;
    s->loop = cl->loop;

    fcntl(pty, F_SETFL, fcntl(pty, F_GETFL, 0) | O_NONBLOCK);

    ev_io_init(&s->ior, pty_read_cb, pty, EV_READ);
    ev_io_start(cl->loop, &s->ior);

    ev_io_init(&s->iow, pty_write_cb, pty, EV_WRITE);

    ev_child_init(&s->cw, pty_on_exit, pid, 0);
    ev_child_start(cl->loop, &s->cw);

    buffer_set_persistent_size(&s->wb, RTTY_BUFFER_PERSISTENT_SIZE);

    sessions[sid] = s;

    /* Notifying the user that the session was successfully created */
    snprintf(str, sizeof(str) - 1, "{\"type\":\"login\",\"sid\":%d,\"code\":0}", sid);
    cl->send(cl, str, strlen(str), UWSC_OP_TEXT);

    uwsc_log_info("New session:%llu\n", sid);
}

static void change_winsize(int sid, int cols, int rows)
{
    struct tty_session *tty = find_tty_session(sid);
    struct winsize size = {
        .ws_col = cols,
        .ws_row = rows
    };

    if(ioctl(tty->pty, TIOCSWINSZ, &size) < 0)
        uwsc_log_err("ioctl TIOCSWINSZ error\n");
}

static void uwsc_onmessage(struct uwsc_client *cl, void *data, size_t len, bool binary)
{
    if (binary) {
        int sid = (*(uint8_t *)data);
        struct tty_session *tty = find_tty_session(sid);

        if (!tty) {
            uwsc_log_err("non-existent sid: %d\n", sid);
            return;
        }

        buffer_put_data(&tty->wb, data + 1, len - 1);
        ev_io_start(tty->loop, &tty->iow);
        return;
    } else {
        const json_value *json;
        const char *type;
        int sid;
       
        json = json_parse((char *)data, len);
        if (!json) {
            uwsc_log_err("Invalid format: [%.*s]\n", len, (char *)data);
            return;
        }

        type = json_get_string(json, "type");
        if (!type || !type[0]) {
            uwsc_log_err("Invalid format, not found type\n");
            goto done;
        }

        sid = json_get_int(json, "sid");

        if (!strcmp(type, "register")) {
            uwsc_log_err("register failed: %s\n", json_get_string(json, "msg"));
            ev_break(cl->loop, EVBREAK_ALL);
        } else if (!strcmp(type, "login")) {
            if (sid > RTTY_MAX_SESSIONS) {
                char str[128] = "";
                /* Notifies the user that the session creation failed  */
                snprintf(str, sizeof(str) - 1, "{\"type\":\"login\",\"sid\":%d,\"err\":2,\"msg\":\"sessions is full\"}", sid);
                cl->send(cl, str, strlen(str), UWSC_OP_TEXT);
                uwsc_log_err("Can only run up to 5 sessions at the same time\n");
                goto done;
            }
            new_tty_session(cl, sid);
        } if (!strcmp(type, "logout")) {
            del_tty_session_by_sid(sid);
        } if (!strcmp(type, "cmd")) {
            run_command(cl, json);
            return;
        } if (!strcmp(type, "winsize")) {
            int cols = json_get_int(json, "cols");
            int rows = json_get_int(json, "rows");
            change_winsize(sid, cols, rows);
        }

done:
        json_value_free((json_value *)json);
    }
}

static void uwsc_onopen(struct uwsc_client *cl)
{
    uwsc_log_info("Connect to server succeed\n");
}

static void uwsc_onerror(struct uwsc_client *cl, int err, const char *msg)
{
    struct ev_loop *loop = cl->loop;

    uwsc_log_err("onerror:%d: %s\n", err, msg);

    free(cl);

	if (auto_reconnect)
        ev_timer_again(loop, &reconnect_timer);
    else
        ev_break(loop, EVBREAK_ALL);
}

static void uwsc_onclose(struct uwsc_client *cl, int code, const char *reason)
{
    struct ev_loop *loop = cl->loop;
    int i;

    uwsc_log_err("onclose:%d: %s\n", code, reason);

    for (i = 0; i < RTTY_MAX_SESSIONS + 1; i++)
        if (sessions[i])
            del_tty_session(sessions[i]);

    free(cl);

    if (auto_reconnect)
        ev_timer_again(loop, &reconnect_timer);
    else
        ev_break(loop, EVBREAK_ALL);
}

static void do_connect(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    struct uwsc_client *cl = uwsc_new(loop, server_url, keepalive);
    if (cl) {
        cl->onopen = uwsc_onopen;
        cl->onmessage = uwsc_onmessage;
        cl->onerror = uwsc_onerror;
        cl->onclose = uwsc_onclose;
        ev_timer_stop(cl->loop, &reconnect_timer);
        return;
    }

    if (!auto_reconnect) {
   	    ev_break(cl->loop, EVBREAK_ALL);
        free(cl);
    }
}

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
    if (w->signum == SIGINT) {
        ev_break(loop, EVBREAK_ALL);
        uwsc_log_info("Normal quit\n");
    }
}

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [option]\n"
        "      -i ifname    # Network interface name - Using the MAC address of\n"
        "                          the interface as the device ID\n"
        "      -I id        # Set an ID for the device(Maximum 63 bytes, valid character:letters\n"
        "                          and numbers and underlines and short lines) - If set,\n"
        "                          it will cover the MAC address(if you have specify the ifname)\n"
        "      -h host      # Server host\n"
        "      -p port      # Server port\n"
        "      -a           # Auto reconnect to the server\n"
        "      -v           # verbose\n"
        "      -d           # Adding a description to the device(Maximum 126 bytes)\n"
        "      -s           # SSL on\n"
        "      -k keepalive # keep alive in seconds for this client. Defaults to 5\n"
        "      -V           # Show version\n"
        "      -D           # Run in the background\n"
        , prog);
    exit(1);
}

int main(int argc, char **argv)
{
    int opt;
    struct ev_loop *loop = EV_DEFAULT;
    struct ev_signal signal_watcher;
    char mac[13] = "";
    char devid[64] = "";
    const char *host = NULL;
    int port = 0;
    char *description = NULL;
    bool background = false;
    bool verbose = false;
    bool ssl = false;

    while ((opt = getopt(argc, argv, "i:h:p:I:avd:sk:VD")) != -1) {
        switch (opt)
        {
        case 'i':
            if (get_iface_mac(optarg, mac, sizeof(mac)) < 0) {
                return -1;
            }
            break;
        case 'h':
            host = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'I':
            strncpy(devid, optarg, sizeof(devid) - 1);
            break;
        case 'a':
            auto_reconnect = true;
            break;
        case 'v':
            verbose = true;
            break;
        case 'd':
            if (strlen(optarg) > 126) {
                uwsc_log_err("Description too long\n");
                usage(argv[0]);
            }
            description = calloc(1, strlen(optarg) * 4);
            if (!description) {
                uwsc_log_err("malloc failed:%s\n", strerror(errno));
                exit(1);
            }
            urlencode(description, strlen(optarg) * 4, optarg, strlen(optarg));
            break;
        case 's':
            ssl = true;
            break;
        case 'k':
            keepalive = atoi(optarg);
            break;
        case 'V':
            uwsc_log_info("rtty version %s\n", RTTY_VERSION_STRING);
            exit(0);
            break;
        case 'D':
            background = true;
            break;
        default: /* '?' */
            usage(argv[0]);
        }
    }

    if (background && daemon(0, 0))
        uwsc_log_err("Can't run in the background: %s\n", strerror(errno));

    if (!verbose)
        uwsc_log_threshold(LOG_ERR);

    if (!devid[0]) {
        if (!mac[0]) {
            uwsc_log_err("You must specify the ifname or id\n");
            usage(argv[0]);
        }
        strcpy(devid, mac);
    }

    if (!valid_id(devid)) {
        uwsc_log_err("Invalid device id\n");
        usage(argv[0]);
    }

    if (!host || !port) {
        uwsc_log_err("You must specify the host and port\n");
        usage(argv[0]);
    }

    uwsc_log_info("libuwsc version %s\n", UWSC_VERSION_STRING);
    uwsc_log_info("rtty version %s\n", RTTY_VERSION_STRING);

    if (getuid() > 0) {
        uwsc_log_err("Operation not permitted\n");
        return -1;
    }

    if (find_login(login, sizeof(login) - 1) < 0) {
        uwsc_log_err("The program 'login' is not found\n");
        return -1;
    }

    snprintf(server_url, sizeof(server_url),
        "ws%s://%s:%d/ws?device=1&devid=%s&description=%s&keepalive=%d",
        ssl ? "s" : "", host, port, devid, description ? description : "", keepalive);
    free(description);

    ev_timer_init(&reconnect_timer, do_connect, 0.0, RTTY_RECONNECT_INTERVAL);
	ev_timer_start(loop, &reconnect_timer);

    ev_signal_init(&signal_watcher, signal_cb, SIGINT);
    ev_signal_start(loop, &signal_watcher);

    ev_run(loop, 0);
    
    return 0;
}
