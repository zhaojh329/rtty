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

#include <sys/time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include "utils.h"
#include "file.h"

static uint8_t RTTY_FILE_MAGIC[12] = {0xb6, 0xbc, 0xbd};

static struct timeval start_time;

static uint32_t total_size;

static char fifo_name[128];

static void clear_fifo()
{
    unlink(fifo_name);
}

static void signal_handler(int sig)
{
    puts("");
    exit(0);
}

static u_int32_t update_progress(uint8_t *buf)
{
    struct timeval now;
    uint32_t remain;

    gettimeofday(&now, NULL);

    memcpy(&remain, buf, 4);

    printf("%100c\r", ' ');
    printf("  %lu%%    %s     %.3fs\r", (total_size - remain) * 100UL / total_size,
           format_size(total_size - remain),
           (now.tv_sec + now.tv_usec / 1000.0 / 1000) - (start_time.tv_sec + start_time.tv_usec / 1000.0 / 1000));
    fflush(stdout);

    return remain;
}

static void handle_file_control_msg(int fd, int sfd, const char *path)
{
    struct file_control_msg msg;
    struct buffer b = {};

    while (true) {
        if (buffer_put_fd(&b, fd, -1, NULL) < 0)
            break;
        
        if (buffer_length(&b) < sizeof(msg))
            continue;

        buffer_pull(&b, &msg, sizeof(msg));

        switch (msg.type) {
        case RTTY_FILE_MSG_REQUEST_ACCEPT:
            if (sfd > -1) {
                close(sfd);
                gettimeofday(&start_time, NULL);
                printf("Transferring '%s'...Press Ctrl+C to cancel\n", basename(path));

                if (total_size == 0) {
                    printf("  100%%    0 B     0s\n");
                    goto done;
                }
            } else {
                printf("Waiting to receive. Press Ctrl+C to cancel\n");
            }
            break;
        
        case RTTY_FILE_MSG_INFO:
            memcpy(&total_size, msg.buf, 4);
            
            printf("Transferring '%s'...\n", (char *)(msg.buf + 4));

            if (total_size == 0) {
                printf("  100%%    0 B     0s\n");
                goto done;
            }
            
            gettimeofday(&start_time, NULL);

            break;
        
        case RTTY_FILE_MSG_PROGRESS:
            if (update_progress(msg.buf) == 0) {
                puts("");
                goto done;
            }
            break;
        
        case RTTY_FILE_MSG_ABORT:
            puts("");
            goto done;

        case RTTY_FILE_MSG_BUSY:
            printf("\033[31mRtty is busy to transfer file\033[0m\n");
            goto done;

        case RTTY_FILE_MSG_NO_SPACE:
            printf("\033[31mNo enough space\033[0m\n");
            goto done;

        case RTTY_FILE_MSG_ERR_EXIST:
            printf("\033[31mThe file already exists\033[0m\n");
            goto done;

        default:
            goto done;
        }
    }

done:
    buffer_free(&b);
}

void request_transfer_file(char type, const char *path)
{
    pid_t pid = getpid();
    struct stat st;
    int sfd = -1;
    int ctlfd;

    if (type == 'R') {
        if (access(".", W_OK | X_OK)) {
            printf("Permission denied\n");
            exit(EXIT_FAILURE);
        }
    } else {
        sfd = open(path, O_RDONLY);
        if (sfd < 0) {
            printf("open '%s' failed: ", path);
            if (errno == ENOENT)
                printf("No such filen\n");
            else
                printf("%s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        fstat(sfd, &st);
        if (!(st.st_mode & S_IFREG)) {
            printf("'%s' is not a regular file\n", path);
            close(sfd);
            exit(EXIT_FAILURE);
        }

        if (st.st_size > UINT32_MAX) {
            printf("'%s' is too large(> %u Byte)\n", path, UINT32_MAX);
            close(sfd);
            exit(EXIT_FAILURE);
        }

        total_size = st.st_size;
    }

    sprintf(fifo_name, "/tmp/rtty-file-%d.fifo", pid);

    if (mkfifo(fifo_name, 0644) < 0) {
        fprintf(stderr, "Could not create fifo %s\n", fifo_name);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, signal_handler);

    atexit(clear_fifo);

    usleep(10000);

    RTTY_FILE_MAGIC[3] = type;

    memcpy(RTTY_FILE_MAGIC + 4, &pid, 4);

    if (type == 'S')
        memcpy(RTTY_FILE_MAGIC + 8, &sfd, 4);

    fwrite(RTTY_FILE_MAGIC, sizeof(RTTY_FILE_MAGIC), 1, stdout);
    fflush(stdout);

    ctlfd = open(fifo_name, O_RDONLY | O_NONBLOCK);
    if (ctlfd < 0) {
        fprintf(stderr, "Could not open fifo %s\n", fifo_name);
        exit(EXIT_FAILURE);
    }

    handle_file_control_msg(ctlfd, sfd, path);

    close(ctlfd);
}
