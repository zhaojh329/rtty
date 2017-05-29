#ifndef __UTIL_H_
#define __UTIL_H_

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>

int get_iface_mac(const char *ifname, char mac[6]);

#endif
