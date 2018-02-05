#ifndef _H_PROTOCOL
#define _H_PROTOCOL

#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>

#define RTTY_PROTOCOL_VERSION	1

#define RTTY_CLIENT_TYPE_DEVICE 0
#define RTTY_CLIENT_TYPE_BROWSE 1

/* rtty packet type */
enum rtty_packet_type {
	RTTY_PACKET_LOGIN = 1,
    RTTY_PACKET_LOGINACK,
    RTTY_PACKET_LOGOUT,
    RTTY_PACKET_TTY,
    RTTY_PACKET_ANNOUNCE,
    RTTY_PACKET_UPFILE,
    RTTY_PACKET_DOWNFILE
};

/* rtty attribute type */
enum rtty_attr_type {
	RTTY_ATTR_SID = 1,
	RTTY_ATTR_CODE,
	RTTY_ATTR_DATA,
	RTTY_ATTR_NAME,
	RTTY_ATTR_SIZE
};

struct rtty_packet {
	uint16_t size;
	uint32_t len;
	uint8_t *data;
};

struct rtty_packet_info {
	uint8_t version;
	uint8_t type;
	uint8_t code;
	const char *sid;
	const uint8_t *data;
	uint16_t data_len;
	const char *name;
	uint32_t size;
};

struct rtty_packet *rtty_packet_new(int size);
void rtty_packet_free(struct rtty_packet *pkt);
int rtty_packet_grow(struct rtty_packet *pkt, int size);
int rtty_packet_init(struct rtty_packet *pkt, enum rtty_packet_type type);

int rtty_attr_put(struct rtty_packet *pkt, enum rtty_attr_type type, uint16_t len, const void *data);

static inline int rtty_attr_put_u8(struct rtty_packet *pkt, enum rtty_attr_type type, uint8_t val)
{
	return rtty_attr_put(pkt, type, sizeof(uint8_t), &val);
}

static inline int rtty_attr_put_u16(struct rtty_packet *pkt, enum rtty_attr_type type, uint16_t val)
{
	val = htons(val);
	return rtty_attr_put(pkt, type, sizeof(uint16_t), &val);
}

static inline int rtty_attr_put_u32(struct rtty_packet *pkt, enum rtty_attr_type type, uint32_t val)
{
	val = htonl(val);
	return rtty_attr_put(pkt, type, sizeof(uint32_t), &val);
}

static inline int rtty_attr_put_string(struct rtty_packet *pkt, enum rtty_attr_type type, const char *str)
{
	return rtty_attr_put(pkt, type, strlen(str) + 1, str);
}

int rtty_packet_parse(const uint8_t *data, int len, struct rtty_packet_info *pi);

#endif
