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

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "utils.h"
#include "file.h"

struct filectl_context {
    struct timespec start;
    uint32_t remaining;
    uint32_t total;
    int socket;
    int fd;
};

static volatile sig_atomic_t terminal_attrs_saved;
static struct termios terminal_attrs;
static int ttyfd = -1;

static void restore_terminal(void)
{
    if (ttyfd < 0)
        return;

    if (terminal_attrs_saved)
        tcsetattr(ttyfd, TCSANOW, &terminal_attrs);

    close(ttyfd);
}

static void on_signal(int sig)
{
    ssize_t written;

    restore_terminal();
    written = write(STDOUT_FILENO, "\n", 1);
    (void)written;
    _exit(128 + sig);
}

static int receive_local(int fd, struct file_packet *packet)
{
    int ret;

    ret = file_ipc_recv(fd, packet);
    if (!ret) {
        errno = ECONNRESET;
        return -1;
    }

    if (ret < 0)
        return -1;

    if (packet->fd >= 0) {
        close(packet->fd);
        errno = EPROTO;
        return -1;
    }

    if (packet->type == FILE_IPC_ERROR) {
        uint32_t error;

        if (packet->len != sizeof(error)) {
            errno = EPROTO;
            return -1;
        }

        memcpy(&error, packet->data, sizeof(error));
        error = ntohl(error);
        errno = error && error <= INT_MAX ? error : EPROTO;
        return -1;
    }

    return 0;
}

static void show_progress(struct filectl_context *ctx, bool complete)
{
    double elapsed = monotonic_time() - ctx->start.tv_sec - ctx->start.tv_nsec / 1000000000.0;
    uint32_t transferred = ctx->total - ctx->remaining;
    unsigned percent = ctx->total ? transferred * 100ULL / ctx->total : 100;
    double speed = elapsed > 0 ? transferred / elapsed / 1024.0 / 1024.0 : 0;

    printf("%100c\r", ' ');
    printf("  %u%%    %s    %.2f MB/s", percent, format_size(transferred), speed);
    if (complete)
        printf("    %.3fs", elapsed);

    putchar('\r');
    fflush(stdout);
}

static int transfer_file(struct filectl_context *ctx, char type)
{
    struct file_packet packet;
    bool have_info = false;
    uint32_t remaining;

    if (receive_local(ctx->socket, &packet) < 0)
        return -1;

    if (packet.type != FILE_IPC_ACCEPT || packet.len) {
        errno = EPROTO;
        return -1;
    }

    if (file_ipc_send(ctx->socket, type == 'S' ? FILE_IPC_SEND : FILE_IPC_RECV, NULL, 0, ctx->fd) < 0)
        return -1;

    close(ctx->fd);
    ctx->fd = -1;

    if (type == 'R') {
        printf("Waiting to receive. Press Ctrl+C to cancel\n");
        fflush(stdout);
    }

    while (true) {
        if (receive_local(ctx->socket, &packet) < 0)
            return -1;

        switch (packet.type) {
        case FILE_IPC_INFO:
            if (have_info || packet.len < 5 || !file_valid_name(packet.data + 4, packet.len - 4))
                break;

            memcpy(&remaining, packet.data, sizeof(remaining));
            ctx->total = ctx->remaining = ntohl(remaining);
            clock_gettime(CLOCK_MONOTONIC, &ctx->start);
            printf("Transferring '%.*s'...Press Ctrl+C to cancel\n", packet.len - 4, packet.data + 4);
            fflush(stdout);
            have_info = true;
            continue;

        case FILE_IPC_PROGRESS:
            if (!have_info || packet.len != sizeof(remaining))
                break;

            memcpy(&remaining, packet.data, sizeof(remaining));
            remaining = ntohl(remaining);
            if (remaining > ctx->remaining)
                break;

            ctx->remaining = remaining;
            show_progress(ctx, false);
            /* Completion may already be queued and the daemon socket closed.
             * This acknowledgment only enables the next progress update. */
            file_ipc_send(ctx->socket, FILE_IPC_PROGRESS, NULL, 0, -1);
            continue;

        case FILE_IPC_DONE:
            if (!have_info || packet.len)
                break;

            ctx->remaining = 0;
            show_progress(ctx, true);
            puts("");
            return 0;

        default:
            break;
        }

        errno = EPROTO;
        return -1;
    }
}

int request_transfer_file(char type, const char *path)
{
    struct filectl_context ctx = { .socket = -1, .fd = -1 };
    struct sockaddr_un addr;
    struct ucred cred;
    struct termios attrs;
    struct sigaction action = { .sa_handler = on_signal };
    socklen_t len;
    int address_len;
    int ret = 1;

    if (type == 'S') {
        ctx.fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (ctx.fd < 0)
            goto done;
    } else {
        if (access(".", W_OK | X_OK) < 0)
            goto done;

        ctx.fd = open(".", O_PATH | O_DIRECTORY | O_CLOEXEC);
        if (ctx.fd < 0)
            goto done;
    }

    ctx.socket = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (ctx.socket < 0)
        goto done;

    ttyfd = open("/dev/tty", O_RDONLY | O_CLOEXEC);
    if (ttyfd < 0) {
        fprintf(stderr, "File transfer is unavailable without a controlling terminal\n");
        goto done;
    }

    address_len = file_socket_address(&addr, ttyfd);
    if (address_len < 0)
        goto cleanup;

    if (connect(ctx.socket, (struct sockaddr *)&addr, address_len) < 0) {
        fprintf(stderr, "File transfer is unavailable in this terminal\n");
        goto done;
    }

    len = sizeof(cred);
    if (getsockopt(ctx.socket, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0)
        goto done;

    if (cred.uid != 0) {
        errno = EACCES;
        goto done;
    }

    if (tcgetpgrp(ttyfd) == getpgrp()) {
        if (tcgetattr(ttyfd, &terminal_attrs) < 0)
            goto done;

        terminal_attrs_saved = 1;

        sigemptyset(&action.sa_mask);
        if (sigaction(SIGINT, &action, NULL) < 0)
            goto done;

        attrs = terminal_attrs;
        attrs.c_lflag &= ~ECHOCTL;
        if (tcsetattr(ttyfd, TCSANOW, &attrs) < 0)
            goto done;
    }

    if (!transfer_file(&ctx, type)) {
        ret = 0;
        goto cleanup;
    }

done:
    fprintf(stderr, "File transfer failed: %s\n", strerror(errno));

cleanup:
    restore_terminal();

    if (ctx.fd >= 0)
        close(ctx.fd);

    if (ctx.socket >= 0)
        close(ctx.socket);

    return ret;
}
