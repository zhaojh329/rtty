#include <mongoose.h>
#include <pty.h>
#include <net/if.h>
#include "common.h"

static char *devid;

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
	if (ev == MG_EV_CONNECT) {
		/* Connect to mqtt broker */
		struct mg_send_mqtt_handshake_opts opts;
		memset(&opts, 0, sizeof(opts));

		mg_set_protocol_mqtt(nc);
		mg_send_mqtt_handshake_opt(nc, devid, opts);
		
	} else if (ev == MG_EV_MQTT_CONNACK) {
		struct mg_mqtt_message *msg = (struct mg_mqtt_message *)ev_data;
		struct mg_mqtt_topic_expression topic_expr[1];
		
		if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
			printf("Got mqtt connection error: %d\n", msg->connack_ret_code);
			ev_break(EV_DEFAULT, EVBREAK_ALL);
			return;
		}
		
		topic_expr[0].qos = MG_MQTT_QOS(0);
		asprintf((char **)&topic_expr[0].topic, "xterminal/%s/+", devid);
		
		mg_mqtt_subscribe(nc, topic_expr, 1, 0);

		free((char *)topic_expr[0].topic);
		
	} else if (ev == MG_EV_MQTT_PUBLISH) {
		struct mg_mqtt_message *msg = (struct mg_mqtt_message *)ev_data;
		printf("connect:%.*s\n", (int)msg->topic.len, msg->topic.p);
	}
}

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
	if (w->signum == SIGINT) {
		ev_break(loop, EVBREAK_ALL);
	}
}

static void heartbeat_cb(struct ev_loop *loop, ev_timer *w, int revents)
{
	struct mg_connection *nc = (struct mg_connection *)w->data;
	mg_mqtt_publish(nc, "xterminal/heartbeat", 0, MG_MQTT_QOS(0), devid, strlen(devid));
}

static int get_iface_mac(const char *ifname, char mac[6])
{
    int r, s;
    struct ifreq ifr;

    strncpy(ifr.ifr_name, ifname, 15);
    ifr.ifr_name[15] = '\0';

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (-1 == s) {
		perror("socket");
		return -1;
    }

    r = ioctl(s, SIOCGIFHWADDR, &ifr);
    if (r == -1) {
        perror("ioctl");
        close(s);
        return -1;
    }

    close(s);

	memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
	
    return 0;
}

int main(int argc, char *argv[])
{
	struct ev_loop *loop = EV_DEFAULT;
	ev_signal sigint_watcher;
	ev_timer heartbeat_timer;
	struct mg_mgr mgr;
	struct mg_connection *nc;
	char mac[6] = "";
	const char *s_address = "localhost:1883";
	
	mg_mgr_init(&mgr, NULL);

	nc = mg_connect(&mgr, s_address, ev_handler); 
	if (!nc) {
		fprintf(stderr, "mg_connect(%s) failed\n", s_address);
		exit(1);
	}

	ev_signal_init(&sigint_watcher, signal_cb, SIGINT);
	sigint_watcher.data = nc;
	ev_signal_start(loop, &sigint_watcher);
	
	ev_timer_init(&heartbeat_timer, heartbeat_cb, 0.1, 2.0);
	heartbeat_timer.data = nc;
	ev_timer_start(loop, &heartbeat_timer);

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


