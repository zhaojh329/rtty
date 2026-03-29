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
#include <time.h>
#include <glob.h>
#include <ini.h>

#include "log/log.h"
#include "utils.h"
#include "rtty.h"

enum {
    LONG_OPT_CONFIG = 1,
    LONG_OPT_HELP,
    LONG_OPT_HTTP_TIMEOUT
};

struct config {
    char *group;
    char *devid;
    char *host;
    char *description;
    char *token;
    char *username;
    int port;
    int heartbeat;
    int http_timeout;
    bool has_port;
    bool has_heartbeat;
    bool has_http_timeout;
    bool reconnect;
    bool has_reconnect;
    bool verbose;
    bool has_verbose;
    bool background;
    bool has_background;
#ifdef SSL_SUPPORT
    bool ssl_on;
    bool has_ssl_on;
    bool insecure;
    bool has_insecure;
    char *cacert;
    char *cert;
    char *key;
#endif
};

static bool parse_bool(const char *s, bool *val)
{
    if (!strcasecmp(s, "1") || !strcasecmp(s, "true") ||
        !strcasecmp(s, "yes") || !strcasecmp(s, "on")) {
        *val = true;
        return true;
    }

    if (!strcasecmp(s, "0") || !strcasecmp(s, "false") ||
        !strcasecmp(s, "no") || !strcasecmp(s, "off")) {
        *val = false;
        return true;
    }

    return false;
}

static bool parse_int(const char *s, int *val)
{
    long v;
    char *end;

    errno = 0;
    v = strtol(s, &end, 10);
    if (errno || *end != '\0' || v < INT_MIN || v > INT_MAX)
        return false;

    *val = (int)v;
    return true;
}

static bool replace_string(char **dst, const char *src)
{
    char *tmp = strdup(src);
    if (!tmp)
        return false;

    free(*dst);
    *dst = tmp;
    return true;
}

static void free_config(struct config *cfg)
{
    free(cfg->group);
    free(cfg->devid);
    free(cfg->host);
    free(cfg->description);
    free(cfg->token);
    free(cfg->username);
#ifdef SSL_SUPPORT
    free(cfg->cacert);
    free(cfg->cert);
    free(cfg->key);
#endif
}

static int apply_cfg_pair(struct config *cfg, const char *section,
                          const char *key, const char *value)
{
    int n;
    bool b;

    if (!strcmp(section, "rtty")) {
        if (!strcmp(key, "group"))
            return replace_string(&cfg->group, value) ? 0 : -1;

        if (!strcmp(key, "id"))
            return replace_string(&cfg->devid, value) ? 0 : -1;

        if (!strcmp(key, "host"))
            return replace_string(&cfg->host, value) ? 0 : -1;

        if (!strcmp(key, "port")) {
            if (!parse_int(value, &n))
                goto bad_value;
            cfg->port = n;
            cfg->has_port = true;
            return 0;
        }

        if (!strcmp(key, "description"))
            return replace_string(&cfg->description, value) ? 0 : -1;

        if (!strcmp(key, "token"))
            return replace_string(&cfg->token, value) ? 0 : -1;

        if (!strcmp(key, "username"))
            return replace_string(&cfg->username, value) ? 0 : -1;

        if (!strcmp(key, "heartbeat")) {
            if (!parse_int(value, &n))
                goto bad_value;
            cfg->heartbeat = n;
            cfg->has_heartbeat = true;
            return 0;
        }

        if (!strcmp(key, "http-timeout")) {
            if (!parse_int(value, &n))
                goto bad_value;
            cfg->http_timeout = n;
            cfg->has_http_timeout = true;
            return 0;
        }

        if (!strcmp(key, "reconnect")) {
            if (!parse_bool(value, &b))
                goto bad_value;
            cfg->reconnect = b;
            cfg->has_reconnect = true;
            return 0;
        }

        if (!strcmp(key, "verbose")) {
            if (!parse_bool(value, &b))
                goto bad_value;
            cfg->verbose = b;
            cfg->has_verbose = true;
            return 0;
        }

        if (!strcmp(key, "background")) {
            if (!parse_bool(value, &b))
                goto bad_value;
            cfg->background = b;
            cfg->has_background = true;
            return 0;
        }
    }

#ifdef SSL_SUPPORT
    if (!strcmp(section, "ssl")) {
        if (!strcmp(key, "enabled")) {
            if (!parse_bool(value, &b))
                goto bad_value;
            cfg->ssl_on = b;
            cfg->has_ssl_on = true;
            return 0;
        }

        if (!strcmp(key, "insecure")) {
            if (!parse_bool(value, &b))
                goto bad_value;
            cfg->insecure = b;
            cfg->has_insecure = true;
            return 0;
        }

        if (!strcmp(key, "cacert"))
            return replace_string(&cfg->cacert, value) ? 0 : -1;

        if (!strcmp(key, "cert"))
            return replace_string(&cfg->cert, value) ? 0 : -1;

        if (!strcmp(key, "key"))
            return replace_string(&cfg->key, value) ? 0 : -1;
    }
#endif

    log_warn("unknown config key '%s' in section '%s'\n", key, section);
    return 0;

bad_value:
    log_err("invalid value '%s' for key '%s' in section '%s'\n", value, key, section);
    return -1;
}

static int inih_handler(void *user, const char *section, const char *name,
                        const char *value)
{
    struct config *cfg = user;

    if (apply_cfg_pair(cfg, section ? section : "", name, value) < 0)
        return 0;

    return 1;
}

static int load_config_file(const char *path, struct config *cfg)
{
    int ret = ini_parse(path, inih_handler, cfg);

    if (ret == -1) {
        log_err("open config file '%s' fail: %s\n", path, strerror(errno));
        return -1;
    }

    if (ret > 0) {
        log_err("parse config file '%s' fail at line %d\n", path, ret);
        return -1;
    }

    return 0;
}

static void clamp_heartbeat(int *heartbeat)
{
    if (*heartbeat < 5) {
        *heartbeat = 5;
        log_warn("Heartbeat interval too short, set to 5s\n");
    }

    if (*heartbeat > 255) {
        *heartbeat = 255;
        log_warn("Heartbeat interval too long, set to 255s\n");
    }
}

static void clamp_http_timeout(int *http_timeout)
{
    if (*http_timeout < 5) {
        *http_timeout = 5;
        log_warn("HTTP timeout too short, set to 5s\n");
    }

    if (*http_timeout > 255) {
        *http_timeout = 255;
        log_warn("HTTP timeout too long, set to 255s\n");
    }
}

static int find_config_path(int argc, char **argv, const char **path)
{
    int i;

    *path = NULL;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--conf")) {
            if (i + 1 >= argc) {
                log_err("option '%s' requires an argument\n", argv[i]);
                return -1;
            }
            *path = argv[++i];
            continue;
        }

        if (!strncmp(argv[i], "--conf=", 7)) {
            *path = argv[i] + 7;
            continue;
        }
    }

    return 0;
}

static int apply_config_file(struct rtty *rtty, struct config *cfg)
{
    if (cfg->group) {
        if (!valid_id(cfg->group, 16)) {
            log_err("invalid group in config file\n");
            return -1;
        }
        rtty->group = cfg->group;
    }

    if (cfg->devid) {
        if (!valid_id(cfg->devid, 32)) {
            log_err("invalid device id in config file\n");
            return -1;
        }
        rtty->devid = cfg->devid;
    }

    if (cfg->host)
        rtty->host = cfg->host;

    if (cfg->has_port)
        rtty->port = cfg->port;

    if (cfg->description) {
        if (strlen(cfg->description) > 126) {
            log_err("Description too long in config file\n");
            return -1;
        }
        rtty->description = cfg->description;
    }

    if (cfg->token)
        rtty->token = cfg->token;

    if (cfg->username)
        rtty->username = cfg->username;

    if (cfg->has_reconnect)
        rtty->reconnect = cfg->reconnect;

    if (cfg->has_heartbeat) {
        rtty->heartbeat = cfg->heartbeat;
        clamp_heartbeat(&rtty->heartbeat);
    }

    if (cfg->has_http_timeout) {
        rtty->http_timeout = cfg->http_timeout;
        clamp_http_timeout(&rtty->http_timeout);
    }

#ifdef SSL_SUPPORT
    if (cfg->has_ssl_on)
        rtty->ssl_on = cfg->ssl_on;

    if (cfg->cacert) {
        if (ssl_load_ca_cert_file(rtty->ssl_ctx, cfg->cacert)) {
            log_err("load ca certificate file fail\n");
            return -1;
        }
    }

    if (cfg->has_insecure) {
        rtty->insecure = cfg->insecure;
        ssl_set_require_validation(rtty->ssl_ctx, !cfg->insecure);
    }

    if (cfg->cert) {
        if (ssl_load_cert_file(rtty->ssl_ctx, cfg->cert)) {
            log_err("load certificate file fail\n");
            return -1;
        }
    }

    if (cfg->key) {
        if (ssl_load_key_file(rtty->ssl_ctx, cfg->key)) {
            log_err("load private key file fail\n");
            return -1;
        }
    }
#endif

    return 0;
}

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
    struct rtty *rtty = ev_userdata(loop);

    if (w->signum == SIGINT) {
        rtty->reconnect = false;
        ev_break(loop, EVBREAK_ALL);
        log_info("Normal quit\n");
    }
}

static struct option long_options[] = {
    {"conf",        required_argument, NULL, LONG_OPT_CONFIG},
    {"group",       required_argument, NULL, 'g'},
    {"id",          required_argument, NULL, 'I'},
    {"host",        required_argument, NULL, 'h'},
    {"port",        required_argument, NULL, 'p'},
    {"description", required_argument, NULL, 'd'},
    {"token",       required_argument, NULL, 't'},
    {"http-timeout", required_argument, NULL, LONG_OPT_HTTP_TIMEOUT},
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
            "      --conf=file              Load options from INI config file\n"
            "      -g, --group=string       Set a group for the device(max 16 chars, no spaces allowed)\n"
            "      -I, --id=string          Set an ID for the device(max 32 chars, no spaces allowed)\n"
            "      -h, --host=string        Server's host or ipaddr(Default is localhost)\n"
            "      -p, --port=number        Server port(Default is 5912)\n"
            "      -d, --description=string Add a description to the device(Maximum 126 bytes)\n"
            "      -a                       Auto reconnect to the server\n"
            "      -i number                Set heartbeat interval in seconds(Default is 30s)\n"
            "      --http-timeout=number    Set HTTP idle timeout in seconds(Default is 30s)\n"
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
#ifdef SSL_SUPPORT
#define SSL_SHORTOPTS "sC:xc:k:"
#else
#define SSL_SHORTOPTS ""
#endif

    const char *shortopts = "g:I:i:h:p:d:aDt:f:RS:vV"SSL_SHORTOPTS;
    struct ev_loop *loop = EV_DEFAULT;
    struct ev_signal signal_watcher;
    bool background = false;
    bool verbose = false;
    const char *conf_path;
    struct config cfg = {};
    struct rtty rtty = {
        .heartbeat = 30,
        .http_timeout = 30,
        .host = "localhost",
        .port = 5912,
        .loop = loop,
        .sock = -1
    };
#ifdef SSL_SUPPORT
    bool has_cacert = false;
#endif
    int option_index;
    int ret = 0;

#ifdef SSL_SUPPORT
    rtty.ssl_ctx = ssl_context_new(false);
    if (!rtty.ssl_ctx)
        return -1;
#endif

    ev_set_userdata(loop, &rtty);

    if (find_config_path(argc, argv, &conf_path)) {
        ret = -1;
        goto clean;
    }

    if (conf_path) {
        if (load_config_file(conf_path, &cfg)) {
            ret = -1;
            goto clean;
        }

        if (apply_config_file(&rtty, &cfg)) {
            ret = -1;
            goto clean;
        }

#ifdef SSL_SUPPORT
        has_cacert = cfg.cacert != NULL;
#endif
        verbose = cfg.has_verbose ? cfg.verbose : false;
        background = cfg.has_background ? cfg.background : false;
    }

    while (true) {
        int c = getopt_long(argc, argv, shortopts, long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'g':
            if (!valid_id(optarg, 16)) {
                log_err("invalid group\n");
                ret = -1;
                goto clean;
            }
            rtty.group = optarg;
            break;
        case 'I':
            if (!valid_id(optarg, 32)) {
                log_err("invalid device id\n");
                ret = -1;
                goto clean;
            }
            rtty.devid = optarg;
            break;
        case 'i':
            rtty.heartbeat = atoi(optarg);
            clamp_heartbeat(&rtty.heartbeat);
            break;
        case LONG_OPT_HTTP_TIMEOUT:
            rtty.http_timeout = atoi(optarg);
            clamp_http_timeout(&rtty.http_timeout);
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
                ret = -1;
                goto clean;
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
                ret = -1;
                goto clean;
            }
            break;
        case 'k':
            if (ssl_load_key_file(rtty.ssl_ctx, optarg)) {
                log_err("load private key file fail\n");
                ret = -1;
                goto clean;
            }
            break;
#endif
        case 'D':
            background = true;
            break;
        case LONG_OPT_CONFIG:
             /* already loaded config file, ignore */
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

    if (!rtty.devid) {
        log_err("you must specify an id for your device\n");
        ret = -1;
        goto clean;
    }

    if (find_login(rtty.login_path, sizeof(rtty.login_path) - 1) < 0) {
        log_err("the program 'login' is not found\n");
        ret = -1;
        goto clean;
    }

    if (getuid() > 0) {
        log_err("Operation not permitted, must be run as root\n");
        ret = -1;
        goto clean;
    }

    if (background && daemon(0, 0))
        log_err("Can't run in the background: %s\n", strerror(errno));

    if (verbose)
        log_level(LOG_DEBUG);

    log_info("rtty version %s\n", RTTY_VERSION_STRING);

    ev_signal_init(&signal_watcher, signal_cb, SIGINT);
    ev_signal_start(loop, &signal_watcher);

#ifdef SSL_SUPPORT
    if (rtty.ssl_ctx && !has_cacert)
        load_default_ca_cert(rtty.ssl_ctx);
#endif

    srand(time(NULL));

    if (rtty_start(&rtty) < 0)
        goto clean;

    ev_run(loop, 0);

    rtty_exit(&rtty);

clean:
#ifdef SSL_SUPPORT
    ssl_context_free(rtty.ssl_ctx);
#endif

    free_config(&cfg);

    ev_loop_destroy(loop);

    return ret;
}
