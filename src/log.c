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

#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "log.h"

static int log_threshold = LOG_DEBUG;
static bool log_initialized;
static const char *ident;

void (*log_write)(int priority, const char *fmt, va_list ap);

static const char *log_ident()
{
    FILE *self;
    static char line[64];
    char *p = NULL;
    char *sbuf;

    if ((self = fopen("/proc/self/status", "r")) != NULL) {
        while (fgets(line, sizeof(line), self)) {
            if (!strncmp(line, "Name:", 5)) {
                strtok_r(line, "\t\n", &sbuf);
                p = strtok_r(NULL, "\t\n", &sbuf);
                break;
            }
        }
        fclose(self);
    }

    return p;
}

static inline void log_write_stdout(int priority, const char *fmt, va_list ap)
{
    time_t now;
    struct tm tm;
    char buf[32];

    now = time(NULL);
    localtime_r(&now, &tm);
    strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S", &tm);

    fprintf(stderr, "%s ", buf);
    vfprintf(stderr, fmt, ap);
}

static inline void log_write_syslog(int priority, const char *fmt, va_list ap)
{
    vsyslog(priority, fmt, ap);
}

static inline void log_init()
{
    if (log_initialized)
        return;

    ident = log_ident();

    if (isatty(STDOUT_FILENO)) {
        log_write = log_write_stdout;
    } else {
        log_write = log_write_syslog;

        openlog(ident, 0, LOG_DAEMON);
    }

    log_initialized = true;
}


void set_log_threshold(int threshold)
{
    log_threshold = threshold;
}

void log_close()
{
    if (!log_initialized)
        return;

    closelog();

    log_initialized = 0;
}

void __ilog(const char *filename, int line, int priority, const char *fmt, ...)
{
    static char new_fmt[256];
    va_list ap;

    if (priority > log_threshold)
        return;

    log_init();

    snprintf(new_fmt, sizeof(new_fmt), "(%s:%d) %s", filename, line, fmt);

    va_start(ap, fmt);
    log_write(priority, new_fmt, ap);
    va_end(ap);
}
