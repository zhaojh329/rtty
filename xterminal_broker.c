#include <mongoose.h>
#include <termios.h>
#include "common.h"

static LIST_HEAD(device_list);
static LIST_HEAD(session_list);

struct device {
	char mac[13];
	int alive;
	struct list_head node;
};

struct session {
	char id[33];
	struct list_head node;
	struct mg_connection *nc;
};

static struct session *session_new(struct mg_connection *nc)
{
	char buf[128];
	struct timeval tv;
	
	struct session *s = calloc(1, sizeof(struct session));
	if (!s) {
		perror("calloc");
		return NULL;
	}
	
	list_add(&s->node, &session_list);
	
	gettimeofday(&tv, NULL);
	
	snprintf(buf, sizeof(buf), "%ld", tv.tv_sec * 1000 + tv.tv_usec / 1000);
	
	cs_md5(s->id, buf, strlen(buf), NULL);
	
	s->nc = nc;
	printf("new session:%s\n", s->id);
	
	return s;
}

static struct device *find_device_by_mac(struct mg_str *mac)
{
	struct device *dev = NULL;
	
	list_for_each_entry(dev, &device_list, node) {
		if (!mg_vcmp(mac, dev->mac))
			return dev;
	}
	
	return NULL;
}

static void update_device(struct mg_str *mac)
{
	struct device *dev = find_device_by_mac(mac);
	if (!dev) {
		dev = calloc(1, sizeof(struct device));
		if (!dev) {
			perror("calloc");
			return;
		}
		
		list_add(&dev->node, &device_list);
		strncpy(dev->mac, mac->p, mac->len);
	}
	
	dev->alive = 10;
}
	
struct mg_connection *mqtt_nc;

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
	if (ev == MG_EV_CONNECT) {
		/* Connect to mqtt broker */
		struct mg_send_mqtt_handshake_opts opts;
		memset(&opts, 0, sizeof(opts));

		mg_set_protocol_mqtt(nc);
		mg_send_mqtt_handshake_opt(nc, "xxxx12342", opts);
		
	} else if (ev == MG_EV_MQTT_CONNACK) {
		struct mg_mqtt_message *msg = (struct mg_mqtt_message *)ev_data;
		struct mg_mqtt_topic_expression topic_expr[1];
		
		if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
			printf("Got mqtt connection error: %d\n", msg->connack_ret_code);
			ev_break(EV_DEFAULT, EVBREAK_ALL);
			return;
		}

		topic_expr[0].qos = MG_MQTT_QOS(0);
		asprintf((char **)&topic_expr[0].topic, "xterminal/heartbeat");
		
		mg_mqtt_subscribe(nc, topic_expr, 1, 0);

		free((char *)topic_expr[0].topic);
		
		mqtt_nc = nc;
		
	} else if (ev == MG_EV_MQTT_PUBLISH) {
		struct mg_mqtt_message *msg = (struct mg_mqtt_message *)ev_data;
		
		if (!mg_vcmp(&msg->topic, "xterminal/heartbeat")) {
			update_device(&msg->payload);
		}
			
	} else if (ev == MG_EV_HTTP_REQUEST) {
		struct http_message *hm = (struct http_message *)ev_data;
		if (!mg_vcmp(&hm->uri, "/list")) {
			struct device *dev;
			
			mg_send_head(nc, 200, -1, NULL);
			
			mg_printf_http_chunk(nc, "[");
			
			list_for_each_entry(dev, &device_list, node) {
				mg_printf_http_chunk(nc, "\"%s\",", dev->mac);
			}
			
			mg_printf_http_chunk(nc, "\"\"]");
			
			mg_send_http_chunk(nc, "", 0);
		} else {
			struct mg_serve_http_opts opts = {};
			mg_serve_http(nc, hm, opts);
		}
	
	} else if (ev == MG_EV_WEBSOCKET_FRAME) {
		struct websocket_message *wm = (struct websocket_message *)ev_data;
		const char *pattern = "connect ????????????";
		const struct mg_str pstr = {pattern, strlen(pattern)};
		
		if (mg_match_prefix_n(pstr, mg_mk_str_n((const char *)wm->data, wm->size))) {
			char topic[128] = "";
			struct session *s = session_new(nc);
			if (!s)
				return;
			
			sprintf(topic, "xterminal/%.*s/%s", (int)(wm->size - 8), wm->data + 8, s->id);
			mg_mqtt_publish(mqtt_nc, topic, 0, MG_MQTT_QOS(0), NULL, 0);
		}
		
	} else if (ev == MG_EV_CLOSE) {
	}
}

static void device_alive_cb(struct ev_loop *loop, ev_timer *w, int revents)
{
	struct device *dev, *tmp;
	
	list_for_each_entry_safe(dev, tmp, &device_list, node) {
		if (dev->alive-- == 0) {
			printf("del timeout device:%s\n", dev->mac);
			list_del(&dev->node);
			free(dev);
		}
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
	struct mg_connection *nc;
	const char *s_address = "localhost:1883";
	const char *s_http_port = "8000";
	ev_timer device_timer;
	
	ev_signal_init(&sig_watcher, signal_cb, SIGINT);
	ev_signal_start(loop, &sig_watcher);
	
	ev_timer_init(&device_timer, device_alive_cb, 0.1, 2.0);
	ev_timer_start(loop, &device_timer);
	
	mg_mgr_init(&mgr, NULL);

	nc = mg_connect(&mgr, s_address, ev_handler);
	if (!nc) {
		fprintf(stderr, "mg_connect(%s) failed\n", s_address);
	}
	
	nc = mg_bind(&mgr, s_http_port, ev_handler);
	if (nc == NULL) {
		printf("Failed to create listener\n");
		return 1;
	}
	
	mg_set_protocol_http_websocket(nc);
	
	ev_run(loop, 0);

	printf("exit...\n");
	
	mg_mgr_free(&mgr);

	return 0;
}


