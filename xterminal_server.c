#include <mongoose.h>
#include <pty.h>
#include "util.h"
#include "list.h"

struct client {
	char cliid[128];
	char session[128];
	int pty;
	ev_io watcher;
	struct mg_connection *nc;
	struct list_head node;
};

int asprintf(char **strp, const char *fmt, ...);
void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);

LIST_HEAD(clients);

static char *devid;
static const char *s_address = "localhost:1883";

static void pty_read_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	int len = 0;
	char buf[1024] = "";
	char topic[128] = "";
	struct client *c = (struct client *)w->data;
	
	len = read(w->fd, buf, sizeof(buf));
	if (len > 0) {
		sprintf(topic, "xterminal/%s/response/data/%s", devid, c->cliid);
		mg_mqtt_publish(c->nc, topic, 0, MG_MQTT_QOS(0), buf, len);
	}
	else {
		perror("read");
		ev_io_stop(EV_DEFAULT, w);
	}
}

static void start_pty(struct client *c)
{
	pid_t pid;
	struct timeval tv;
	struct mg_mqtt_topic_expression topic_expr;
	
	printf("new con:[%s]\n", c->cliid);
	
	pid = forkpty(&c->pty, NULL, NULL, NULL);
	if (pid < 0) {
		perror("forkpty");
		return;
	} else if (pid == 0) {
		execl("/bin/login", "login", NULL);
	}

	gettimeofday(&tv, NULL);

	sprintf(c->session, "%ld", tv.tv_sec * 1000 + tv.tv_usec / 1000);
	asprintf((char **)&topic_expr.topic, "xterminal/%s/request/%s/data/+", devid, c->session);
	mg_mqtt_subscribe(c->nc, &topic_expr, 1, 0);
	free((char *)topic_expr.topic);
	
	ev_io_init(&c->watcher, pty_read_cb, c->pty, EV_READ);
	c->watcher.data = c;
	ev_io_start(EV_DEFAULT, &c->watcher);
}

static void ev_handler(struct mg_connection *nc, int ev, void *data)
{
	struct mg_mqtt_message *msg = (struct mg_mqtt_message *)data;

	switch (ev) {
		case MG_EV_CONNECT: {
			struct mg_send_mqtt_handshake_opts opts;
			memset(&opts, 0, sizeof(opts));

			mg_set_protocol_mqtt(nc);
			mg_send_mqtt_handshake_opt(nc, devid, opts);
			break;
		}

		case MG_EV_MQTT_CONNACK: {				
			struct mg_mqtt_topic_expression topic_expr[2];
							
			if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
				printf("Got mqtt connection error: %d\n", msg->connack_ret_code);
				ev_break(EV_DEFAULT, EVBREAK_ALL);
				return;
			}

			topic_expr[0].qos = MG_MQTT_QOS(0);
			asprintf((char **)&topic_expr[0].topic, "xterminal/%s/request/connect/+", devid);

			topic_expr[1].qos = MG_MQTT_QOS(0);
			asprintf((char **)&topic_expr[1].topic, "xterminal/%s/request/data/+", devid);
			
			mg_mqtt_subscribe(nc, topic_expr, 2, 0);

			free((char *)topic_expr[0].topic);
			free((char *)topic_expr[1].topic);
			
			break;
		}

		case MG_EV_MQTT_PUBLISH:
			if (memmem(msg->topic.p, msg->topic.len, "connect", strlen("connect"))) {
				struct client *c = calloc(1, sizeof(struct client));
				if (!c) {
					perror("calloc");
					break;
				}
				c->nc = nc;
				memcpy(c->cliid, msg->topic.p + 39, msg->topic.len - 39);
				start_pty(c);
			} else if (memmem(msg->topic.p, msg->topic.len, "data", strlen("data"))) {
				int ret = write(1, msg->payload.p, msg->payload.len);
				if (ret < 0)
					perror("write");
			}
		
			break;
		
		case MG_EV_CLOSE:
	      printf("Connection closed\n");
	      exit(1);
	}
}

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
	char topic[128] = "";
	struct mg_connection *nc = (struct mg_connection *)w->data;
	printf("Got signal: %d\n", w->signum);

	if (w->signum == SIGCHLD) {
		sprintf(topic, "xterminal/%s/response/exit/%s", devid, cliid);
		mg_mqtt_publish(nc, topic, 0, MG_MQTT_QOS(0), NULL, 0);
	} else if (w->signum == SIGINT) {
		ev_break(loop, EVBREAK_ALL);
	}
}

int main(int argc, char *argv[])
{
	struct ev_loop *loop = EV_DEFAULT;
	ev_signal sigint_watcher;
	ev_signal sigchld_watcher;
	struct mg_mgr mgr;
	struct mg_connection *nc;
	char mac[6] = "";
	
	mg_mgr_init(&mgr, NULL);

	nc = mg_connect(&mgr, s_address, ev_handler); 
	if (!nc) {
		fprintf(stderr, "mg_connect(%s) failed\n", s_address);
		exit(1);
	}

	ev_signal_init(&sigint_watcher, signal_cb, SIGINT);
	sigint_watcher.data = nc;
	ev_signal_start(loop, &sigint_watcher);

	ev_signal_init(&sigchld_watcher, signal_cb, SIGCHLD);
	sigchld_watcher.data = nc;
	ev_signal_start(loop, &sigchld_watcher);

	get_iface_mac("enp3s0", mac);

	devid = calloc(1, 13);
	sprintf(devid, "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	printf("devid:%s\n", devid);
	
	ev_run(loop, 0);

	printf("exit...\n");

	free(devid);
	
	mg_mgr_free(&mgr);

	return 0;
}


