/*
 * Copyright (C) 2017 Jianhui Zhao <jianhuizhao329@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <libubox/ulog.h>
#include <libubox/utils.h>

#include "protocol.h"

struct rtty_packet *rtty_packet_new(int size)
{
	struct rtty_packet *pkt = calloc(1, sizeof(struct rtty_packet));
	if (!pkt) {
		ULOG_ERR("malloc failed:%s\n", strerror(errno));
		return NULL;
	}

	pkt->data = malloc(size);
	if (!pkt->data) {
		free(pkt);
		ULOG_ERR("malloc failed:%s\n", strerror(errno));
		return NULL;
	}

	pkt->size = size;

	return pkt;
}

int rtty_packet_grow(struct rtty_packet *pkt, int size)
{
	uint8_t *data = realloc(pkt->data, pkt->size + size);
	if (!data) {
		ULOG_ERR("rtty_packet_grow failed:%s\n", strerror(errno));
		return -1;
	}
	pkt->data = data;
	pkt->size += size;
	return 0;
}

void rtty_packet_free(struct rtty_packet *pkt)
{
	if (pkt) {
		if (pkt->data)
			free(pkt->data);
		free(pkt);
	}
}

int rtty_packet_init(struct rtty_packet *pkt, enum rtty_packet_type type)
{
	uint8_t *data = pkt->data;;

	if (pkt->size < 2 || !data) {
		ULOG_ERR("rtty_packet_init failed\n");
		return -1;
	}

	data[0] = RTTY_PROTOCOL_VERSION;
	data[1] = type;
	pkt->len = 2;

	return 0;
}

int rtty_attr_put(struct rtty_packet *pkt, enum rtty_attr_type type, uint16_t len, const void *data)
{
	uint8_t *p = pkt->data + pkt->len;

	if (pkt->len + len + 3 > pkt->size) {
		if (rtty_packet_grow(pkt, len) < 0) {
			ULOG_ERR("rtty_packet_grow failed\n");
			return -1;
		}
		p = pkt->data + pkt->len;
	}

	pkt->len += 3 + len;

	p[0] = type;

	memcpy(p + 3, data, len);

	len = htons(len);
	memcpy(p + 1, &len, sizeof(len));

	return 0;
}

int rtty_packet_parse(const uint8_t *data, int pktlen, struct rtty_packet_info *pi)
{
	const uint8_t *p;

	memset(pi, 0, sizeof(struct rtty_packet_info));

	pi->version = data[0];
	pi->type = data[1];

	p = data + 2;

	while(p < data + pktlen) {
		uint8_t type;
		uint16_t len;

		type = p[0];
		memcpy(&len, p + 1, 2);

		len = ntohs(len);

		p += 3;

		/* Check if len is invalid */
		if (p + len > data + pktlen)
			break;

		switch (type) {
		case RTTY_ATTR_SID:
			pi->sid = (const char *)p;
			break;
		case RTTY_ATTR_DATA:
			pi->data = p;
			pi->data_len = len;
			break;
		case RTTY_ATTR_CODE:
			pi->code = *p;
			break;
		case RTTY_ATTR_NAME:
			pi->name = (const char *)p;
			break;
		case RTTY_ATTR_SIZE:
			memcpy(&pi->size, p, 4);
			pi->size = ntohl(pi->size);
			break;
		default:
			break;
		}
		p += len;
	}

	return 0;
}