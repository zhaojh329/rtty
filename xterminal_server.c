#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ev.h>
#include <time.h>
#include <stdlib.h>
#include "list.h"
#include "msg.h"
#include "util.h"

#define CON_TYPE_UNKNOWN 	0
#define CON_TYPE_USER 		1
#define CON_TYPE_RT 		2

LIST_HEAD(connections);

struct connection {
	int fd;
	struct list_head node;
	int type;
	int online;
	char mac[8];
	int user;
	ev_io w;
};

int rt_sock;
int usr_sock;


static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
	printf("Got signal: %d\n", w->signum);
	ev_break(loop, EVBREAK_ALL);
}

int rt_sock;
int usr_sock;

static void connection_free(struct connection *con)
{
	if (!con)
		return;
	if (con->fd > 0)
		close(con->fd);

	ev_io_stop(EV_DEFAULT, &con->w);

	list_del(&con->node);

	free(con);
}

struct connection *find_dev_by_mac(char *mac)
{
	struct connection *con;
	list_for_each_entry(con, &connections, node) {
		if (memcmp(con->mac, mac, 6))
			return con;
	}

	return NULL;
}


static void con_read_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	struct connection *con = (struct connection *)w->data;
	char buf[1024] = "";
	int len;
	struct msg *h = (struct msg *)buf;
	int direction, type;
	
	len = read(w->fd, buf, sizeof(buf));
	if (len == 0) {
		connection_free(con);
		return;
	}

	hexdump(buf, 5);
	
	direction = h->direction;
	type = h->type;
	
	if (direction == DIR_R2S) {
		if (type == TYPE_HEART) {
			if (!con->online) {
				con->type = CON_TYPE_RT;
				con->online = 1;
				memcpy(con->mac, h->buf, 6);
			}
			
		} else if (type == TYPE_DATA) {
			if (write(usr_sock, h->buf, h->data_len) < 0)
				perror("write");
			printf("data:[%s]\n", h->buf);
		}
	} else if (direction == DIR_U2S) {
		con->type = CON_TYPE_USER;

		if (type == TYPE_CONNECT) {
			struct connection *dev = find_dev_by_mac(h->buf);
			if (!dev) {
				printf("Not found\n");
				connection_free(con);
				return;
			}

			dev->user = w->fd;
			h->direction = DIR_S2R;
			if (write(dev->fd, buf, len) < 0)
				perror("write");
		} else if (type == TYPE_DATA) {
			h->direction = DIR_S2R;
			if (write(rt_sock, buf, len) < 0)
				perror("write");
		}
	}
}
static void accept_read_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	int fd = -1;
	struct connection *con = NULL;
	
	fd = accept(w->fd, NULL, NULL);
	con = calloc(1, sizeof(struct connection));
	if (!con) {
		close(fd);
		perror("calloc");
		return;
	}

	con->fd = fd;
	list_add(&con->node, &connections);

	printf("new con:%d\n", con->fd);
	ev_io_init(&con->w, con_read_cb, con->fd, EV_READ);
	con->w.data = con;
	ev_io_start(loop, &con->w);
}

int main(int argc, char **argv)
{
	struct ev_loop *loop = EV_DEFAULT;
	ev_signal sig_watcher;
	int listen_fd;
	ev_io accept_watcher;

	ev_signal_init(&sig_watcher, signal_cb, SIGINT);
	ev_signal_start(loop, &sig_watcher);

	listen_fd = tcp_listen(NULL, 7000);
	if (listen_fd < 0) {
		fprintf(stderr, "tcp_listen failed\n");
		exit(1);
	}

	ev_io_init(&accept_watcher, accept_read_cb, listen_fd, EV_READ);
	ev_io_start(loop, &accept_watcher);

	printf("Server start...\n");

	srandom(time(NULL));
	
	ev_run(loop, 0);

	printf("Server exit...\n");
		
	return 0;
}

