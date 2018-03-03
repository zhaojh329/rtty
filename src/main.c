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
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <pty.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <uwsc/uwsc.h>
#include <libubox/ulog.h>
#include <libubox/blobmsg_json.h>
#include <libubox/avl.h>
#include <libubox/avl-cmp.h>

#include "config.h"
#include "utils.h"
#include "protocol.h"

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
static struct rtty_packet *pkt;
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
    ULOG_INFO("Del session:%s\n", tty->sid);
    free(tty);

    if (avl_is_empty(&tty_sessions)) {
        rtty_packet_free(pkt);
        pkt = NULL;
    }
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
    struct uwsc_client *cl = tty->cl;
    void *data;
    int len;

    data = ustream_get_read_buf(s, &len);
    if (!data || len == 0)
        return;

    rtty_packet_init(pkt, RTTY_PACKET_TTY);
    rtty_attr_put_string(pkt, RTTY_ATTR_SID, tty->sid);
    rtty_attr_put(pkt, RTTY_ATTR_DATA, len, data);
    ustream_consume(s, len);

    cl->send(cl, pkt->data, pkt->len, WEBSOCKET_OP_BINARY);
}

static void pty_on_exit(struct uloop_process *p, int ret)
{
    struct tty_session *tty = container_of(p, struct tty_session, up);
    struct uwsc_client *cl = tty->cl;

    rtty_packet_init(pkt, RTTY_PACKET_LOGOUT);
    rtty_attr_put_string(pkt, RTTY_ATTR_SID, tty->sid);

    cl->send(cl, pkt->data, pkt->len, WEBSOCKET_OP_BINARY);

    del_tty_session(tty);
}

static void new_tty_session(struct uwsc_client *cl, struct rtty_packet_info *pi)
{
    struct tty_session *s;
    int pty;
    pid_t pid;

    if (!pkt) {
        pkt = rtty_packet_new(sizeof(struct rtty_packet) + 8192);
        if (!pkt) {
            ULOG_ERR("rtty_packet_new failed\n");
            return;
        }
    }

    s = calloc(1, sizeof(struct tty_session));
    if (!s)
        return;

    pid = forkpty(&pty, NULL, NULL, NULL);
    if (pid == 0)
        execl(login, login, NULL);

    s->pid = pid;
    s->pty = pty;
    strcpy(s->sid, pi->sid);

    s->sfd.stream.notify_read = pty_read_cb;
    ustream_fd_init(&s->sfd, s->pty);

    s->cl = cl;
    s->up.pid = pid;
    s->up.cb = pty_on_exit;
    uloop_process_add(&s->up);

    s->avl.key = s->sid;
    avl_insert(&tty_sessions, &s->avl);

    ULOG_INFO("New session:%s\n", pi->sid);
}

static void pty_write(struct rtty_packet_info *pi)
{
    struct tty_session *tty = find_tty_session(pi->sid);

    if (tty && write(tty->pty, pi->data, pi->data_len) < 0) {
        ULOG_ERR("write to pty error:%s\n", strerror(errno));
    }
}

static void handle_upfile(struct rtty_packet_info *pi)
{
    struct tty_session *tty = find_tty_session(pi->sid);
    struct upfile_info *upfile;

    if (!tty)
        return;

    upfile = &tty->upfile;

    if (pi->code == 0) {
        char path[512] = "";
        if (upfile->fd > 0) {
            ULOG_ERR("Only one file can be uploading at the same time\n");
            return;
        }

        if (!pi->name || pi->size == 0) {
            ULOG_ERR("Upfile failed: name and size required\n");
            return;
        }

        snprintf(path, sizeof(path) - 1, "/tmp/%s", pi->name);
        upfile->fd = open(path, O_CREAT | O_RDWR, 0644);
        if (upfile->fd < 0) {
            ULOG_ERR("open upfile failed: %s\n", strerror(errno));
            return;
        }

        upfile->size = pi->size;
        upfile->uploaded = 0;

        ULOG_INFO("Begin upload:%s %d\n", pi->name, pi->size);
    }

    if (upfile->fd > 0) {
        if (pi->code == 1 && pi->data && pi->data_len) {
            if (write(upfile->fd, pi->data, pi->data_len) < 0) {
                ULOG_ERR("upfile failed:%s\n", strerror(errno));
                close(upfile->fd);
                upfile->fd = 0;
                return;
            }

            upfile->uploaded += pi->data_len;
        }

        if (pi->code == 2 || upfile->uploaded == upfile->size)  {
            close(upfile->fd);
            upfile->fd = 0;
            ULOG_INFO("Upload file %s\n", (pi->code == 1) ? "finish" : "canceled");
        }
    }
}

static void send_filelist(struct uwsc_client *cl, const char *sid, const char *path)
{
    static struct blob_buf b;
    DIR *dir;
    struct dirent *dentry;
    void *tbl, *array;
    char *str;
    char buf[512];

    dir = opendir(path);
    if (!dir)
        return;

    blobmsg_buf_init(&b);

    array = blobmsg_open_array(&b, "");

    tbl = blobmsg_open_table(&b, "");
    blobmsg_add_string(&b, "name", "..");
    blobmsg_add_u8(&b, "dir", 1);
    blobmsg_close_table(&b, tbl);

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

        tbl = blobmsg_open_table(&b, "");

        blobmsg_add_string(&b, "name", name);
        blobmsg_add_u8(&b, "dir", type == DT_DIR);

        snprintf(buf, sizeof(buf) - 1, "%s%s", path, name);
        if (stat(buf, &st) < 0) {
            ULOG_ERR("stat '%s': %s\n", buf, strerror(errno));
            continue;
        }

        blobmsg_add_u32(&b, "mtim", st.st_mtime);

        if (type == DT_REG)
            blobmsg_add_u32(&b, "size", st.st_size);
        blobmsg_close_table(&b, tbl);
    }
    closedir(dir);
    blobmsg_close_table(&b, array);

    str = blobmsg_format_json(b.head, true);

    rtty_packet_init(pkt, RTTY_PACKET_DOWNFILE);
    rtty_attr_put_string(pkt, RTTY_ATTR_SID, sid);
    rtty_attr_put(pkt, RTTY_ATTR_DATA, strlen(str) - 2, str + 1);
    cl->send(cl, pkt->data, pkt->len, WEBSOCKET_OP_BINARY);

    free(str);
    blob_buf_free(&b);
}

static void send_file_cb(struct uloop_timeout *timer)
{
    struct tty_session *tty = container_of(timer, struct tty_session, timer);

    if (tty->downfile > 0) {
        int len;
        char buf[4096];

        rtty_packet_init(pkt, RTTY_PACKET_DOWNFILE);
        rtty_attr_put_string(pkt, RTTY_ATTR_SID, tty->sid);

        len = read(tty->downfile, buf, sizeof(buf));
        if (len > 0) {
            rtty_attr_put_u8(pkt, RTTY_ATTR_CODE, 1);
            rtty_attr_put(pkt, RTTY_ATTR_DATA, len, buf);
            uloop_timeout_set(timer, 5);
        } else {
            close(tty->downfile);
            tty->downfile = 0;
            rtty_attr_put_u8(pkt, RTTY_ATTR_CODE, 2);
            ULOG_INFO("Down file finish\n");
        }
        tty->cl->send(tty->cl, pkt->data, pkt->len, WEBSOCKET_OP_BINARY);
    }
}

static void handle_downfile(struct uwsc_client *cl, struct rtty_packet_info *pi)
{
    struct stat st;
    const char *name = "/";
    struct tty_session *tty = find_tty_session(pi->sid);

    if (!tty)
        return;

    if (pi->code == 1) {
        if (tty->downfile > 0) {
            close(tty->downfile);
            tty->downfile = 0;
            ULOG_INFO("Down file canceled\n");
        }
        return;
    }

    if (pi->name)
        name = pi->name;

    if (stat(name, &st) < 0) {
        ULOG_ERR("down (%s) failed:%s\n", name, strerror(errno));
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        send_filelist(cl, pi->sid, name);
    } else {
        if (tty->downfile > 0) {
            ULOG_ERR("Only one file can be downloading at the same time\n");
            return;
        }

        tty->downfile = open(name, O_RDONLY);
        if (tty->downfile < 0) {
            ULOG_ERR("open downfile failed: %s\n", strerror(errno));
            return;
        }

        ULOG_INFO("Begin down file:%s %d\n", pi->name, st.st_size);
        tty->timer.cb = send_file_cb;
        send_file_cb(&tty->timer);
    }
}

static void uwsc_onmessage(struct uwsc_client *cl, void *msg, uint64_t len, enum websocket_op op)
{
    struct rtty_packet_info pi;

    rtty_packet_parse(msg, len, &pi);

    switch (pi.type) {
    case RTTY_PACKET_LOGIN:
        new_tty_session(cl, &pi);
        break;
    case RTTY_PACKET_LOGOUT: {
        del_tty_session_by_sid(pi.sid);
        break;
    }
    case RTTY_PACKET_TTY:
        pty_write(&pi);
        break;
    case RTTY_PACKET_ANNOUNCE:
        if (pi.code) {
            auto_reconnect = false;
            ULOG_ERR("register failed: ID conflicting\n");
            uloop_end();
        }
    case RTTY_PACKET_UPFILE:
        handle_upfile(&pi);
        break;
    case RTTY_PACKET_DOWNFILE:
        handle_downfile(cl, &pi);
        break;
    default:
        break;
    }
}

static void uwsc_onopen(struct uwsc_client *cl)
{
    ULOG_INFO("onopen\n");

    cl->set_ping_interval(cl, ping_interval);
}

static void uwsc_onerror(struct uwsc_client *cl)
{
    ULOG_ERR("onerror:%d\n", cl->error);
}

static void uwsc_onclose(struct uwsc_client *cl)
{
    struct tty_session *tty, *tmp;

    ULOG_ERR("onclose\n");

    if (pkt) {
        rtty_packet_free(pkt);
        pkt = NULL;
    }

    avl_remove_all_elements(&tty_sessions, tty, avl, tmp)
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

    while ((opt = getopt(argc, argv, "i:h:p:I:avd:sP:")) != -1) {
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
                ULOG_ERR("Description too long\n");
                usage(argv[0]);
            }
            description = calloc(1, strlen(optarg) * 4);
            if (!description) {
                ULOG_ERR("malloc failed:%s\n", strerror(errno));
                exit(1);
            }
            urlencode(description, strlen(optarg) * 4, optarg, strlen(optarg));
            break;
        case 's':
            ssl = true;
            break;
        default: /* '?' */
            usage(argv[0]);
        }
    }

    if (!verbose)
        ulog_threshold(LOG_ERR);

    if (!devid[0]) {
        if (!mac[0]) {
            ULOG_ERR("You must specify the ifname or id\n");
            usage(argv[0]);
        }
        strcpy(devid, mac);
    }

    if (!valid_id(devid)) {
        ULOG_ERR("Invalid device id\n");
        usage(argv[0]);
    }

    if (!host || !port) {
        ULOG_ERR("You must specify the host and port\n");
        usage(argv[0]);
    }

    ULOG_INFO("rtty version %s\n", RTTY_VERSION_STRING);

    if (setuid(0) < 0) {
        ULOG_ERR("Operation not permitted\n");
        return -1;
    }

    if (find_login(login, sizeof(login) - 1) < 0) {
        ULOG_ERR("The program 'login' is not found\n");
        return -1;
    }

    avl_init(&tty_sessions, avl_strcmp, false, NULL);

    uloop_init();

    snprintf(server_url, sizeof(server_url), "ws%s://%s:%d/ws?device=1&devid=%s&description=%s",
        ssl ? "s" : "", host, port, devid, description ? description : "");
    free(description);

    reconnect_timer.cb = do_connect;
    uloop_timeout_set(&reconnect_timer, 100);

    uloop_run();
    uloop_done();
    
    return 0;
}
