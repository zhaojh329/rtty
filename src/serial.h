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

#ifndef RTTY_SERIAL_H
#define RTTY_SERIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct rtty;
struct tty;

enum {
    SERIAL_OK,
    SERIAL_BUSY,
    SERIAL_NOT_FOUND,
    SERIAL_PERMISSION,
    SERIAL_INVALID_SETTINGS,
    SERIAL_FAILED
};

enum {
    MSG_SERIAL_PORTS_ATTR_NAME
};

int serial_list_ports(struct rtty *rtty, const uint8_t *data, size_t len);
int serial_open(struct rtty *rtty, const uint8_t *data, size_t len);
int serial_handle_session(struct tty *tty, int type, int len);
int serial_logout(struct tty *tty);
void serial_close(struct tty *tty);

#endif
