#include "util.h"
#include <assert.h>
#include <net/if.h>
#include <sys/ioctl.h>

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


