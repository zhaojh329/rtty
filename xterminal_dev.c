#include <pty.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ev.h>
#include "msg.h"
#include "util.h"

pid_t pid;
int pty;
ev_io pty_watcher;
static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{	
	printf("Got signal: %d\n", w->signum);
	ev_break(loop, EVBREAK_ALL);
}

static void pty_read_cb(struct ev_loop *loop, ev_io *w, int revents)
{

	int len = 0;
	char buf[1024] = "";
	struct msg *h = (struct msg *)buf;
	
	len = read(w->fd, h->buf, sizeof(buf) - sizeof(struct msg));
	if (len > 0) {
		h->direction = DIR_R2S;
		h->type = TYPE_DATA;
		h->data_len = len;
		if (write(sockfd, h, sizeof(struct msg) + len) < 0)
			perror("write");
	} else {
		perror("read");
		ev_io_stop(loop, w);
	}
}

static void tcp_read_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	int len = 0;
	char buf[1024] = "";
	struct msg *h = (struct msg *)buf;
	
	len = read(w->fd, buf, sizeof(buf));
	if (len <= 0) {
		perror("read");
		ev_io_stop(loop, w);
		return;
	}
	
	if (h->direction == DIR_S2R) {
		if (h->type == TYPE_CONNECT) {
			pid = forkpty(&pty, NULL, NULL, NULL);
			if (pid < 0) {
				perror("forkpty");
				return;
			} else if (pid == 0) {
				execl("/bin/login", "login", NULL);
			}
			
			printf("new con, start pty...\n");
			
			ev_io_init(&pty_watcher, pty_read_cb, pty, EV_READ);
			ev_io_start(loop, &pty_watcher);
		} else if (h->type == TYPE_DATA) {
			if (write(pty, h->buf, h->data_len) < 0)
				perror("write");
		}
	}
}


// heart
char devid[6];

static void timeout_cb(struct ev_loop *loop, ev_timer *w, int revents)
{
	int *sockfd = (int *)w->data;
	char buf[sizeof(struct msg) + 6];
	struct msg *m = (struct msg *)buf;

	m->direction = DIR_R2S;
	m->type = TYPE_HEART;
	m->data_len = 6;
	
	memcpy(m->buf, devid, 6);
	
	if (write(*sockfd, buf, sizeof(buf)) < 0)
		perror("write");
	hexdump(buf, 5);
	printf("heart to server...:%d %d\n", *sockfd, (int)sizeof(buf));

	printf("%d %d %02X\n", m->direction, m->type, m->buf[0]);
}

int main(int argc, char **argv)
{
	struct ev_loop *loop = EV_DEFAULT;
	ev_signal sig_watcher;
	ev_io sock_watcher;
	ev_timer timeout_watcher;
	int sockfd;

	if (argc != 3) {
		fprintf(stderr, "Usage:%s serviceIP port\n", argv[0]);
		exit(0);
	}

	sockfd = tcp_connect(argv[1], atoi(argv[2]));
	if (sockfd < 0) {
		perror("socket");
		exit(1);
	}

	get_iface_mac("enp3s0", devid);
	printf("devid: %02X\n", devid[0]);
	
	ev_signal_init(&sig_watcher, signal_cb, SIGINT);
	ev_signal_start(loop, &sig_watcher);
	
	ev_io_init(&sock_watcher, tcp_read_cb, sockfd, EV_READ);
	ev_io_start(loop, &sock_watcher);
	
	ev_timer_init(&timeout_watcher, timeout_cb, 1.0, 5.0);
	timeout_watcher.data = &sockfd;
	ev_timer_start(loop, &timeout_watcher);
	
	ev_run(loop, 0);
		
	return 0;
}

