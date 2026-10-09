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
#include <errno.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

#include "net.h"
#include "http.h"
#include "tcp.h"
#include "rtty.h"
#include "serial.h"
#include "term.h"
#include "list.h"
#include "command.h"
#include "utils.h"
#include "log/log.h"

void del_tty(struct tty *tty)
{
    struct rtty *rtty = tty->rtty;

    ev_io_stop(rtty->loop, &tty->ior);
    ev_io_stop(rtty->loop, &tty->iow);
    buffer_free(&tty->wb);
    rtty->ntty--;
    list_del(&tty->node);

    if (tty->type == TTY_SERIAL) {
        serial_close(tty);
    } else {
        term_close(tty);
    }

    log_info("delete tty: %s\n", tty->sid);

    free(tty);
}

struct tty *find_tty(struct rtty *rtty, const char *sid)
{
    struct tty *tty;

    list_for_each_entry(tty, &rtty->ttys, node) {
        if (!strcmp(tty->sid, sid))
            return tty;
    }

    return NULL;
}

void tty_on_write(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct tty *tty = container_of(w, struct tty, iow);
    int ret;

    ret = buffer_pull_to_fd(&tty->wb, w->fd, buffer_length(&tty->wb));
    if (ret < 0) {
        if (tty->type == TTY_SERIAL) {
            struct rtty *rtty = tty->rtty;

            if (serial_logout(tty) < 0)
                rtty_exit(rtty);
        } else {
            log_err("write to pty failed: %s\n", strerror(errno));
        }
        return;
    }

    if (!buffer_length(&tty->wb))
        ev_io_stop(loop, w);
}

void tty_wait_ack(struct tty *tty, uint32_t len)
{
    tty->wait_ack += len;
    if (tty->wait_ack > RTTY_TTY_ACK_BLOCK)
        ev_io_stop(tty->rtty->loop, &tty->ior);
}

void tty_ack(struct tty *tty, uint16_t ack)
{
    tty->wait_ack = ack >= tty->wait_ack ? 0 : tty->wait_ack - ack;
    if (tty->wait_ack <= RTTY_TTY_ACK_BLOCK)
        ev_io_start(tty->rtty->loop, &tty->ior);
}

static void rtty_run_state(int state)
{
    static int st = RTTY_STATE_DISCONNECTED;
    const char *file = "/var/run/rtty";
    const char *str_state;
    FILE *fp;

    if (geteuid() != 0)
        return;

    if (st == state)
        return;

    st = state;

    fp = fopen(file, "w+");
    if (!fp) {
        log_err("Cannot create state %s: %s", file, strerror(errno));
        return;
    }

    switch(st) {
    case RTTY_STATE_CONNECTED:
        str_state = "Connected\n";
        break;
    default:
        str_state = "Disconnected\n";
        break;
    }

    fputs(str_state, fp);
    fclose(fp);
}

void rtty_exit(struct rtty *rtty)
{
    struct tty *tty, *ntty;

    if (rtty->sock < 0)
        goto done;

    ev_io_stop(rtty->loop, &rtty->ior);
    ev_io_stop(rtty->loop, &rtty->iow);
    ev_timer_stop(rtty->loop, &rtty->tmr);

    buffer_free(&rtty->rb);
    buffer_free(&rtty->wb);

    list_for_each_entry_safe(tty, ntty, &rtty->ttys, node) {
        del_tty(tty);
    }

#ifdef SSL_SUPPORT
    if (rtty->ssl) {
        ssl_session_free(rtty->ssl);
        rtty->ssl_negotiated = false;
    }
#endif

    close(rtty->sock);
    rtty->sock = -1;
    rtty->registered = false;

    http_conns_free(&rtty->http_conns);
    tcp_conns_free(rtty);

    rtty_run_state(RTTY_STATE_DISCONNECTED);

done:
    if (!rtty->reconnect) {
        ev_break(rtty->loop, EVBREAK_ALL);
    } else {
        int delay = rand() % 10 + 5;
        ev_timer_set(&rtty->tmr, delay, 0);
        ev_timer_start(rtty->loop, &rtty->tmr);
        log_err("reconnect in %d seconds\n", delay);
    }
}

static size_t rtty_put_attr(struct buffer *b, int type, const void *data, size_t len)
{
    buffer_put_u8(b, type);
    buffer_put_u16be(b, len);
    buffer_put_data(b, data, len);
    return 3 + len;
}

static size_t rtty_put_attr_u8(struct buffer *b, int type, uint8_t val)
{
    return rtty_put_attr(b, type, &val, 1);
}

static size_t rtty_put_attr_u32be(struct buffer *b, int type, uint32_t val)
{
    val = htobe32(val);
    return rtty_put_attr(b, type, &val, 4);
}

static size_t rtty_put_attr_str(struct buffer *b, int type, const char *s)
{
    return rtty_put_attr(b, type, s, strlen(s));
}

static void rtty_register(struct rtty *rtty)
{
    struct buffer *wb = &rtty->wb;
    uint8_t *len_ptr;
    uint16_t be_len;
    size_t len = 0;

    buffer_put_u8(wb, MSG_TYPE_REGISTER);
    len_ptr = buffer_put(wb, 2);

    len += buffer_put_u8(wb, RTTY_PROTO_VER);

    len += rtty_put_attr_u8(wb, MSG_REG_ATTR_HEARTBEAT, rtty->heartbeat);
    len += rtty_put_attr_str(wb, MSG_REG_ATTR_DEVID, rtty->devid);

    if (rtty->group)
        len += rtty_put_attr_str(wb, MSG_REG_ATTR_GROUP, rtty->group);

    if (rtty->description)
        len += rtty_put_attr_str(wb, MSG_REG_ATTR_DESCRIPTION, rtty->description);

    if (rtty->token)
        len += rtty_put_attr_str(wb, MSG_REG_ATTR_TOKEN, rtty->token);

    /* The length field is not 2-byte aligned in the buffer */
    be_len = htobe16(len);
    memcpy(len_ptr, &be_len, sizeof(be_len));

    ev_io_start(rtty->loop, &rtty->iow);

    ev_timer_set(&rtty->tmr, 5.0, 0);
    ev_timer_start(rtty->loop, &rtty->tmr);

    log_debug("send msg: register\n");
}

static int parse_tty_msg(struct rtty *rtty, int type, int len)
{
    struct buffer *b = &rtty->rb;
    struct tty *tty;
    char sid[33] = "";

    if (len < 32)
        return -1;

    buffer_pull(b, sid, 32);
    len -= 32;

    tty = find_tty(rtty, sid);
    if (!tty) {
        log_err("non-existent sid: %s\n", sid);
        buffer_pull(b, NULL, len);
        return 0;
    }

    switch (type) {
    case MSG_TYPE_LOGOUT:
        del_tty(tty);
        return 0;
    case MSG_TYPE_ACK:
        if (len != 2)
            return -1;

        tty_ack(tty, buffer_pull_u16be(b));
        return 0;
    default:
        break;
    }

    if (tty->type == TTY_SERIAL) {
        return serial_handle_session(tty, type, len);
    } else {
        return term_handle_session(tty, type, len);
    }
}

static const char *msg_type_name(int type)
{
    switch (type) {
    case MSG_TYPE_REGISTER:
        return "register";
    case MSG_TYPE_TERM_OPEN:
        return "termopen";
    case MSG_TYPE_LOGOUT:
        return "logout";
    case MSG_TYPE_TERMDATA:
        return "termdata";
    case MSG_TYPE_WINSIZE:
        return "winsize";
    case MSG_TYPE_CMD:
        return "cmd";
    case MSG_TYPE_HEARTBEAT:
        return "heartbeat";
    case MSG_TYPE_FILE:
        return "file";
    case MSG_TYPE_HTTP:
        return "http";
    case MSG_TYPE_ACK:
        return "ack";
    case MSG_TYPE_SERIAL_PORTS:
        return "serialports";
    case MSG_TYPE_SERIAL_OPEN:
        return "serialopen";
    case MSG_TYPE_TCP:
        return "tcp";
    default:
        return "unknown";
    }
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

        log_debug("recv msg: %s\n", msg_type_name(msgtype));

        rtty->wait_heartbeat = false;

        switch (msgtype) {
        case MSG_TYPE_REGISTER:
            if (msglen < 1)
                return -1;

            if (buffer_pull_u8(rb)) {
                char errs[128] = "";
                int len = msglen - 1;

                /* Truncate the error message to fit, and drop the rest */
                if (len > (int)sizeof(errs) - 1)
                    len = sizeof(errs) - 1;

                buffer_pull(rb, errs, len);
                buffer_pull(rb, NULL, msglen - 1 - len);
                log_err("register fail: %s\n", errs);
                return -1;
            }
            buffer_pull(rb, NULL, msglen - 1);
            log_info("register success\n");
            rtty_run_state(RTTY_STATE_CONNECTED);
            rtty->registered = true;
            rtty->last_heartbeat = 0;
            ev_timer_stop(rtty->loop, &rtty->tmr);
            ev_timer_set(&rtty->tmr, rtty->heartbeat, 0);
            ev_timer_start(rtty->loop, &rtty->tmr);
            break;

        case MSG_TYPE_LOGOUT:
        case MSG_TYPE_TERMDATA:
        case MSG_TYPE_WINSIZE:
        case MSG_TYPE_FILE:
        case MSG_TYPE_ACK:
            if (parse_tty_msg(rtty, msgtype, msglen) < 0)
                return -1;
            break;

        case MSG_TYPE_SERIAL_PORTS:
            if (serial_list_ports(rtty, buffer_data(rb), msglen) < 0)
                return -1;
            buffer_pull(rb, NULL, msglen);
            break;

        case MSG_TYPE_TERM_OPEN:
            if (term_open(rtty, buffer_data(rb), msglen) < 0)
                return -1;
            buffer_pull(rb, NULL, msglen);
            break;

        case MSG_TYPE_SERIAL_OPEN:
            if (serial_open(rtty, buffer_data(rb), msglen) < 0)
                return -1;
            buffer_pull(rb, NULL, msglen);
            break;

        case MSG_TYPE_TCP:
            if (tcp_handle_msg(rtty, buffer_data(rb), msglen) < 0)
                return -1;
            buffer_pull(rb, NULL, msglen);
            break;

        case MSG_TYPE_CMD:
            if (run_command(rtty, buffer_data(rb), msglen) < 0)
                return -1;
            buffer_pull(rb, NULL, msglen);
            break;

        case MSG_TYPE_HEARTBEAT:
            break;

        case MSG_TYPE_HTTP:
            http_request(rtty, msglen);
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
    bool *valid_cert = arg;

    *valid_cert = false;

    log_warn("SSL certificate error(%d): %s\n", error, str);
}

/* -1 error, 0 pending, 1 ok */
static int ssl_negotiated(struct rtty *rtty)
{
    bool valid_cert = true;
    char err_buf[128];
    int ret;

    ret = ssl_connect(rtty->ssl, on_ssl_verify_error, &valid_cert);
    if (ret == SSL_WANT_READ)
        return 0;

    if (ret == SSL_WANT_WRITE) {
        ev_io_start(rtty->loop, &rtty->iow);
        return 0;
    }

    if (ret == SSL_ERROR) {
        log_err("ssl connect error: %s\n", ssl_last_error_string(rtty->ssl, err_buf, sizeof(err_buf)));
        return -1;
    }

    if (!valid_cert && !rtty->insecure)
        return -1;

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
        log_err("ssl_read: %s\n", ssl_last_error_string(rtty->ssl, err_buf, sizeof(err_buf)));
        return P_FD_ERR;
    }

    if (ret == SSL_WANT_READ || ret == SSL_WANT_WRITE)
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

        ret = buffer_put_fd_ex(&rtty->rb, w->fd, -1, &eof, rtty_ssl_read, rtty);
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
            log_err("ssl_write: %s\n", ssl_last_error_string(rtty->ssl, err_buf, sizeof(err_buf)));
            goto err;
        }

        if (ret == SSL_WANT_READ || ret == SSL_WANT_WRITE)
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

    tcp_update_readers(rtty);

    return;

err:
    rtty_exit(rtty);
}

static void on_net_connected(int sock, void *arg)
{
    struct rtty *rtty = arg;

    if (sock < 0) {
        rtty_exit(rtty);
        return;
    }

    log_info("connected to server\n");

    rtty->sock = sock;

    ev_io_init(&rtty->ior, on_net_read, sock, EV_READ);
    ev_io_start(rtty->loop, &rtty->ior);

    ev_io_init(&rtty->iow, on_net_write, sock, EV_WRITE);

    if (rtty->ssl_on) {
#ifdef SSL_SUPPORT
        rtty->ssl = ssl_session_new(rtty->ssl_ctx, sock);
        if (!rtty->ssl) {
            log_err("SSL session create fail\n");
            ev_break(rtty->loop, EVBREAK_ALL);
            return;
        }
        ssl_set_server_name(rtty->ssl, rtty->host);
#endif
    }

    rtty_register(rtty);
}

static void rtty_send_heartbeat(struct rtty *rtty)
{
    struct buffer *wb = &rtty->wb;
    struct sysinfo info = {};
    uint8_t *len_ptr;
    uint16_t be_len;
    size_t len = 0;

    sysinfo(&info);

    buffer_put_u8(wb, MSG_TYPE_HEARTBEAT);

    len_ptr = buffer_put(wb, 2);

    len += rtty_put_attr_u32be(wb, MSG_HEARTBEAT_ATTR_UPTIME, info.uptime);

    /* The length field is not 2-byte aligned in the buffer */
    be_len = htobe16(len);
    memcpy(len_ptr, &be_len, sizeof(be_len));

    ev_io_start(rtty->loop, &rtty->iow);

    rtty->wait_heartbeat = true;
    rtty->last_heartbeat = monotonic_time();

    log_debug("send msg: heartbeat\n");
}

static void rtty_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    struct rtty *rtty = container_of(w, struct rtty, tmr);

    if (rtty->sock < 0) {
        tcp_connect(rtty->loop, rtty->host, rtty->port, on_net_connected, rtty);
        return;
    }

    if (!rtty->registered) {
        log_err("rtty register timeout\n");
        rtty_exit(rtty);
        return;
    }

    if (rtty->wait_heartbeat) {
        log_err("heartbeat timeout\n");
        rtty_exit(rtty);
        return;
    }

    /*
     * ev_now() follows the system time, so a time correction made this
     * negative or huge. A last_heartbeat of 0 means that no heartbeat was
     * sent yet.
     */
    double elapsed = monotonic_time() - rtty->last_heartbeat;

    if (rtty->last_heartbeat && elapsed < rtty->heartbeat) {
        ev_timer_set(&rtty->tmr, rtty->heartbeat - elapsed, 0);
    } else {
        rtty_send_heartbeat(rtty);
        ev_timer_set(&rtty->tmr, RTTY_HEARTBEAT_TIMEOUT, 0);
    }

    ev_timer_start(rtty->loop, &rtty->tmr);
}

int rtty_start(struct rtty *rtty)
{
    rtty_run_state(RTTY_STATE_DISCONNECTED);

    ev_init(&rtty->tmr, rtty_timer_cb);

    INIT_LIST_HEAD(&rtty->ttys);
    INIT_LIST_HEAD(&rtty->http_conns);
    INIT_LIST_HEAD(&rtty->tcp_conns);

    if (tcp_connect(rtty->loop, rtty->host, rtty->port, on_net_connected, rtty) < 0
            && !rtty->reconnect)
        return -1;

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
