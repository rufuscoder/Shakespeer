/*
 * Copyright (c) 2006 Martin Hedenfalk <martin@bzero.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _uhttp_h
#define _uhttp_h

#include <stdio.h>
#include <netinet/in.h>

typedef struct uhttp uhttp_t;

typedef int (*uhttp_read_cb_t)(uhttp_t *, void *, size_t, size_t, void *);

struct uhttp
{
    int fd;
    struct sockaddr_in raddr; /* remote address */
    struct sockaddr_in laddr; /* local address */

    char *hostname;
    char *directory;
    int port;

    unsigned nheaders;
    char **headers;

    int chunked_encoding;
    unsigned content_length;

    int headers_sent;
};

uhttp_t *uhttp_init(void);
void uhttp_cleanup(uhttp_t *uhttp);
int uhttp_open(uhttp_t *uhttp);
int uhttp_open_url(uhttp_t *uhttp, const char *urlstr);
void uhttp_free_headers(uhttp_t *uhttp);
int uhttp_add_header(uhttp_t *uhttp, const char *fmt, ...);
int uhttp_send_headers(uhttp_t *uhttp);
int uhttp_write_chunk(uhttp_t *uhttp, void *data, unsigned int length);
int uhttp_read_response_headers(uhttp_t *uhttp);
int uhttp_send_string(uhttp_t *uhttp, const char *fmt, ...);
char *uhttp_read_string(uhttp_t *uhttp, char *buf, unsigned int len);
const char *uhttp_get_header(uhttp_t *uhttp, const char *header);
int uhttp_get(uhttp_t *uhttp);

int uhttp_read_response_cb(uhttp_t *uhttp,
        uhttp_read_cb_t read_cb, void *user_data);
int uhttp_read_response_data(uhttp_t *uhttp, void **p_data, size_t *p_length);
int uhttp_read_response_fp(uhttp_t *uhttp, FILE *fp);
int uhttp_read_response_file(uhttp_t *uhttp, const char *filename);

#endif

