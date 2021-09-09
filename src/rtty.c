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

#include <pty.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

#include "net.h"
#include "web.h"
#include "file.h"
#include "rtty.h"
#include "list.h"
#include "utils.h"
#include "command.h"
#include "log/log.h"

static char login_path[128];       /* /bin/login */

static void del_tty(struct tty *tty)
{
    struct rtty *rtty = tty->rtty;
    struct ev_loop *loop = rtty->loop;

    ev_io_stop(loop, &tty->ior);
    ev_io_stop(loop, &tty->iow);
    ev_child_stop(loop, &tty->cw);

    buffer_free(&tty->wb);

    close(tty->pty);
    kill(tty->pid, SIGTERM);

    rtty->ttys[tty->sid] = NULL;

    file_context_reset(&rtty->file_context);

    log_info("delete tty: %d\n", tty->sid);

    free(tty);
}

static inline struct tty *find_tty(struct rtty *rtty, int sid)
{
    if (sid > RTTY_MAX_TTY - 1)
        return NULL;
    return rtty->ttys[sid];
}

static inline void tty_logout(struct rtty *rtty, int sid)
{
    struct tty *tty = find_tty(rtty, sid);
    if (tty)
        del_tty(tty);
}

static void pty_on_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct tty *tty = container_of(w, struct tty, ior);
    struct rtty *rtty = tty->rtty;
    struct buffer *wb = &rtty->wb;
    static uint8_t buf[4096];
    int len;

    while (1) {
        len = read(w->fd, buf, sizeof(buf));
        if (likely(len > 0))
            break;

        if (len < 0) {
            if (errno == EINTR)
                continue;
            if (errno != EIO)
                log_err("read from pty failed: %s\n", strerror(errno));
            return;
        }

        if (len == 0)
            return;
    }

    if (detect_file_operation(buf, len, tty->sid, &rtty->file_context))
        return;

    buffer_put_u8(wb, MSG_TYPE_TERMDATA);
    buffer_put_u16be(wb, len + 1);
    buffer_put_u8(wb, tty->sid);
    buffer_put_data(wb, buf, len);
    ev_io_start(loop, &rtty->iow);
}

static void pty_on_write(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct tty *tty = container_of(w, struct tty, iow);
    struct buffer *wb = &tty->wb;
    int ret;

    ret = buffer_pull_to_fd(wb, w->fd, buffer_length(wb));
    if (ret < 0) {
        log_err("write to pty failed: %s\n", strerror(errno));
        return;
    }

    if (buffer_length(wb) < 1)
        ev_io_stop(loop, w);
}

static void pty_on_exit(struct ev_loop *loop, struct ev_child *w, int revents)
{
    struct tty *tty = container_of(w, struct tty, cw);
    struct rtty *rtty = tty->rtty;
    struct buffer *wb = &rtty->wb;

    buffer_put_u8(wb, MSG_TYPE_LOGOUT);
    buffer_put_u16be(wb, 1);
    buffer_put_u8(wb, tty->sid);
    ev_io_start(loop, &rtty->iow);

    del_tty(tty);
}

static void tty_login(struct rtty *rtty)
{
    struct tty *tty;
    pid_t pid;
    int pty;
    int sid;

    buffer_put_u8(&rtty->wb, MSG_TYPE_LOGIN);

    for (sid = 0; sid < RTTY_MAX_TTY; sid++) {
        if (!rtty->ttys[sid])
            break;
    }

    if (sid == RTTY_MAX_TTY) {
        log_info("tty login fail, device busy\n");
        goto err;
    }

    pid = forkpty(&pty, NULL, NULL, NULL);
    if (pid < 0) {
        log_err("forkpty: %s\n", strerror(errno));
        goto err;
    }

    if (pid == 0) {
        if (rtty->username)
            execl(login_path, "login", "-f", rtty->username, NULL);
        else
            execl(login_path, "login", NULL);
    }

    tty = calloc(1, sizeof(struct tty));

    tty->sid = sid;
    tty->pid = pid;
    tty->pty = pty;
    tty->rtty = rtty;

    fcntl(pty, F_SETFL, fcntl(pty, F_GETFL, 0) | O_NONBLOCK);

    ev_io_init(&tty->ior, pty_on_read, pty, EV_READ);
    ev_io_start(rtty->loop, &tty->ior);

    ev_io_init(&tty->iow, pty_on_write, pty, EV_WRITE);

    ev_child_init(&tty->cw, pty_on_exit, pid, 0);
    ev_child_start(rtty->loop, &tty->cw);

    rtty->ttys[sid] = tty;

    buffer_put_u16be(&rtty->wb, 2);
    buffer_put_u8(&rtty->wb, 0);
    buffer_put_u8(&rtty->wb, sid);
    ev_io_start(rtty->loop, &rtty->iow);

    log_info("new tty: %d\n", sid);

    return;

err:
    buffer_put_u16be(&rtty->wb, 1);
    buffer_put_u8(&rtty->wb, 1);
    ev_io_start(rtty->loop, &rtty->iow);
}

static void write_data_to_tty(struct rtty *rtty, int sid, int len)
{
    struct tty *tty = find_tty(rtty, sid);
    if (!tty) {
        log_err("non-existent sid: %d\n", sid);
        return;
    }

    buffer_put_data(&tty->wb, buffer_data(&rtty->rb), len);
    buffer_pull(&rtty->rb, NULL, len);
    ev_io_start(rtty->loop, &tty->iow);
}

static void set_tty_winsize(struct rtty *rtty, int sid)
{
    struct tty *tty = find_tty(rtty, sid);
    struct winsize size = {};

    if (!tty) {
        log_err("non-existent sid: %d\n", sid);
        return;
    }

    size.ws_col = buffer_pull_u16be(&rtty->rb);
    size.ws_row = buffer_pull_u16be(&rtty->rb);

    if (ioctl(tty->pty, TIOCSWINSZ, &size) < 0)
        log_err("ioctl TIOCSWINSZ failed: %s\n", strerror(errno));
}

void rtty_exit(struct rtty *rtty)
{
    if (rtty->sock < 0)
        return;

    ev_io_stop(rtty->loop, &rtty->ior);
    ev_io_stop(rtty->loop, &rtty->iow);

    buffer_free(&rtty->rb);
    buffer_free(&rtty->wb);

    for (int i = 0; i < RTTY_MAX_TTY; i++)
        if (rtty->ttys[i])
            del_tty(rtty->ttys[i]);

#ifdef SSL_SUPPORT
    if (rtty->ssl) {
        ssl_session_free(rtty->ssl);
        rtty->ssl_negotiated = false;
    }
#endif

    if (rtty->sock > 0) {
        close(rtty->sock);
        rtty->sock = -1;
    }

    web_reqs_free(&rtty->web_reqs);

    if (!rtty->reconnect)
        ev_break(rtty->loop, EVBREAK_ALL);
}

static void rtty_register(struct rtty *rtty)
{
    size_t len = 3 + strlen(rtty->devid);
    struct buffer *wb = &rtty->wb;

    if (rtty->description)
        len += strlen(rtty->description);

    if (rtty->token)
        len += strlen(rtty->token);

    buffer_put_u8(wb, MSG_TYPE_REGISTER);
    buffer_put_u16be(wb, len);

    buffer_put_string(wb, rtty->devid);
    buffer_put_u8(wb, '\0');

    if (rtty->description)
        buffer_put_string(wb, rtty->description);
    buffer_put_u8(wb, '\0');

    if (rtty->token)
        buffer_put_string(wb, rtty->token);
    buffer_put_u8(wb, '\0');

    ev_io_start(rtty->loop, &rtty->iow);
}

static int parse_msg(struct rtty *rtty)
{
    struct buffer *rb = &rtty->rb;
    int msgtype;
    int msglen;

    while (true) {
        if (buffer_length(rb) < 3)
            return 0;

        msglen = buffer_get_u16be(rb, 1);
        if (buffer_length(rb) < msglen + 3)
            return 0;

        msgtype = buffer_pull_u8(rb);
        if (msgtype > MSG_TYPE_MAX) {
            log_err("invalid message type: %d\n", msgtype);
            return -1;
        }

        buffer_pull_u16(rb);

        switch (msgtype) {
        case MSG_TYPE_REGISTER:
            if (buffer_pull_u8(rb)) {
                char errs[128] = "";
                buffer_pull(rb, errs, msglen - 1);
                log_err("register fail: %s\n", errs);
                return -1;
            }
            buffer_pull(rb, NULL, msglen - 1);
            log_info("register success\n");
            break;

        case MSG_TYPE_LOGIN:
            tty_login(rtty);
            break;

        case MSG_TYPE_LOGOUT:
            tty_logout(rtty, buffer_pull_u8(rb));
            break;

        case MSG_TYPE_TERMDATA:
            write_data_to_tty(rtty, buffer_pull_u8(rb), msglen - 1);
            break;

        case MSG_TYPE_WINSIZE:
            set_tty_winsize(rtty, buffer_pull_u8(rb));
            break;

        case MSG_TYPE_CMD:
            run_command(rtty, buffer_data(rb));
            buffer_pull(rb, NULL, msglen);
            break;

        case MSG_TYPE_HEARTBEAT:
            break;

        case MSG_TYPE_FILE:
            parse_file_msg(&rtty->file_context, rb, msglen);
            break;

        case MSG_TYPE_WEB:
            web_request(rtty, msglen);
            break;

        default:
            /* never to here */
            break;
        }
    }
}

#ifdef SSL_SUPPORT
static void on_ssl_verify_error(int error, const char *str, void *arg)
{
    log_warn("SSL certificate error(%d): %s\n", error, str);
}

/* -1 error, 0 pending, 1 ok */
static int ssl_negotiated(struct rtty *rtty)
{
    char err_buf[128];
    int ret;

    ret = ssl_connect(rtty->ssl, false, on_ssl_verify_error, NULL);
    if (ret == SSL_PENDING)
        return 0;

    if (ret == SSL_ERROR) {
        log_err("ssl connect error(%d): %s\n", ssl_err_code, ssl_strerror(ssl_err_code, err_buf, sizeof(err_buf)));
        return -1;
    }

    rtty->ssl_negotiated = true;

    return 1;
}

static int rtty_ssl_read(int fd, void *buf, size_t count, void *arg)
{
    static char err_buf[128];
    struct rtty *rtty = arg;
    int ret;

    ret = ssl_read(rtty->ssl, buf, count);
    if (ret == SSL_ERROR) {
        log_err("ssl_read(%d): %s\n", ssl_err_code,
                ssl_strerror(ssl_err_code, err_buf, sizeof(err_buf)));
        return P_FD_ERR;
    }

    if (ret == SSL_PENDING)
        return P_FD_PENDING;

    return ret;
}
#endif

static void on_net_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct rtty *rtty = container_of(w, struct rtty, ior);
    bool eof = false;
    int ret;

    if (rtty->ssl_on) {
#ifdef SSL_SUPPORT
        if (unlikely(!rtty->ssl_negotiated)) {
            ret = ssl_negotiated(rtty);
            if (ret < 0)
                goto err;
            if (ret == 0)
                return;
        }

        ret = buffer_put_fd_ex(&rtty->rb, w->fd, 4096, &eof, rtty_ssl_read, rtty);
        if (ret < 0)
            goto err;
#endif
    } else {
        ret = buffer_put_fd(&rtty->rb, w->fd, 4096, &eof);
        if (ret < 0) {
            log_err("socket read error: %s\n", strerror(errno));
            goto err;
        }
    }

    rtty->ninactive = 0;
    rtty->active = ev_now(loop);

    if (parse_msg(rtty))
        goto err;

    if (eof) {
        log_err("socket closed by server\n");
        goto err;
    }

    return;

err:
    rtty_exit(rtty);
}

static void on_net_write(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct rtty *rtty = container_of(w, struct rtty, iow);
    int ret;

    if (rtty->ssl_on) {
#ifdef SSL_SUPPORT
        static char err_buf[128];
        struct buffer *b = &rtty->wb;

        if (unlikely(!rtty->ssl_negotiated)) {
            ret = ssl_negotiated(rtty);
            if (ret < 0)
                goto err;
            if (ret == 0)
                return;
        }

        ret = ssl_write(rtty->ssl, buffer_data(b), buffer_length(b));
        if (ret == SSL_ERROR) {
            log_err("ssl_write(%d): %s\n", ssl_err_code,
                    ssl_strerror(ssl_err_code, err_buf, sizeof(err_buf)));
            goto err;
        }

        if (ret == SSL_PENDING)
            return;

        buffer_pull(b, NULL, ret);
#endif
    } else {
        ret = buffer_pull_to_fd(&rtty->wb, w->fd, -1);
        if (ret < 0) {
            log_err("socket write error: %s\n", strerror(errno));
            goto err;
        }
    }

    if (buffer_length(&rtty->wb) < 1)
        ev_io_stop(loop, w);

    return;

err:
    rtty_exit(rtty);
}

static void on_net_connected(int sock, void *arg)
{
    struct rtty *rtty = arg;

    if (sock < 0) {
        if (!rtty->reconnect)
            ev_break(rtty->loop, EVBREAK_ALL);
        return;
    }

    log_info("connected to server\n");

    rtty->sock = sock;
    rtty->ninactive = 0;

    ev_io_init(&rtty->ior, on_net_read, sock, EV_READ);
    ev_io_start(rtty->loop, &rtty->ior);

    ev_io_init(&rtty->iow, on_net_write, sock, EV_WRITE);

    if (rtty->ssl_on) {
#ifdef SSL_SUPPORT
        rtty->ssl = ssl_session_new(rtty->ssl_ctx, sock);
        if (!rtty->ssl) {
            log_err("SSL session create fail\n");
            ev_break(rtty->loop, EVBREAK_ALL);
        }
        ssl_set_server_name(rtty->ssl, rtty->host);
#endif
    }

    rtty_register(rtty);
}

static void rtty_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    struct rtty *rtty = container_of(w, struct rtty, tmr);
    ev_tstamp now = ev_now(loop);

    if (rtty->sock < 0) {
        if (now - rtty->active < 5)
            return;
        rtty->active = now;
        log_err("rtty reconnecting...\n");
        tcp_connect(rtty->loop, rtty->host, rtty->port, on_net_connected, rtty);
        return;
    }

    if (now - rtty->active > RTTY_HEARTBEAT_INTEVAL * 3 / 2) {
        log_err("Inactive too long time\n");
        if (rtty->ninactive++ > 1) {
            log_err("Inactive 3 times, now exit\n");
            rtty_exit(rtty);
            return;
        }
        rtty->active = now;
    }

    if (now - rtty->last_heartbeat > RTTY_HEARTBEAT_INTEVAL - 1) {
        struct sysinfo info = {};

        rtty->last_heartbeat = now;

        sysinfo(&info);

        buffer_put_u8(&rtty->wb, MSG_TYPE_HEARTBEAT);
        buffer_put_u16be(&rtty->wb, 16);
        buffer_put_u32be(&rtty->wb, info.uptime);
        buffer_put_zero(&rtty->wb, 12);  /* pad */
        ev_io_start(loop, &rtty->iow);
    }
}

int rtty_start(struct rtty *rtty)
{
    if (!rtty->devid) {
        log_err("you must specify an id for your device\n");
        return -1;
    }

    if (!valid_id(rtty->devid)) {
        log_err("invalid device id\n");
        return -1;
    }

    if (find_login(login_path, sizeof(login_path) - 1) < 0) {
        log_err("the program 'login' is not found\n");
        return -1;
    }

    ev_timer_init(&rtty->tmr, rtty_timer_cb, 1.0, 1.0);
    ev_timer_start(rtty->loop, &rtty->tmr);

    if (tcp_connect(rtty->loop, rtty->host, rtty->port, on_net_connected, rtty) < 0
            && !rtty->reconnect)
        return -1;

    rtty->file_context.fd = -1;

    INIT_LIST_HEAD(&rtty->web_reqs);

    rtty->active = ev_now(rtty->loop);

    return 0;
}

void rtty_send_msg(struct rtty *rtty, int type, void *data, int len)
{
    struct buffer *wb = &rtty->wb;
    buffer_put_u8(wb, type);
    buffer_put_u16be(wb, len);
    buffer_put_data(wb, data, len);
    ev_io_start(rtty->loop, &rtty->iow);
}
