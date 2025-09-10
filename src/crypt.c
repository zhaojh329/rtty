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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#elif defined(HAVE_WOLFSSL)
#include <wolfssl/options.h>
#include <wolfssl/openssl/evp.h>
#else
#include <mbedtls/pkcs5.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#endif

#include "crypt.h"

void *cipher_init_ctx(const char *password)
{
    uint8_t key[32];

#if defined(HAVE_OPENSSL) || defined(HAVE_WOLFSSL)
    EVP_CIPHER_CTX *cipher = EVP_CIPHER_CTX_new();

    PKCS5_PBKDF2_HMAC(password, strlen(password), NULL, 0, 1024, EVP_sha256(), 32, key);
    EVP_EncryptInit(cipher, EVP_aes_256_ecb(), key, NULL);

    return cipher;
#else
    mbedtls_aes_context *aes_ctx;
    mbedtls_md_context_t md_ctx;

    mbedtls_md_init(&md_ctx);
    mbedtls_md_setup(&md_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_pkcs5_pbkdf2_hmac(&md_ctx, (const uint8_t *)password, strlen(password),
                         NULL, 0, 1024,  32, key);
    mbedtls_md_free(&md_ctx);

    aes_ctx = malloc(sizeof(mbedtls_aes_context));

    mbedtls_aes_init(aes_ctx);
    mbedtls_aes_setkey_enc(aes_ctx, key, 256);

    return aes_ctx;
#endif
}

int get_random_bytes(uint8_t *out, size_t len)
{
    int ret = 0;
    int fd;

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
        return -1;

    if (read(fd, out, len) < 0)
        ret = -1;

    close(fd);

    return ret;
}

static uint8_t initial_vector[] = {167, 115, 79, 156, 18, 172, 27, 1, 164, 21, 242, 193, 252, 120, 230, 107};

static void xor_bytes(uint8_t *dst, const uint8_t *a, const uint8_t *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] = a[i] ^ b[i];
    }
}

static inline void aes_block_encrypt(void *ctx, uint8_t *dst, const uint8_t *src)
{
#if defined(HAVE_OPENSSL) || defined(HAVE_WOLFSSL)
    int out_len;
    EVP_EncryptUpdate(ctx, dst, &out_len, src, 16);
#else
    mbedtls_aes_crypt_ecb(ctx, MBEDTLS_AES_ENCRYPT, src, dst);
#endif
}

void encrypt16(void *ctx, uint8_t *dst, const uint8_t *src, size_t src_len)
{
    size_t n = src_len / 16;
    size_t repeat = n / 8;
    size_t left = n % 8;
    size_t remaining;
    size_t base = 0;
    uint8_t tbl[16];

    aes_block_encrypt(ctx, tbl, initial_vector);

    for (size_t i = 0; i < repeat; i++) {
        const uint8_t *s = src + base;
        uint8_t *d = dst + base;
        for (int j = 0; j < 8; j++) {
            xor_bytes(d + j*16, s + j*16, tbl, 16);
            aes_block_encrypt(ctx, tbl, d + j * 16);
        }
        base += 128;
    }

    switch (left) {
    case 7:
        xor_bytes(dst + base, src + base, tbl, 16);
        aes_block_encrypt(ctx, tbl, dst + base);
        base += 16;
    case 6:
        xor_bytes(dst + base, src + base, tbl, 16);
        aes_block_encrypt(ctx, tbl, dst + base);
        base += 16;
    case 5:
        xor_bytes(dst + base, src + base, tbl, 16);
        aes_block_encrypt(ctx, tbl, dst + base);
        base += 16;
    case 4:
        xor_bytes(dst + base, src + base, tbl, 16);
        aes_block_encrypt(ctx, tbl, dst + base);
        base += 16;
    case 3:
        xor_bytes(dst + base, src + base, tbl, 16);
        aes_block_encrypt(ctx, tbl, dst + base);
        base += 16;
    case 2:
        xor_bytes(dst + base, src + base, tbl, 16);
        aes_block_encrypt(ctx, tbl, dst + base);
        base += 16;
    case 1:
        xor_bytes(dst + base, src + base, tbl, 16);
        aes_block_encrypt(ctx, tbl, dst + base);
        base += 16;
    case 0:
        remaining = src_len - base;
        if (remaining > 0)
            xor_bytes(dst + base, src + base, tbl, remaining);
        break;
    }
}

void decrypt16(void *ctx, uint8_t *dst, const uint8_t *src, size_t src_len)
{
    size_t n = src_len / 16;
    size_t repeat = n / 8;
    size_t left = n % 8;
    size_t base = 0;
    uint8_t buf[32];
    uint8_t *tbl = buf;
    uint8_t *next = buf + 16;
    uint8_t *temp_ptr;

    aes_block_encrypt(ctx, tbl, initial_vector);

    for (size_t i = 0; i < repeat; i++) {
        const uint8_t *s = src + base;
        uint8_t *d = dst + base;

        aes_block_encrypt(ctx, next, s);
        xor_bytes(d, s, tbl, 16);

        aes_block_encrypt(ctx, tbl, s + 16);
        xor_bytes(d + 16, s + 16, next, 16);

        aes_block_encrypt(ctx, next, s + 32);
        xor_bytes(d + 32, s + 32, tbl, 16);

        aes_block_encrypt(ctx, tbl, s + 48);
        xor_bytes(d + 48, s + 48, next, 16);

        aes_block_encrypt(ctx, next, s + 64);
        xor_bytes(d + 64, s + 64, tbl, 16);

        aes_block_encrypt(ctx, tbl, s + 80);
        xor_bytes(d + 80, s + 80, next, 16);

        aes_block_encrypt(ctx, next, s + 96);
        xor_bytes(d + 96, s + 96, tbl, 16);

        aes_block_encrypt(ctx, tbl, s + 112);
        xor_bytes(d + 112, s + 112, next, 16);

        base += 128;
    }
    
    switch (left) {
    case 7:
        aes_block_encrypt(ctx, next, src + base);
        xor_bytes(dst + base, src + base, tbl, 16);
        temp_ptr = tbl;
        tbl = next;
        next = temp_ptr;
        base += 16;
    case 6:
        aes_block_encrypt(ctx, next, src + base);
        xor_bytes(dst + base, src + base, tbl, 16);
        temp_ptr = tbl;
        tbl = next;
        next = temp_ptr;
        base += 16;
    case 5:
        aes_block_encrypt(ctx, next, src + base);
        xor_bytes(dst + base, src + base, tbl, 16);
        temp_ptr = tbl;
        tbl = next;
        next = temp_ptr;
        base += 16;
    case 4:
        aes_block_encrypt(ctx, next, src + base);
        xor_bytes(dst + base, src + base, tbl, 16);
        temp_ptr = tbl;
        tbl = next;
        next = temp_ptr;
        base += 16;
    case 3:
        aes_block_encrypt(ctx, next, src + base);
        xor_bytes(dst + base, src + base, tbl, 16);
        temp_ptr = tbl;
        tbl = next;
        next = temp_ptr;
        base += 16;
    case 2:
        aes_block_encrypt(ctx, next, src + base);
        xor_bytes(dst + base, src + base, tbl, 16);
        temp_ptr = tbl;
        tbl = next;
        next = temp_ptr;
        base += 16;
    case 1:
        aes_block_encrypt(ctx, next, src + base);
        xor_bytes(dst + base, src + base, tbl, 16);
        temp_ptr = tbl;
        tbl = next;
        next = temp_ptr;
        base += 16;
    case 0:
        size_t remaining = src_len - base;
        if (remaining > 0)
            xor_bytes(dst + base, src + base, tbl, remaining);
        break;
    }
}
