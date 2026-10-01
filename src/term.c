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

#include "term.h"
#include "utils.h"
#include "log/log.h"

void term_close(struct tty *tty)
{
    struct tty_term *term = container_of(tty, struct tty_term, tty);
    struct ev_loop *loop = tty->rtty->loop;

    ev_timer_stop(loop, &term->tmr);
    ev_child_stop(loop, &term->cw);
    close(term->pty);
    kill(term->pid, SIGTERM);
    file_context_close(&term->file);
}

static void pty_on_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct tty *tty = container_of(w, struct tty, ior);
    struct tty_term *term = container_of(tty, struct tty_term, tty);
    struct rtty *rtty = tty->rtty;
    struct buffer *wb = &rtty->wb;
    static uint8_t buf[4096];
    int len;

    ev_timer_again(loop, &term->tmr);

    len = read(w->fd, buf, sizeof(buf));
    if (len < 0) {
        if (errno != EIO)
            log_err("read from pty failed: %s\n", strerror(errno));
        return;
    }

    if (len == 0)
        return;

    tty_wait_ack(tty, len);

    buffer_put_u8(wb, MSG_TYPE_TERMDATA);
    buffer_put_u16be(wb, 32 + len);
    buffer_put_data(wb, tty->sid, 32);
    buffer_put_data(wb, buf, len);
    ev_io_start(loop, &rtty->iow);
}

static void pty_on_exit(struct ev_loop *loop, struct ev_child *w, int revents)
{
    struct tty_term *tty = container_of(w, struct tty_term, cw);
    struct rtty *rtty = tty->tty.rtty;
    struct buffer *wb = &rtty->wb;

    buffer_put_u8(wb, MSG_TYPE_LOGOUT);
    buffer_put_u16be(wb, 32);
    buffer_put_data(wb, tty->tty.sid, 32);
    ev_io_start(loop, &rtty->iow);

    del_tty(&tty->tty);
}

static void tty_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    struct tty_term *tty = container_of(w, struct tty_term, tmr);

    ev_timer_stop(loop, w);
    kill(tty->pid, SIGTERM);

    log_err("tty(%s) inactive over %ds, now kill it\n", tty->tty.sid, RTTY_TTY_TIMEOUT);
}

int term_open(struct rtty *rtty, const uint8_t *data, size_t len)
{
    struct tty_term *tty = NULL;
    char sid[33] = "";
    int code = 1;
    pid_t pid;
    int pty;

    if (len != 32)
        return -1;

    memcpy(sid, data, 32);

    buffer_put_u8(&rtty->wb, MSG_TYPE_TERM_OPEN);
    buffer_put_u16be(&rtty->wb, 33);
    buffer_put_data(&rtty->wb, sid, 32);

    if (rtty->ntty >= RTTY_MAX_TTY || find_tty(rtty, sid)) {
        log_info("tty login fail, device busy\n");
        goto done;
    }

    if (getuid() != 0) {
        log_err("shell login requires root privileges\n");
        goto done;
    }

    if (find_login(rtty->login_path, sizeof(rtty->login_path) - 1) < 0) {
        log_err("the program 'login' is not found\n");
        goto done;
    }

    tty = calloc(1, sizeof(struct tty_term));
    if (!tty) {
        log_err("calloc: %s\n", strerror(errno));
        goto done;
    }

    pid = forkpty(&pty, NULL, NULL, NULL);
    if (pid < 0) {
        log_err("forkpty: %s\n", strerror(errno));
        goto done;
    }

    if (pid == 0) {
        if (rtty->username)
            execl(rtty->login_path, "login", "-f", rtty->username, NULL);
        else
            execl(rtty->login_path, "login", NULL);

        exit(1);
    }

    tty->pid = pid;
    tty->pty = pty;
    tty->tty.rtty = rtty;
    tty->tty.type = TTY_TERM;

    strcpy(tty->tty.sid, sid);

    rtty->ntty++;
    list_add(&tty->tty.node, &rtty->ttys);

    fcntl(pty, F_SETFL, fcntl(pty, F_GETFL, 0) | O_NONBLOCK);

    ev_io_init(&tty->tty.ior, pty_on_read, pty, EV_READ);
    ev_io_start(rtty->loop, &tty->tty.ior);

    ev_io_init(&tty->tty.iow, tty_on_write, pty, EV_WRITE);

    ev_child_init(&tty->cw, pty_on_exit, pid, 0);
    ev_child_start(rtty->loop, &tty->cw);

    ev_timer_init(&tty->tmr, tty_timer_cb, RTTY_TTY_TIMEOUT, RTTY_TTY_TIMEOUT);
    ev_timer_start(rtty->loop, &tty->tmr);

    file_context_init(&tty->file);

    code = 0;

    log_info("new tty: %d/%d %s\n", rtty->ntty, RTTY_MAX_TTY, sid);

done:
    if (code)
        free(tty);

    buffer_put_u8(&rtty->wb, code);
    ev_io_start(rtty->loop, &rtty->iow);
    return 0;
}

static void write_data_to_tty(struct tty_term *tty, int len)
{
    struct rtty *rtty = tty->tty.rtty;

    ev_timer_again(rtty->loop, &tty->tmr);

    buffer_put_data(&tty->tty.wb, buffer_data(&rtty->rb), len);
    buffer_pull(&rtty->rb, NULL, len);
    ev_io_start(rtty->loop, &tty->tty.iow);
}

static void set_tty_winsize(struct tty_term *tty)
{
    struct rtty *rtty = tty->tty.rtty;
    struct winsize size = {};

    size.ws_col = buffer_pull_u16be(&rtty->rb);
    size.ws_row = buffer_pull_u16be(&rtty->rb);

    if (ioctl(tty->pty, TIOCSWINSZ, &size) < 0)
        log_err("ioctl TIOCSWINSZ failed: %s\n", strerror(errno));
}

int term_handle_session(struct tty *tty, int type, int len)
{
    struct tty_term *term = container_of(tty, struct tty_term, tty);
    struct rtty *rtty = tty->rtty;
    struct buffer *b = &rtty->rb;

    switch (type) {
    case MSG_TYPE_TERMDATA:
        write_data_to_tty(term, len);
        break;
    case MSG_TYPE_WINSIZE:
        set_tty_winsize(term);
        break;
    case MSG_TYPE_FILE:
        parse_file_msg(&term->file, buffer_data(b), len);
        buffer_pull(b, NULL, len);
        break;
    default:
        /* never to here */
        break;
    }

    return 0;
}
