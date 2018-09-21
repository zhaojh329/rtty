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
#include <sys/wait.h>
#include <sys/stat.h>
#include <uwsc/uwsc.h>

#include "avl.h"
#include "config.h"
#include "utils.h"
#include "message.h"
#include "command.h"

#define RTTY_PROTO_VERSION  1
#define RECONNECT_INTERVAL  5

struct upfile_info {
    int fd;
    int size;
    int uploaded;
};

struct tty_session {
    pid_t pid;
    int pty;
    char sid[33];

    struct ev_loop *loop;
    struct ev_timer timer;
    struct uwsc_client *cl;
    struct ev_io ior;
    struct ev_io iow;
    struct ev_child cw;
    struct buffer rb;
    struct buffer wb;
    struct avl_node avl;

    int downfile;
    struct upfile_info upfile;
};

static char login[128];       /* /bin/login */
static char server_url[512];
static bool auto_reconnect;
static int keepalive = 5;       /* second */
static struct ev_timer reconnect_timer;
static struct avl_tree tty_sessions;

static void del_tty_session(struct tty_session *tty)
{
    ev_io_stop(tty->loop, &tty->ior);
    ev_io_stop(tty->loop, &tty->iow);
    ev_timer_stop(tty->loop, &tty->timer);
    ev_child_stop(tty->loop, &tty->cw);

    buffer_free(&tty->rb);
    buffer_free(&tty->wb);

    avl_delete(&tty_sessions, &tty->avl);

    close(tty->pty);
    kill(tty->pid, SIGTERM);
    waitpid(tty->pid, NULL, 0);
    close(tty->upfile.fd);
    uwsc_log_info("Del session:%s\n", tty->sid);
    free(tty);
}

static inline struct tty_session *find_tty_session(const char *sid)
{
    struct tty_session *tty;

    return avl_find_element(&tty_sessions, sid, tty, avl);
}

static inline void del_tty_session_by_sid(const char *sid)
{
    struct tty_session *tty = find_tty_session(sid);
    if (tty)
        del_tty_session(tty);
}

static void pty_read_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct tty_session *tty = container_of(w, struct tty_session, ior);
    RttyMessage *msg = rtty_message_init(RTTY_MESSAGE__TYPE__TTY, tty->sid);
    struct uwsc_client *cl = tty->cl;
    struct buffer *rb = &cl->rb;
    bool eof;
    int len;

    len = buffer_put_fd(rb, w->fd, -1, &eof, NULL, NULL);
    if (len <= 0) {
        if (errno != EIO)
            uwsc_log_err("Read from pty failed: %s\n", strerror(errno));
        return;
    }

    while (buffer_length(rb)) {
        len = buffer_length(rb);
        if (len > 65000)
            len = 65000;
        rtty_message_set_data(msg, buffer_data(rb), len);
        rtty_message_send(cl, msg);
        buffer_pull(rb, NULL, len);
    }
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
    RttyMessage *msg = rtty_message_init(RTTY_MESSAGE__TYPE__LOGOUT, tty->sid);

    rtty_message_send(tty->cl, msg);

    del_tty_session(tty);
}

static void new_tty_session(struct uwsc_client *cl, RttyMessage *msg)
{
    struct tty_session *s;
    int pty;
    pid_t pid;

    s = calloc(1, sizeof(struct tty_session));
    if (!s)
        return;

    pid = forkpty(&pty, NULL, NULL, NULL);
    if (pid == 0)
        execl(login, login, NULL);

    s->cl = cl;
    s->pid = pid;
    s->pty = pty;
    s->loop = cl->loop;
    strcpy(s->sid, msg->sid);

    fcntl(pty, F_SETFL, fcntl(pty, F_GETFL, 0) | O_NONBLOCK);

    ev_io_init(&s->ior, pty_read_cb, pty, EV_READ);
    ev_io_start(cl->loop, &s->ior);

    ev_io_init(&s->iow, pty_write_cb, pty, EV_WRITE);

    ev_child_init(&s->cw, pty_on_exit, pid, 0);
    ev_child_start(cl->loop, &s->cw);

    s->avl.key = s->sid;
    avl_insert(&tty_sessions, &s->avl);

    uwsc_log_info("New session:%s\n", msg->sid);
}

static void pty_write(RttyMessage *msg)
{
    struct tty_session *tty = find_tty_session(msg->sid);
    ProtobufCBinaryData *data = &msg->data;

    if (!tty)
        return;

    buffer_put_data(&tty->wb, data->data, data->len);
    ev_io_start(tty->loop, &tty->iow);
}

static void handle_upfile(RttyMessage *msg)
{
    struct tty_session *tty = find_tty_session(msg->sid);
    struct upfile_info *upfile;
    ProtobufCBinaryData *data = &msg->data;

    if (!tty)
        return;

    upfile = &tty->upfile;

    if (msg->code == RTTY_MESSAGE__FILE_CODE__START) {
        char path[512] = "";
        if (upfile->fd > 0) {
            uwsc_log_err("Only one file can be uploading at the same time\n");
            return;
        }

        if (!msg->name || msg->size == 0) {
            uwsc_log_err("Upfile failed: name and size required\n");
            return;
        }

        snprintf(path, sizeof(path) - 1, "/tmp/%s", msg->name);
        upfile->fd = open(path, O_CREAT | O_RDWR, 0644);
        if (upfile->fd < 0) {
            uwsc_log_err("open upfile failed: %s\n", strerror(errno));
            return;
        }

        upfile->size = msg->size;
        upfile->uploaded = 0;

        uwsc_log_info("Begin upload:%s %d\n", msg->name, msg->size);
    }

    if (upfile->fd > 0) {
        if (msg->code == RTTY_MESSAGE__FILE_CODE__FILEDATA && data && data->len) {
            if (write(upfile->fd, data->data, data->len) < 0) {
                uwsc_log_err("upfile failed:%s\n", strerror(errno));
                close(upfile->fd);
                upfile->fd = 0;
                return;
            }

            upfile->uploaded += data->len;
        }

        if (msg->code == RTTY_MESSAGE__FILE_CODE__CANCELED ||
            upfile->uploaded == upfile->size)  {
            close(upfile->fd);
            upfile->fd = 0;
            uwsc_log_info("Upload file %s\n",
                (msg->code == RTTY_MESSAGE__FILE_CODE__FILEDATA) ? "finish" : "canceled");
        }
    }
}

static void send_filelist(struct uwsc_client *cl, const char *sid, const char *path)
{
    RttyMessage *msg = rtty_message_init(RTTY_MESSAGE__TYPE__DOWNFILE, sid);
    RttyMessage__File **filelist = NULL;
    struct dirent *dentry;
    int n_filelist = 0;
    char buf[512];
    DIR *dir = NULL;

    dir = opendir(path);
    if (!dir) {
        uwsc_log_err("opendir '%s' failed\n", path);
        goto done;
    }

    filelist = calloc(FILE_LIST_MAX, sizeof(RttyMessage__File *));
    if (!filelist) {
        uwsc_log_err("calloc failed\n");
        goto done;
    }

    if (rtty_message_file_init(&filelist[n_filelist], "..", true, 0, 0) < 0)
        goto done;
    n_filelist++;

    while((dentry = readdir(dir))) {
        const char *name = dentry->d_name;
        int type = dentry->d_type;
        struct stat st;

        if(!strncmp(name, ".", 1))
            continue;

        if(!strcmp(path, "/") &&
            (!strcmp(name, "dev") || !strcmp(name, "proc") || !strcmp(name, "sys")))
            continue;

        if (type != DT_DIR && type != DT_REG)
            continue;

        snprintf(buf, sizeof(buf) - 1, "%s%s", path, name);
        if (stat(buf, &st) < 0) {
            uwsc_log_err("stat '%s': %s\n", buf, strerror(errno));
            continue;
        }

        if (type == DT_REG && st.st_size > INT_MAX)
            continue;

        if (rtty_message_file_init(&filelist[n_filelist], name, type == DT_DIR,
            st.st_mtime, st.st_size) < 0)
            break;

        if (n_filelist++ == FILE_LIST_MAX)
            break;
    }

done:
    if (dir)
        closedir(dir);

    msg->n_filelist = n_filelist;
    msg->filelist = filelist;

    rtty_message_send(cl, msg);

    if (filelist) {
        for (int i = 0; i < n_filelist; i++)
            free(filelist[i]);

        free(filelist);
    }
}

static void send_file_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    struct tty_session *tty = container_of(w, struct tty_session, timer);
    RttyMessage *msg = rtty_message_init(RTTY_MESSAGE__TYPE__DOWNFILE, tty->sid);

    if (tty->downfile > 0) {
        int len;
        char buf[4096];

        len = read(tty->downfile, buf, sizeof(buf));
        if (len > 0) {
            rtty_message_set_code(msg, RTTY_MESSAGE__FILE_CODE__FILEDATA);
            rtty_message_set_data(msg, buf, len);

            ev_timer_start(loop, w);
        } else {
            close(tty->downfile);
            tty->downfile = 0;

            rtty_message_set_code(msg, RTTY_MESSAGE__FILE_CODE__END);

            uwsc_log_info("Down file finish\n");
        }

        rtty_message_send(tty->cl, msg);
    }
}

static void handle_downfile(struct uwsc_client *cl, RttyMessage *msg)
{
    struct stat st;
    const char *name = "/";
    struct tty_session *tty = find_tty_session(msg->sid);

    if (!tty)
        return;

    if (msg->code == RTTY_MESSAGE__FILE_CODE__CANCELED) {
        if (tty->downfile > 0) {
            close(tty->downfile);
            tty->downfile = 0;
            uwsc_log_info("Down file canceled\n");
        }
        return;
    }

    if (msg->name)
        name = msg->name;

    if (stat(name, &st) < 0) {
        uwsc_log_err("down (%s) failed:%s\n", name, strerror(errno));
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        send_filelist(cl, msg->sid, name);
    } else {
        if (tty->downfile > 0) {
            uwsc_log_err("Only one file can be downloading at the same time\n");
            return;
        }

        tty->downfile = open(name, O_RDONLY);
        if (tty->downfile < 0) {
            uwsc_log_err("open downfile failed: %s\n", strerror(errno));
            return;
        }

        uwsc_log_info("Begin down file:%s %d\n", msg->name, st.st_size);
        ev_timer_init(&tty->timer, send_file_cb, 5.0, 0.0);
        send_file_cb(tty->loop, &tty->timer, 0);
    }
}

static void change_winsize(RttyMessage *msg)
{
    struct tty_session *tty = find_tty_session(msg->sid);
    struct winsize size = {
        .ws_col = msg->cols,
        .ws_row = msg->rows
    };

    if(ioctl(tty->pty, TIOCSWINSZ, &size) < 0)
        uwsc_log_err("ioctl TIOCSWINSZ error\n");
}

static void uwsc_onmessage(struct uwsc_client *cl, void *data, size_t len, bool binary)
{
    RttyMessage *msg;

    if (!binary)
        return;

    msg = rtty_message__unpack(NULL, len, data);

    switch (msg->type) {
    case RTTY_MESSAGE__TYPE__LOGIN:
        new_tty_session(cl, msg);
        break;
    case RTTY_MESSAGE__TYPE__LOGOUT:
        del_tty_session_by_sid(msg->sid);
        break;
    case RTTY_MESSAGE__TYPE__TTY:
        pty_write(msg);
        break;
    case RTTY_MESSAGE__TYPE__UPFILE:
        handle_upfile(msg);
        break;
    case RTTY_MESSAGE__TYPE__DOWNFILE:
        handle_downfile(cl, msg);
        break;
    case RTTY_MESSAGE__TYPE__COMMAND:
        run_command(cl, msg, data, len);
        break;
    case RTTY_MESSAGE__TYPE__WINSIZE:
        change_winsize(msg);
        break;
    default:
        break;
    }

    rtty_message__free_unpacked(msg, NULL);
}

static void uwsc_onopen(struct uwsc_client *cl)
{
    uwsc_log_info("Connect to server succeed\n");
}

static void uwsc_onerror(struct uwsc_client *cl, int err, const char *msg)
{
    uwsc_log_err("onerror:%d: %s\n", err, msg);

    free(cl);

	if (auto_reconnect)
        ev_timer_again(cl->loop, &reconnect_timer);
    else
        ev_break(cl->loop, EVBREAK_ALL);
}

static void uwsc_onclose(struct uwsc_client *cl, int code, const char *reason)
{
    struct tty_session *tty, *tmp;

    uwsc_log_err("onclose:%d: %s\n", code, reason);

    avl_for_each_element_safe(&tty_sessions, tty, avl, tmp)
        del_tty_session(tty);

    free(cl);

    if (auto_reconnect)
        ev_timer_again(cl->loop, &reconnect_timer);
    else
        ev_break(cl->loop, EVBREAK_ALL);
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

    if (!auto_reconnect)
   	    ev_break(cl->loop, EVBREAK_ALL);
}

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
    if (w->signum == SIGINT) {
        ev_break(loop, EVBREAK_ALL);
        uwsc_log_info("Normal quit\n");
    }
}

static int avl_strcmp(const void *k1, const void *k2, void *ptr)
{
    return strcmp(k1, k2);
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
    bool verbose = false;
    bool ssl = false;

    while ((opt = getopt(argc, argv, "i:h:p:I:avd:sk:V")) != -1) {
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
        default: /* '?' */
            usage(argv[0]);
        }
    }

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

    avl_init(&tty_sessions, avl_strcmp, false, NULL);

    snprintf(server_url, sizeof(server_url), "ws%s://%s:%d/ws?device=1&devid=%s&description=%s&proto=%d"
            "&keepalive=%d", ssl ? "s" : "", host, port, devid, description ? description : "",
            RTTY_PROTO_VERSION, keepalive);
    free(description);

    ev_timer_init(&reconnect_timer, do_connect, 0.0, RECONNECT_INTERVAL);
	ev_timer_start(loop, &reconnect_timer);

    ev_signal_init(&signal_watcher, signal_cb, SIGINT);
    ev_signal_start(loop, &signal_watcher);

    ev_run(loop, 0);
    
    return 0;
}
