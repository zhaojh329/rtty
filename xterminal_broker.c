#include <mongoose.h>
#include "util.h"
#include <termios.h>
#include <list.h>

int asprintf(char **strp, const char *fmt, ...);
void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);

static char *macaddr;
static char *myid;

static const char *s_address = "localhost:1883";
struct termios oterm, nterm;

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
			struct mg_mqtt_topic_expression topic_expr[2];
			
			if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
				printf("Got mqtt connection error: %d\n", msg->connack_ret_code);
				ev_break(EV_DEFAULT, EVBREAK_ALL);
				return;
			}

			topic_expr[0].qos = MG_MQTT_QOS(0);
			asprintf((char **)&topic_expr[0].topic, "xterminal/%s/response/data/%s", macaddr, myid);
			
			topic_expr[1].qos = MG_MQTT_QOS(0);
			asprintf((char **)&topic_expr[1].topic, "xterminal/%s/response/exit/%s", macaddr, myid);
			
			mg_mqtt_subscribe(nc, topic_expr, 2, 0);

			free((char *)topic_expr[0].topic);
			free((char *)topic_expr[1].topic);
			
			break;
		}

		case MG_EV_MQTT_SUBACK: {
			char topic[128] = "";
			sprintf(topic, "xterminal/%s/request/connect/%s", macaddr, myid);
			mg_mqtt_publish(nc, topic, 0, MG_MQTT_QOS(0), NULL, 0);
			break;
		}

		case MG_EV_MQTT_PUBLISH: {
			if (memmem(msg->topic.p, msg->topic.len, "data", strlen("data"))) {
				int ret = write(STDOUT_FILENO, msg->payload.p, msg->payload.len);
				if (ret < 0)
					perror("write");
			} else if (memmem(msg->topic.p, msg->topic.len, "exit", strlen("exit"))) {
				tcsetattr(0, TCSANOW, &oterm);
				ev_break(EV_DEFAULT, EVBREAK_ALL);
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
	char buf[1024] = "", topic[128] = "";
	struct mg_connection *nc = (struct mg_connection *)w->data;
	
	len = read(w->fd, buf, sizeof(buf));
	if (len > 0) {
		sprintf(topic, "xterminal/%s/request/data/%s", macaddr, myid);
		mg_mqtt_publish(nc, topic, 0, MG_MQTT_QOS(0), buf, len);
	}
}

		
static void usage(const char *name)
{
	fprintf(stderr,
		"Usage: %s [OPTION]\n"
		"	-m macaddr\n"
		"	-i id\n"
		"\n", name
	);
	
	exit(0);
}

int main(int argc, char *argv[])
{
	struct ev_loop *loop = EV_DEFAULT;
	ev_signal sig_watcher;
	struct mg_mgr mgr;
	ev_io stdin_watcher;
	struct mg_connection *nc;
	int ch;

	while ((ch = getopt(argc, argv, "m:i:")) != -1) {
		switch (ch) {
		case 'm':
			macaddr = optarg;
			break;

		case 'i':
			myid = optarg;
			break;
			
		default:
			usage(argv[0]);
		}
	}
	
	if ((optind < argc) || !macaddr || !myid) {
		usage(argv[0]);
	}
	
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

	tcgetattr(0, &oterm);
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


