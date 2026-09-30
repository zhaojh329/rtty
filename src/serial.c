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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>

#include "rtty.h"
#include "serial.h"

#define SERIAL_INPUT_LIMIT (64 * 1024)
#define SERIAL_LIST_LIMIT 65000

struct tty_serial {
    struct tty tty;
    int fd;
    dev_t device;
};

static bool serial_device_busy(struct rtty *rtty, dev_t device)
{
    struct tty *tty;

    list_for_each_entry(tty, &rtty->ttys, node) {
        struct tty_serial *s;

        if (tty->type != TTY_SERIAL)
            continue;

        s = container_of(tty, struct tty_serial, tty);
        if (s->device == device)
            return true;
    }

    return false;
}

static int serial_send(struct rtty *rtty, int type, const char *sid,
                       const void *data, size_t len)
{
    struct buffer *wb = &rtty->wb;

    if (rtty->sock == -1)
        return 0;

    /* Reserve the complete frame before writing its header. */
    if (buffer_tailroom(wb) < 35 + len && buffer_grow(wb, 35 + len) != 0)
        return -1;

    buffer_put_u8(wb, type);
    buffer_put_u16be(wb, 32 + len);
    buffer_put_data(wb, sid, 32);
    if (len)
        buffer_put_data(wb, data, len);

    ev_io_start(rtty->loop, &rtty->iow);
    return 0;
}

void serial_close(struct tty *tty)
{
    struct tty_serial *s = container_of(tty, struct tty_serial, tty);

    ioctl(s->fd, TIOCNXCL, 0);
    close(s->fd);
}

int serial_logout(struct tty *tty)
{
    int ret = serial_send(tty->rtty, MSG_TYPE_LOGOUT, tty->sid, NULL, 0);

    del_tty(tty);
    return ret;
}

static int serial_name_filter(const struct dirent *entry)
{
    static const char *prefixes[] = {
        "ttyS", "ttyHS", "ttyUSB", "ttyACM", "ttyAMA", "rfcomm", "ttyO", "ttymxc"
    };
    size_t i;

    for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        size_t n = strlen(prefixes[i]);
        const char *suffix = entry->d_name + n;
        size_t digits;

        if (strncmp(entry->d_name, prefixes[i], n))
            continue;

        digits = strlen(suffix);
        if (digits < 1 || digits > 3)
            continue;

        if (strspn(suffix, "0123456789") == digits)
            return 1;
    }

    return 0;
}

static int serial_enumerate(struct buffer *payload, const char *wanted, bool *found)
{
    struct dirent **entries;
    int count = scandir("/dev", &entries, serial_name_filter, alphasort);
    int code = SERIAL_OK;
    int i;

    if (count < 0)
        return SERIAL_FAILED;

    for (i = 0; i < count; i++) {
        char path[sizeof(entries[i]->d_name) + 6];
        struct stat st;

        snprintf(path, sizeof(path), "/dev/%s", entries[i]->d_name);
        if (code != SERIAL_OK || stat(path, &st) < 0 || !S_ISCHR(st.st_mode))
            goto next;

        if (wanted && !strcmp(path, wanted))
            *found = true;

        if (payload) {
            size_t len = strlen(path);

            /* The leading status byte is not part of the TLV size limit. */
            if (buffer_length(payload) - 1 + 3 + len > SERIAL_LIST_LIMIT) {
                code = SERIAL_FAILED;
                goto next;
            }

            if (buffer_tailroom(payload) < 3 + len && buffer_grow(payload, 3 + len) != 0) {
                code = SERIAL_FAILED;
                goto next;
            }

            buffer_put_u8(payload, MSG_SERIAL_PORTS_ATTR_NAME);
            buffer_put_u16be(payload, len);
            buffer_put_data(payload, path, len);
        }

next:
        free(entries[i]);
    }

    free(entries);
    return code;
}

int serial_list_ports(struct rtty *rtty, const uint8_t *data, size_t len)
{
    struct buffer payload = {};
    char id[33] = "";
    uint8_t code;
    int ret;

    if (len != 32)
        return -1;

    memcpy(id, data, 32);

    if (buffer_put_u8(&payload, SERIAL_OK) < 0)
        return -1;

    code = serial_enumerate(&payload, NULL, NULL);
    if (code == SERIAL_OK)
        ret = serial_send(rtty, MSG_TYPE_SERIAL_PORTS, id, buffer_data(&payload), buffer_length(&payload));
    else
        ret = serial_send(rtty, MSG_TYPE_SERIAL_PORTS, id, &code, 1);

    buffer_free(&payload);
    return ret;
}

static bool serial_speed(uint32_t baud, speed_t *speed)
{
    static const struct {
        uint32_t baud;
        speed_t speed;
    } rates[] = {
        {300, B300}, {600, B600}, {1200, B1200}, {1800, B1800},
        {2400, B2400}, {4800, B4800}, {9600, B9600}, {19200, B19200},
        {38400, B38400}, {57600, B57600}, {115200, B115200}, {230400, B230400},
#ifdef B460800
        {460800, B460800},
#endif
#ifdef B500000
        {500000, B500000},
#endif
#ifdef B576000
        {576000, B576000},
#endif
#ifdef B921600
        {921600, B921600},
#endif
#ifdef B1000000
        {1000000, B1000000},
#endif
#ifdef B1152000
        {1152000, B1152000},
#endif
#ifdef B1500000
        {1500000, B1500000},
#endif
#ifdef B2000000
        {2000000, B2000000},
#endif
#ifdef B2500000
        {2500000, B2500000},
#endif
#ifdef B3000000
        {3000000, B3000000},
#endif
#ifdef B3500000
        {3500000, B3500000},
#endif
#ifdef B4000000
        {4000000, B4000000},
#endif
    };
    size_t i;

    for (i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
        if (rates[i].baud == baud) {
            *speed = rates[i].speed;
            return true;
        }
    }

    return false;
}

static int serial_error(int error)
{
    switch (error) {
    case EBUSY:
        return SERIAL_BUSY;
    case ENOENT:
    case ENODEV:
    case ENXIO:
        return SERIAL_NOT_FOUND;
    case EACCES:
    case EPERM:
        return SERIAL_PERMISSION;
    case EINVAL:
    case ENOTTY:
        return SERIAL_INVALID_SETTINGS;
    default:
        return SERIAL_FAILED;
    }
}

static void serial_on_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct tty *tty = container_of(w, struct tty, ior);
    struct tty_serial *s = container_of(tty, struct tty_serial, tty);
    uint8_t data[4096];
    ssize_t len = read(s->fd, data, sizeof(data));

    if (len < 0 && (errno == EINTR))
        return;

    if (len <= 0) {
        struct rtty *rtty = s->tty.rtty;

        if (serial_logout(&s->tty) < 0)
            rtty_exit(rtty);
        return;
    }

    if (serial_send(s->tty.rtty, MSG_TYPE_TERMDATA, s->tty.sid, data, len) < 0) {
        rtty_exit(s->tty.rtty);
        return;
    }

    tty_wait_ack(tty, len);
}

int serial_open(struct rtty *rtty, const uint8_t *data, size_t len)
{
    int code = SERIAL_INVALID_SETTINGS;
    struct termios settings;
    struct tty_serial *s;
    struct stat st;
    char path[257];
    char sid[33] = "";
    uint32_t baud;
    speed_t speed;
    bool found = false;
    bool exclusive = false;
    int fd = -1;
    int ret;

    if (len < 32)
        return -1;

    memcpy(sid, data, 32);
    data += 32;
    len -= 32;

    if (len < 8 || len > 263)
        goto reply;

    memcpy(&baud, data, sizeof(baud));
    baud = be32toh(baud);

    if (!serial_speed(baud, &speed) || data[4] < 5 || data[4] > 8 ||
        (data[5] != 1 && data[5] != 2) || data[6] > 2 || memchr(data + 7, 0, len - 7))
        goto reply;

    memcpy(path, data + 7, len - 7);
    path[len - 7] = '\0';

    code = SERIAL_BUSY;
    if (find_tty(rtty, sid) || rtty->ntty >= RTTY_MAX_TTY)
        goto reply;

    code = serial_enumerate(NULL, path, &found);
    if (code != SERIAL_OK)
        goto reply;

    code = SERIAL_NOT_FOUND;
    if (!found)
        goto reply;

    if (stat(path, &st) < 0)
        goto error;

    if (serial_device_busy(rtty, st.st_rdev)) {
        code = SERIAL_BUSY;
        goto reply;
    }

    fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0 || fstat(fd, &st) < 0)
        goto error;

    code = SERIAL_NOT_FOUND;
    if (!S_ISCHR(st.st_mode))
        goto reply;

    if (serial_device_busy(rtty, st.st_rdev)) {
        code = SERIAL_BUSY;
        goto reply;
    }

    if (ioctl(fd, TIOCEXCL, 0) < 0)
        goto error;
    exclusive = true;

    if (tcgetattr(fd, &settings) < 0)
        goto error;

    cfmakeraw(&settings);
    settings.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD | CRTSCTS);
#ifdef CMSPAR
    settings.c_cflag &= ~CMSPAR;
#endif
    settings.c_cflag |= CLOCAL | CREAD;

    switch (data[4]) {
    case 5: settings.c_cflag |= CS5; break;
    case 6: settings.c_cflag |= CS6; break;
    case 7: settings.c_cflag |= CS7; break;
    case 8: settings.c_cflag |= CS8; break;
    }

    if (data[5] == 2)
        settings.c_cflag |= CSTOPB;
    if (data[6])
        settings.c_cflag |= PARENB;
    if (data[6] == 1)
        settings.c_cflag |= PARODD;

    settings.c_iflag &= ~(IXON | IXOFF | IXANY | INPCK | IGNPAR);
#ifdef IUCLC
    settings.c_iflag &= ~IUCLC;
#endif
    settings.c_cc[VMIN] = 1;
    settings.c_cc[VTIME] = 0;

    if (cfsetispeed(&settings, speed) < 0 || cfsetospeed(&settings, speed) < 0 ||
        tcsetattr(fd, TCSANOW, &settings) < 0)
        goto error;

    code = SERIAL_FAILED;
    s = calloc(1, sizeof(*s));
    if (!s)
        goto reply;

    s->tty.rtty = rtty;
    s->tty.type = TTY_SERIAL;
    s->fd = fd;
    s->device = st.st_rdev;

    memcpy(s->tty.sid, sid, 32);

    ev_io_init(&s->tty.ior, serial_on_read, fd, EV_READ);
    ev_io_init(&s->tty.iow, tty_on_write, fd, EV_WRITE);

    list_add(&s->tty.node, &rtty->ttys);

    rtty->ntty++;

    code = SERIAL_OK;

    ret = serial_send(rtty, MSG_TYPE_SERIAL_OPEN, sid, &code, 1);
    if (ret < 0) {
        del_tty(&s->tty);
        return ret;
    }

    ev_io_start(rtty->loop, &s->tty.ior);
    return 0;

error:
    code = serial_error(errno);
reply:
    if (fd >= 0) {
        if (exclusive)
            ioctl(fd, TIOCNXCL, 0);
        close(fd);
    }

    return serial_send(rtty, MSG_TYPE_SERIAL_OPEN, sid, &code, 1);
}

static int write_data_to_serial(struct tty_serial *s, int len)
{
    struct rtty *rtty = s->tty.rtty;
    struct buffer *input = &s->tty.wb;

    if (!len)
        return 0;

    if (len > SERIAL_INPUT_LIMIT - buffer_length(input))
        return serial_logout(&s->tty);

    if (buffer_tailroom(input) < len &&
        buffer_resize(input, buffer_length(input) + len) != 0)
        return serial_logout(&s->tty);

    if (!buffer_put_data(input, buffer_data(&rtty->rb), len))
        return serial_logout(&s->tty);

    ev_io_start(rtty->loop, &s->tty.iow);
    return 0;
}

int serial_handle_session(struct tty *tty, int type, int len)
{
    struct tty_serial *s = container_of(tty, struct tty_serial, tty);
    struct rtty *rtty = tty->rtty;
    struct buffer *b = &rtty->rb;
    int ret = 0;

    switch (type) {
    case MSG_TYPE_TERMDATA:
        ret = write_data_to_serial(s, len);
        break;
    default:
        /* Window changes and file messages do not apply to serial sessions. */
        break;
    }

    buffer_pull(b, NULL, len);
    return ret;
}
