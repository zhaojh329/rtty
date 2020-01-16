/*
 * MIT License
 *
 * Copyright (c) 2019 Jianhui Zhao <zhaojh329@gmail.com>
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
#include <sys/time.h>

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

/* reference from https://tools.ietf.org/html/rfc4648#section-4 */
int b64_encode(const void *src, size_t srclen, void *dest, size_t destsize)
{
    char *Base64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    const uint8_t *input = src;
    char *output = dest;

    while (srclen > 0) {
            int skip = 1;
            int i0 = input[0] >> 2;
            int i1 = (input[0] & 0x3) << 4;
            int i2 = 64;
            int i3 = 64;

            if (destsize < 5)
                return -1;

            if (srclen > 1) {
                    skip++;
                    i1 += input[1] >> 4;
                    i2 = (input[1] & 0xF) << 2;

                    if (srclen > 2) {
                            i2 += input[2] >> 6;
                            i3 = input[2] & 0x3F;
                            skip++;
                        }
                }

            *output++ = Base64[i0];
            *output++ = Base64[i1];
            *output++ = Base64[i2];
            *output++ = Base64[i3];

            input += skip;
            srclen -= skip;
            destsize -= 4;
        }

    *output++ = 0;
    return output - (char *)dest - 1;
}

const char *format_size(size_t size)
{
    static char str[64];

    if (size < 1024)
        sprintf(str,"%zu B", size);
    else if (size < 1024 * 1024)
        sprintf(str,"%.2f KB", size / 1024.0);
    else
        sprintf(str,"%.2f MB", size / 1024.0 / 1024.0);

    return str;
}

