#include <mongoose.h>
#include <pty.h>
#include "util.h"

static char *devid;
static const char *s_address = "localhost:1883";

ev_io pty_watcher;
int pty;

int id = 0;

static void pty_read_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	int len = 0;
	char buf[1024] = "";
	struct mg_connection *nc = (struct mg_connection *)w->data;
	
	len = read(w->fd, buf, sizeof(buf));
	if (len > 0) {
		printf("---------------------len = %d-------------------------------\n", len);
		printf("[%.*s]\n", len, buf);
		mg_mqtt_publish(nc, "touser", id++, MG_MQTT_QOS(0), buf, len);
	}
	else {
		perror("read");
		ev_io_stop(EV_DEFAULT, w);
	}
}

static void start_pty(struct mg_connection *nc)
{
	pid_t pid;
	
	pid = forkpty(&pty, NULL, NULL, NULL);
	if (pid < 0) {
		perror("forkpty");
		return;
	} else if (pid == 0) {
		execl("/bin/login", "login", NULL);
	}

	ev_io_init(&pty_watcher, pty_read_cb, pty, EV_READ);
	pty_watcher.data = nc;
	ev_io_start(EV_DEFAULT, &pty_watcher);
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
			static struct mg_mqtt_topic_expression s_topic_expr[] = {
				{.topic = "connect", MG_MQTT_QOS(0)},
				{.topic = "todev", MG_MQTT_QOS(0)}
			};
							
			if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
				printf("Got mqtt connection error: %d\n", msg->connack_ret_code);
				ev_break(EV_DEFAULT, EVBREAK_ALL);
				return;
			}

			mg_mqtt_subscribe(nc, s_topic_expr, 
				sizeof(s_topic_expr) / sizeof(struct mg_mqtt_topic_expression), 0);
			break;
		}

		case MG_EV_MQTT_PUBLISH: {
			printf("%.*s:[%.*s]\n", (int)msg->topic.len, msg->topic.p, (int)msg->payload.len,
				msg->payload.p);

			if (!mg_vcmp(&msg->topic, "connect")) {
				start_pty(nc);
			} else if (!mg_vcmp(&msg->topic, "todev")) {
				if(write(pty, msg->payload.p, msg->payload.len) < 0)
					perror("write");
			}
		
			break;
		}
		case MG_EV_CLOSE:
	      printf("Connection closed\n");
	      exit(1);
	}
}

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
	printf("Got signal: %d\n", w->signum);
	ev_break(loop, EVBREAK_ALL);
}

int main(int argc, char *argv[])
{
	struct ev_loop *loop = EV_DEFAULT;
	ev_signal sig_watcher;
	struct mg_mgr mgr;
	char mac[6] = "";

	ev_signal_init(&sig_watcher, signal_cb, SIGINT);
	ev_signal_start(loop, &sig_watcher);
	
	mg_mgr_init(&mgr, NULL);

	if (mg_connect(&mgr, s_address, ev_handler) == NULL) {
		fprintf(stderr, "mg_connect(%s) failed\n", s_address);
	}

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


