#ifndef __UTIL_H_
#define __UTIL_H_

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>

int tcp_listen(const char *ip, int port);
int tcp_connect(const char *ip, int port);
int get_iface_mac(const char *ifname, char mac[6]);
void hexdump(char *buf, int len);

#endif
