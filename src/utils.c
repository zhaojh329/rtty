/*
 * MIT License
 *
 * Copyright (c) 2019 Jianhui Zhao <jianhuizhao329@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "utils.h"
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <uwsc/log.h>

int get_iface_mac(const char *ifname, char *dst, int len)
{
    struct ifreq ifr;
    int sock;
    uint8_t *hw;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        uwsc_log_err("create socket failed:%s\n", strerror(errno));
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
        uwsc_log_err("ioctl failed:%s\n", strerror(errno));
        close(sock);
        return -1;
    }

    close(sock);

    hw = (uint8_t *)ifr.ifr_hwaddr.sa_data;
    snprintf(dst, len, "%02X%02X%02X%02X%02X%02X", hw[0], hw[1], hw[2], hw[3], hw[4], hw[5]);

    return 0;
}

/* blen is the size of buf; slen is the length of src.  The input-string need
** not be, and the output string will not be, null-terminated.  Returns the
** length of the encoded string, or -1 on error (buffer overflow) */
int urlencode(char *buf, int blen, const char *src, int slen)
{
    int i;
    int len = 0;
    static const char hex[] = "0123456789abcdef";

    for (i = 0; (i < slen) && (len < blen); i++) {
        if (isalnum(src[i]) || (src[i] == '-') || (src[i] == '_') ||
            (src[i] == '.') || (src[i] == '~')) {
            buf[len++] = src[i];
        } else if (src[i] == ' ') {
            buf[len++] = '+';
        } else if ((len + 3) <= blen) {
            buf[len++] = '%';
            buf[len++] = hex[(src[i] >> 4) & 15];
            buf[len++] = hex[ src[i]       & 15];
        } else {
            len = -1;
            break;
        }
    }

    return (i == slen) ? len : -1;
}

int find_login(char *buf, int len)
{
    FILE *fp = popen("which login", "r");
    if (fp) {
        if (fgets(buf, len, fp))
            buf[strlen(buf) - 1] = 0;
        pclose(fp);

        if (!buf[0])
            return -1;
        return 0;
    }

    return -1;
}

bool valid_id(const char *id)
{
    while (*id) {
        if (!isalnum(*id) && *id != '-' && *id != '_')
            return false;
        id++;
    }

    return true;
}

