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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <glob.h>

#include "log/log.h"
#include "rtty.h"

enum {
    LONG_OPT_HELP = 1
};

#ifdef SSL_SUPPORT
static void load_default_ca_cert(struct ssl_context *ctx)
{
	glob_t gl;
	size_t i;

	glob("/etc/ssl/certs/*.crt", 0, NULL, &gl);

	for (i = 0; i < gl.gl_pathc; i++)
		ssl_load_ca_cert_file(ctx, gl.gl_pathv[i]);

	globfree(&gl);
}
#endif

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
    if (w->signum == SIGINT) {
        ev_break(loop, EVBREAK_ALL);
        log_info("Normal quit\n");
    }
}

static struct option long_options[] = {
    {"id",          required_argument, NULL, 'I'},
    {"host",        required_argument, NULL, 'h'},
    {"port",        required_argument, NULL, 'p'},
    {"description", required_argument, NULL, 'd'},
    {"token",       required_argument, NULL, 't'},
#ifdef SSL_SUPPORT
    {"cacert",      required_argument, NULL, 'C'},
    {"insecure",    no_argument, NULL, 'x'},
    {"cert",        required_argument, NULL, 'c'},
    {"key",         required_argument, NULL, 'k'},
#endif
    {"verbose",     no_argument,       NULL, 'v'},
    {"version",     no_argument,       NULL, 'V'},
    {"help",        no_argument,       NULL, LONG_OPT_HELP},
    {0, 0, 0, 0}
};

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [option]\n"
            "      -I, --id=string          Set an ID for the device(Maximum 63 bytes, valid\n"
            "                               character:letter, number, underline and short line)\n"
            "      -h, --host=string        Server's host or ipaddr(Default is localhost)\n"
            "      -p, --port=number        Server port(Default is 5912)\n"
            "      -d, --description=string Add a description to the device(Maximum 126 bytes)\n"
            "      -a                       Auto reconnect to the server\n"
#ifdef SSL_SUPPORT
            "      -s                       SSL on\n"
            "      -C, --cacert             CA certificate to verify peer against\n"
            "      -x, --insecure           Allow insecure server connections when using SSL\n"
            "      -c, --cert               Certificate file to use\n"
            "      -k, --key                Private key file to use\n"
#endif
            "      -D                       Run in the background\n"
            "      -t, --token=string       Authorization token\n"
            "      -f username              Skip a second login authentication. See man login(1) about the details\n"
            "      -R                       Receive file\n"
            "      -S file                  Send file\n"
            "      -v, --verbose            verbose\n"
            "      -V, --version            Show version\n"
            "      --help                   Show usage\n",
            prog);
    exit(1);
}

int main(int argc, char **argv)
{
    char shortopts[32] = "I:h:p:d:aDt:f:RS:vV";
    struct ev_loop *loop = EV_DEFAULT;
    struct ev_signal signal_watcher;
    bool background = false;
    bool verbose = false;
    struct rtty rtty = {
        .host = "localhost",
        .port = 5912,
        .loop = loop,
        .sock = -1
    };
#ifdef SSL_SUPPORT
    bool has_cacert = false;
#endif
    int option_index;
    int c;

    log_level(LOG_DEBUG);

#ifdef SSL_SUPPORT
    rtty.ssl_ctx = ssl_context_new(false);
    if (!rtty.ssl_ctx)
        return -1;

    strcat(shortopts, "sC:xc:k:");
#endif

    while (true) {
        c = getopt_long(argc, argv, shortopts, long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'I':
            rtty.devid = optarg;
            break;
        case 'h':
            rtty.host = optarg;
            break;
        case 'p':
            rtty.port = atoi(optarg);
            break;
        case 'd':
            if (strlen(optarg) > 126) {
                log_err("Description too long\n");
                usage(argv[0]);
            }
            rtty.description = optarg;
            break;
        case 'a':
            rtty.reconnect = true;
            break;
#ifdef SSL_SUPPORT
        case 's':
            rtty.ssl_on = true;
            break;
        case 'C':
            if (ssl_load_ca_cert_file(rtty.ssl_ctx, optarg)) {
                log_err("load ca certificate file fail\n");
                return -1;
            }
            has_cacert = true;
            break;
        case 'x':
            rtty.insecure = true;
            ssl_set_require_validation(rtty.ssl_ctx, false);
            break;
        case 'c':
            if (ssl_load_cert_file(rtty.ssl_ctx, optarg)) {
                log_err("load certificate file fail\n");
                return -1;
            }
            break;
        case 'k':
            if (ssl_load_key_file(rtty.ssl_ctx, optarg)) {
                log_err("load private key file fail\n");
                return -1;
            }
            break;
#endif
        case 'D':
            background = true;
            break;
        case 't':
            rtty.token = optarg;
            break;
        case 'f':
            rtty.username = optarg;
            break;
        case 'R':
            request_transfer_file('R', NULL);
            return 0;
        case 'S':
            request_transfer_file('S', optarg);
            return 0;
        case 'v':
            verbose = true;
            break;
        case 'V':
            log_info("rtty version %s\n", RTTY_VERSION_STRING);
            exit(0);
        case LONG_OPT_HELP:
            usage(argv[0]);
            break;
        default: /* '?' */
            usage(argv[0]);
            break;
        }
    }

    signal(SIGPIPE, SIG_IGN);

    if (background && daemon(0, 0))
        log_err("Can't run in the background: %s\n", strerror(errno));

    if (verbose)
        log_level(LOG_DEBUG);
    else
        log_level(LOG_ERR);

    log_info("rtty version %s\n", RTTY_VERSION_STRING);

    if (getuid() > 0) {
        log_err("Operation not permitted, must be run as root\n");
        return -1;
    }

    ev_signal_init(&signal_watcher, signal_cb, SIGINT);
    ev_signal_start(loop, &signal_watcher);

#ifdef SSL_SUPPORT
    if (rtty.ssl_ctx && !has_cacert)
        load_default_ca_cert(rtty.ssl_ctx);
#endif

    if (rtty_start(&rtty) < 0)
        return -1;

    ev_run(loop, 0);

    rtty_exit(&rtty);

#ifdef SSL_SUPPORT
    ssl_context_free(rtty.ssl_ctx);
#endif

    ev_loop_destroy(loop);

    return 0;
}
