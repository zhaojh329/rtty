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
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <uwsc/uwsc.h>
#include <libubox/avl.h>
#include <libubox/avl-cmp.h>

#include "config.h"
#include "utils.h"
#include "message.h"
#include "command.h"

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
    int downfile;
    struct upfile_info upfile;
    struct uloop_timeout timer;
    struct uwsc_client *cl;
    struct ustream_fd sfd;
    struct uloop_process up;
    struct avl_node avl;
};

static char login[128];       /* /bin/login */
static char server_url[512];
static bool auto_reconnect;
static int ping_interval;
static struct uloop_timeout reconnect_timer;
static struct avl_tree tty_sessions;

static void del_tty_session(struct tty_session *tty)
{
    avl_delete(&tty_sessions, &tty->avl);
    uloop_timeout_cancel(&tty->timer);
    uloop_process_delete(&tty->up);
    ustream_free(&tty->sfd.stream);
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

static void pty_read_cb(struct ustream *s, int bytes)
{
    struct tty_session *tty = container_of(s, struct tty_session, sfd.stream);
    RttyMessage *msg = rtty_message_init(RTTY_MESSAGE__TYPE__TTY, tty->sid);
    struct uwsc_client *cl = tty->cl;
    void *data;
    int len;

    data = ustream_get_read_buf(s, &len);
    if (!data || len == 0)
        return;

    rtty_message_set_data(msg, data, len);

    rtty_message_send(cl, msg);

    ustream_consume(s, len);
}

static void pty_on_exit(struct uloop_process *p, int ret)
{
    struct tty_session *tty = container_of(p, struct tty_session, up);
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

    s->pid = pid;
    s->pty = pty;
    strcpy(s->sid, msg->sid);

    s->sfd.stream.notify_read = pty_read_cb;
    ustream_fd_init(&s->sfd, s->pty);

    s->cl = cl;
    s->up.pid = pid;
    s->up.cb = pty_on_exit;
    uloop_process_add(&s->up);

    s->avl.key = s->sid;
    avl_insert(&tty_sessions, &s->avl);

    uwsc_log_info("New session:%s\n", msg->sid);
}

static void pty_write(RttyMessage *msg)
{
    struct tty_session *tty = find_tty_session(msg->sid);
    ProtobufCBinaryData *data = &msg->data;

    if (tty && write(tty->pty, data->data, data->len) < 0) {
        uwsc_log_err("write to pty error:%s\n", strerror(errno));
    }
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

static void send_file_cb(struct uloop_timeout *timer)
{
    struct tty_session *tty = container_of(timer, struct tty_session, timer);
    RttyMessage *msg = rtty_message_init(RTTY_MESSAGE__TYPE__DOWNFILE, tty->sid);

    if (tty->downfile > 0) {
        int len;
        char buf[4096];

        len = read(tty->downfile, buf, sizeof(buf));
        if (len > 0) {
            rtty_message_set_code(msg, RTTY_MESSAGE__FILE_CODE__FILEDATA);
            rtty_message_set_data(msg, buf, len);

            uloop_timeout_set(timer, 5);
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
        tty->timer.cb = send_file_cb;
        send_file_cb(&tty->timer);
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

static void uwsc_onmessage(struct uwsc_client *cl, void *data, uint64_t len, enum websocket_op op)
{
    RttyMessage *msg = rtty_message__unpack(NULL, len, data);

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
    uwsc_log_info("onopen\n");

    cl->set_ping_interval(cl, ping_interval);
}

static void uwsc_onerror(struct uwsc_client *cl)
{
    uwsc_log_err("onerror:%d\n", cl->error);
}

static void uwsc_onclose(struct uwsc_client *cl)
{
    struct tty_session *tty, *tmp;

    uwsc_log_err("onclose\n");

    avl_for_each_element_safe(&tty_sessions, tty, avl, tmp)
        del_tty_session(tty);

    cl->send(cl, NULL, 0, WEBSOCKET_OP_CLOSE);
    cl->free(cl);

    if (auto_reconnect)
        uloop_timeout_set(&reconnect_timer, RECONNECT_INTERVAL * 1000);
    else
        uloop_end();
}

static void do_connect(struct uloop_timeout *utm)
{
    struct uwsc_client *cl = uwsc_new_ssl(server_url, NULL, false);
    if (cl) {
        cl->onopen = uwsc_onopen;
        cl->onmessage = uwsc_onmessage;
        cl->onerror = uwsc_onerror;
        cl->onclose = uwsc_onclose;
        return;
    }

    if (uloop_cancelled || !auto_reconnect)
        uloop_end();
    else
        uloop_timeout_set(&reconnect_timer, RECONNECT_INTERVAL * 1000);
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
        "      -P interval  # Set ping interval(s)\n"
        "      -a           # Auto reconnect to the server\n"
        "      -v           # verbose\n"
        "      -d           # Adding a description to the device(Maximum 126 bytes)\n"
        "      -s           # SSL on\n"
        "      -V           # Show version\n"
        , prog);
    exit(1);
}

int main(int argc, char **argv)
{
    int opt;
    char mac[13] = "";
    char devid[64] = "";
    const char *host = NULL;
    int port = 0;
    char *description = NULL;
    bool verbose = false;
    bool ssl = false;

    while ((opt = getopt(argc, argv, "i:h:p:I:avd:sP:V")) != -1) {
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
        case 'P':
            ping_interval = atoi(optarg);
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

    uloop_init();

    snprintf(server_url, sizeof(server_url), "ws%s://%s:%d/ws?device=1&devid=%s&description=%s",
        ssl ? "s" : "", host, port, devid, description ? description : "");
    free(description);

    reconnect_timer.cb = do_connect;
    uloop_timeout_set(&reconnect_timer, 100);

    command_init();

    uloop_run();
    uloop_done();
    
    return 0;
}
