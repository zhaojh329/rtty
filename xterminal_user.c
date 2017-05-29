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

int sockfd;

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{	
	printf("Got signal: %d\n", w->signum);
	ev_break(loop, EVBREAK_ALL);
}

static void stdin_read_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	char buf[1024] = "";
	struct msg *h = (struct msg *)buf;
	int len = read(w->fd, h->buf, sizeof(buf) - sizeof(struct msg));
	h->direction = DIR_U2S;
	h->type = TYPE_DATA;
	h->data_len = len;
	
	if (write(sockfd, h, sizeof(struct msg) + len) < 0)
		perror("write");
}

static void sock_read_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	char buf[1024] = "";
	int len = read(w->fd, buf, sizeof(buf));
	if (write(1, buf, len) < 0)
		perror("write");
}

int main(int argc, char **argv)
{
	struct termios nterm;
	struct ev_loop *loop = EV_DEFAULT;
	ev_signal sig_watcher;
	ev_io stdin_watcher;
	ev_io sock_watcher;
	struct sockaddr_in servaddr;
	struct msg h = {
		.direction = DIR_U2S,
		.type = TYPE_CONNECT
	};

	if (argc != 3) {
		fprintf(stderr, "Usage:%s serviceIP port\n", argv[0]);
		exit(0);
	}
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(argv[1]);	/* server IP */
	servaddr.sin_port = htons(atoi(argv[2]));
	
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	
	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("connect");
		exit(1);
	}
	
	if (write(sockfd, &h, sizeof(h)) < 0)
		perror("write");
	
	ev_signal_init(&sig_watcher, signal_cb, SIGINT);
	ev_signal_start(loop, &sig_watcher);
	
	ev_io_init(&sock_watcher, sock_read_cb, sockfd, EV_READ);
	ev_io_start(loop, &sock_watcher);

	ev_io_init(&stdin_watcher, stdin_read_cb, 0, EV_READ);
	ev_io_start(loop, &stdin_watcher);
	
	tcgetattr(0, &nterm);
	
	nterm.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	nterm.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
	nterm.c_lflag |=  ISIG;
	nterm.c_cc[VMIN] = 1;
	nterm.c_cc[VTIME] = 0;
	nterm.c_cc[VINTR] = 1;
	
	tcsetattr(0, TCSANOW, &nterm);
	
	ev_run(loop, 0);
		
	return 0;
}

