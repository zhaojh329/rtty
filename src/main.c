/*
 * Copyright (C) 2017 Jianhui Zhao <jianhuizhao329@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <pty.h>
#include <fcntl.h>
#include <ctype.h>
#include <uwsc/uwsc.h>
#include <libubox/ulog.h>
#include <libubox/blobmsg_json.h>

#include "config.h"
#include "utils.h"

#define KEEPALIVE_INTERVAL  10
#define RECONNECT_INTERVAL  5

struct tty_session {
    pid_t pid;
    int pty;
    char sid[33];
    struct ustream_fd sfd;
    struct uloop_process up;
    struct list_head node;
};

enum {
    RTTYD_TYPE,
    RTTYD_SID,
    RTTYD_ERR,
    RTTYD_DATA,
    RTTYD_NAME,
    RTTYD_SIZE
};

static const struct blobmsg_policy pol[] = {
    [RTTYD_TYPE] = {
        .name = "type",
        .type = BLOBMSG_TYPE_STRING
    },
    [RTTYD_SID] = {
        .name = "sid",
        .type = BLOBMSG_TYPE_STRING
    },
    [RTTYD_ERR] = {
        .name = "err",
        .type = BLOBMSG_TYPE_STRING
    },
    [RTTYD_DATA] = {
        .name = "data",
        .type = BLOBMSG_TYPE_STRING
    },
    [RTTYD_NAME] = {
        .name = "name",
        .type = BLOBMSG_TYPE_STRING
    },
    [RTTYD_SIZE] = {
        .name = "size",
        .type = BLOBMSG_TYPE_INT32
    }
};

static char buf[4096 * 10];
static struct blob_buf b;
static char did[64];          /* device id */
static char login[128];       /* /bin/login */
static char server_url[512];
static int active;
static int upfile = -1;         /* The file descriptor of file uploading */
static uint32_t upfile_size;    /* The file size of file uploading */
static uint32_t uploaded;       /* uploaded size */
static bool auto_reconnect;
struct uloop_timeout keepalive_timer;
struct uloop_timeout reconnect_timer;
struct uwsc_client *gcl;

static LIST_HEAD(tty_sessions);


static void keepalive(struct uloop_timeout *utm)
{
    char *str;

    blobmsg_buf_init(&b);
    blobmsg_add_string(&b, "type", "ping");

    str = blobmsg_format_json(b.head, true);
    gcl->send(gcl, str, strlen(str), WEBSOCKET_OP_TEXT);
    free(str);

    if (!active--) {
        ULOG_ERR("keepalive timeout\n");
        if (auto_reconnect) {
            gcl->send(gcl, NULL, 0, WEBSOCKET_OP_CLOSE);
        } else
            uloop_end();
        return;
    }

    uloop_timeout_set(utm, KEEPALIVE_INTERVAL * 1000);
}

static void del_tty_session(struct tty_session *tty)
{
    list_del(&tty->node);
    uloop_process_delete(&tty->up);
    ustream_free(&tty->sfd.stream);
    close(tty->pty);
    kill(tty->pid, SIGTERM);
    waitpid(tty->pid, NULL, 0);
}

static void pty_read_cb(struct ustream *s, int bytes)
{
    struct tty_session *tty = container_of(s, struct tty_session, sfd.stream);
    char *str;
    int len;

    str = ustream_get_read_buf(s, &len);
    
    blobmsg_buf_init(&b);
    blobmsg_add_string(&b, "type", "data");
    blobmsg_add_string(&b, "sid", tty->sid);

    b64_encode(str, len, buf, sizeof(buf));
    ustream_consume(s, len);

    blobmsg_add_string(&b, "data", buf);

    str = blobmsg_format_json(b.head, true);
    gcl->send(gcl, str, strlen(str), WEBSOCKET_OP_TEXT);
    free(str);
}

static void pty_on_exit(struct uloop_process *p, int ret)
{
    struct tty_session *tty = container_of(p, struct tty_session, up);
    char *str;

    blobmsg_buf_init(&b);
    blobmsg_add_string(&b, "type", "logout");
    blobmsg_add_string(&b, "did", did);
    blobmsg_add_string(&b, "sid", tty->sid);

    str = blobmsg_format_json(b.head, true);
    gcl->send(gcl, str, strlen(str), WEBSOCKET_OP_TEXT);
    free(str);

    del_tty_session(tty);
}

static void new_tty_session(struct blob_attr **tb)
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
    memcpy(s->sid, blobmsg_get_string(tb[RTTYD_SID]), 32);
    
    list_add(&s->node, &tty_sessions);

    s->sfd.stream.notify_read = pty_read_cb;
    ustream_fd_init(&s->sfd, s->pty);

    s->up.pid = pid;
    s->up.cb = pty_on_exit;
    uloop_process_add(&s->up);
}

static void uwsc_onopen(struct uwsc_client *cl)
{
    active = 3;
    keepalive_timer.cb = keepalive;
    uloop_timeout_set(&keepalive_timer, KEEPALIVE_INTERVAL * 1000);
    ULOG_INFO("onopen\n");
}

static void write_file(char *msg, uint64_t len)
{
    if (upfile) {
        if (write(upfile, msg, len) < 0) {
            ULOG_ERR("upfile failed:%s\n", strerror(errno));
            close(upfile);
            upfile = -1;
            return;
        }
        uploaded += len;

        if (uploaded == upfile_size) {
            close(upfile);
            upfile = -1;
            ULOG_INFO("upload finish\n");
        }
    }
}

static void uwsc_onmessage(struct uwsc_client *cl, char *msg, uint64_t len, enum websocket_op op)
{
    struct blob_attr *tb[ARRAY_SIZE(pol)];
    const char *type;

    if (op == WEBSOCKET_OP_BINARY) {
        write_file(msg, len);
        return;
    }

    blobmsg_buf_init(&b);

    blobmsg_add_json_from_string(&b, msg);

    if (blobmsg_parse(pol, ARRAY_SIZE(pol), tb, blob_data(b.head), blob_len(b.head)) != 0) {
        ULOG_ERR("Parse failed\n");
        return;
    }

    if (!tb[RTTYD_TYPE])
        return;

    type = blobmsg_get_string(tb[RTTYD_TYPE]);
    if (!strcmp(type, "login")) {
        new_tty_session(tb);
    } else if (!strcmp(type, "logout")) {
        struct tty_session *tty, *tmp;
        const char *sid;

        if (!tb[RTTYD_SID])
            return;

        sid = blobmsg_get_string(tb[RTTYD_SID]);

        list_for_each_entry_safe(tty, tmp, &tty_sessions, node) {
            if (!strcmp(tty->sid, sid)) {
                del_tty_session(tty);
            }
        }
    } else if (!strcmp(type, "data")) {
        const char *sid, *data;
        struct tty_session *tty;
        int len;

        if (!tb[RTTYD_SID] || !tb[RTTYD_DATA])
            return;

        sid = blobmsg_get_string(tb[RTTYD_SID]);
        data = blobmsg_get_string(tb[RTTYD_DATA]);

        len = b64_decode(data, buf, sizeof(buf));
        list_for_each_entry(tty, &tty_sessions, node) {
            if (!strcmp(tty->sid, sid)) {
                if (write(tty->pty, buf, len) < 0)
                    perror("write");
                break;
            }
        }
    } else if (!strcmp(type, "pong")) {
        active = 3;
    } else if (!strcmp(type, "add")) {
        if (tb[RTTYD_ERR]) {
            ULOG_ERR("add failed: %s\n", blobmsg_get_string(tb[RTTYD_ERR]));
            uloop_end();
        }
    } else if (!strcmp(type, "upfile")) {
        if (!tb[RTTYD_NAME] || !tb[RTTYD_SIZE]) {
            ULOG_ERR("upfile failed: Invalid param\n");
            return;
        }

        if (upfile < 0) {
            snprintf(buf, sizeof(buf) - 1, "/tmp/%s", blobmsg_get_string(tb[RTTYD_NAME]));
            upfile = open(buf, O_CREAT | O_RDWR, 0644);
            if (upfile < 0) {
                ULOG_ERR("open upfile failed: %s\n", strerror(errno));
                return;
            }

            uploaded = 0;
            upfile_size = blobmsg_get_u32(tb[RTTYD_SIZE]);

            ULOG_INFO("Begin upload file:%d %s\n", upfile_size, buf);
        } else {
            ULOG_ERR("Only one file can be uploaded at the same time\n");
            return;
        }
    }
}

static void uwsc_onerror(struct uwsc_client *cl)
{
    ULOG_ERR("onerror:%d\n", cl->error);
}

static void uwsc_onclose(struct uwsc_client *cl)
{
    active = 0;
    ULOG_ERR("onclose\n");

    if (auto_reconnect) {
        cl->free(cl);
        cl = NULL;
        uloop_timeout_set(&reconnect_timer, RECONNECT_INTERVAL * 1000);
    } else {
        uloop_end();
    }
}

static void do_connect(struct uloop_timeout *utm)
{
    uloop_timeout_cancel(&keepalive_timer);

    gcl = uwsc_new_ssl(server_url, NULL, false);
    if (gcl) {
        gcl->onopen = uwsc_onopen;
        gcl->onmessage = uwsc_onmessage;
        gcl->onerror = uwsc_onerror;
        gcl->onclose = uwsc_onclose;
        return;
    }

    if (uloop_cancelled || !auto_reconnect)
        uloop_end();
}

static int find_login()
{
    FILE *fp = popen("which login", "r");
    if (fp) {
        if (fgets(login, sizeof(login), fp))
            login[strlen(login) - 1] = 0;
        pclose(fp);

        if (!login[0])
            return -1;
        return 0;
    }

    return -1;
}

static bool valid_id(const char *id)
{
    while (*id) {
        if (!isalnum(*id) && *id != '-' && *id != '_')
            return false;
        id++;
    }

    return true;
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
        , prog);
    exit(1);
}

int main(int argc, char **argv)
{
    int opt;
    char mac[13] = "";
    const char *host = NULL;
    const char *description = NULL;
    int port = 0;
    bool verbose = false;
    bool ssl = false;

    while ((opt = getopt(argc, argv, "i:h:p:I:avd:s")) != -1) {
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
            strncpy(did, optarg, sizeof(did) - 1);
            break;
        case 'a':
            auto_reconnect = true;
            break;
        case 'v':
            verbose = true;
            break;
        case 'd':
            description = optarg;
            if (strlen(description) > 126) {
                ULOG_ERR("Description too long\n");
                usage(argv[0]);
            }
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

    if (!did[0]) {
        if (!mac[0]) {
            ULOG_ERR("You must specify the ifname or id\n");
            usage(argv[0]);
        }
        strcpy(did, mac);
    }

    if (!valid_id(did)) {
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

    if (find_login() < 0) {
        ULOG_ERR("The program 'login' is not found\n");
        return -1;
    }

    uloop_init();

    memset(buf, 0, sizeof(buf));
    if (description)
        urlencode(buf, sizeof(buf), description, strlen(description));

    snprintf(server_url, sizeof(server_url), "ws%s://%s:%d/ws/device?did=%s&des=%s", ssl ? "s" : "", host, port, did, buf);

    reconnect_timer.cb = do_connect;
    do_connect(&reconnect_timer);

    if (gcl) {
        uloop_run();

        gcl->send(gcl, NULL, 0, WEBSOCKET_OP_CLOSE);
        gcl->free(gcl);
    }

    uloop_done();
    
    return 0;
}
