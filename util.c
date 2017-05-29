#include "util.h"
#include <assert.h>
#include <net/if.h>
#include <sys/ioctl.h>

int tcp_listen(const char *ip, int port)
{
	int ret = -1;
	int fd = -1;
	struct sockaddr_in addr;

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if (ip) {
		ret = inet_pton(AF_INET, ip, &addr.sin_addr.s_addr);
		if (ret == 0) {
			fprintf(stderr, "Nor a valid network  address\n");
			return -1;
		} else if (ret < 0) {
			perror("inet_pton");
			return -1;
		}
	}

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return -1;
	}

	ret = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &ret, sizeof(ret));

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		goto err;
	}
	
	if (listen(fd, 128) < 0) {
		perror("listen");
		goto err;
	}

	return fd;
err:
	if (fd > 0)
		close(fd);
	return -1;
}

int tcp_connect(const char *ip, int port)
{
	int ret = -1;
	int fd = -1;
	struct sockaddr_in addr;

	assert(ip);
		
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return -1;
	}
	
	ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		perror("connect");
		goto err;
	}

	return fd;

err:
	if (fd > 0)
		close(fd);
	return -1;
}

int get_iface_mac(const char *ifname, char mac[6])
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

void hexdump(char *buf, int len)
{
	int i;
	printf("-------- hexdump -----------\n");
	for (i = 0; i < len; i++) {
		printf("%02hhX ", buf[i]);
		if (i % 16 == 0 && i != 0)
			puts("");
	}
	puts("");
}

