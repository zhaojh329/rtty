#include <mongoose.h>
#include "util.h"
#include <termios.h>

static const char *s_address = "localhost:1883";

static void ev_handler(struct mg_connection *nc, int ev, void *data)
{
	struct mg_mqtt_message *msg = (struct mg_mqtt_message *)data;

	switch (ev) {
		case MG_EV_CONNECT: {
			struct mg_send_mqtt_handshake_opts opts;
			memset(&opts, 0, sizeof(opts));

			mg_set_protocol_mqtt(nc);
			mg_send_mqtt_handshake_opt(nc, "xxxx12342", opts);
			break;
		}

		case MG_EV_MQTT_CONNACK: {				
			static struct mg_mqtt_topic_expression s_topic_expr[] = {
				{.topic = "touser", MG_MQTT_QOS(0)}
			};
							
			if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
				printf("Got mqtt connection error: %d\n", msg->connack_ret_code);
				ev_break(EV_DEFAULT, EVBREAK_ALL);
				return;
			}

			mg_mqtt_subscribe(nc, s_topic_expr, 
				sizeof(s_topic_expr) / sizeof(struct mg_mqtt_topic_expression), 0);

			mg_mqtt_publish(nc, "connect", 0, MG_MQTT_QOS(0), NULL, 0);
			
			break;
		}

		case MG_EV_MQTT_PUBLISH: {
			if (!mg_vcmp(&msg->topic, "touser")) {
				fprintf(stderr, "----------------len =  %d---------------\n", (int)msg->payload.len);
				fprintf(stderr, "%.*s", (int)msg->payload.len, msg->payload.p);
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


static void stdin_read_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	int len = 0;
	char buf[1024] = "";
	struct mg_connection *nc = (struct mg_connection *)w->data;
	
	len = read(w->fd, buf, sizeof(buf));
	if (len > 0)
		mg_mqtt_publish(nc, "todev", 0, MG_MQTT_QOS(0), buf, len);
}


int main(int argc, char *argv[])
{
	struct ev_loop *loop = EV_DEFAULT;
	ev_signal sig_watcher;
	struct mg_mgr mgr;
	ev_io stdin_watcher;
	struct mg_connection *nc;
	struct termios nterm;

	ev_signal_init(&sig_watcher, signal_cb, SIGINT);
	ev_signal_start(loop, &sig_watcher);
	
	mg_mgr_init(&mgr, NULL);

	nc = mg_connect(&mgr, s_address, ev_handler);
	if (!nc) {
		fprintf(stderr, "mg_connect(%s) failed\n", s_address);
	}

	ev_io_init(&stdin_watcher, stdin_read_cb, 0, EV_READ);
	stdin_watcher.data = nc;
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

	printf("exit...\n");
	
	mg_mgr_free(&mgr);

	return 0;
}


