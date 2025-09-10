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

#include <sys/types.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#define IEEE 0xedb88320

#define slicing8_cutoff 16

typedef uint32_t table[256];

typedef struct slicing8_table {
    table tables[8];
} slicing8_table;

static slicing8_table tab;

static void simple_populate_table(uint32_t poly, table t)
{
    int i, j;

    for (i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ poly;
            } else {
                crc >>= 1;
            }
        }
        t[i] = crc;
    }
}

static void slicing_make_table(uint32_t poly)
{
    int i, j;

    simple_populate_table(poly, tab.tables[0]);

    for (i = 0; i < 256; i++) {
        uint32_t crc = tab.tables[0][i];
        for (j = 1; j < 8; j++) {
            crc = tab.tables[0][crc & 0xFF] ^ (crc >> 8);
            tab.tables[j][i] = crc;
        }
    }
}

static uint32_t simple_update(uint32_t crc, const table tab, const uint8_t *p, size_t len)
{
    size_t i;

    crc = ~crc;

    for (i = 0; i < len; i++)
        crc = tab[(uint8_t)crc ^ p[i]] ^ (crc >> 8);

    return ~crc;
}

uint32_t crc32_checksum_ieee(const uint8_t* p, size_t len)
{
    static bool inited;
    uint32_t crc = 0;

    if (!inited) {
        inited = true;
        slicing_make_table(IEEE);
    }

    if (len >= slicing8_cutoff) {
        crc = ~crc;
        while (len >= 8) {
            uint32_t n;
            memcpy(&n, p, sizeof(n));
            crc ^= n;
            crc = tab.tables[0][p[7]] ^
                  tab.tables[1][p[6]] ^
                  tab.tables[2][p[5]] ^
                  tab.tables[3][p[4]] ^
                  tab.tables[4][crc >> 24] ^
                  tab.tables[5][(crc >> 16) & 0xFF] ^
                  tab.tables[6][(crc >> 8) & 0xFF] ^
                  tab.tables[7][crc & 0xFF];
            p += 8;
            len -= 8;
        }
        crc = ~crc;
    }

    if (len == 0)
        return crc;

    return simple_update(crc, tab.tables[0], p, len);
}
