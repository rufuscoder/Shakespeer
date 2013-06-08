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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> /* for gethostbyname */
#include <arpa/inet.h> /* for inet_ntoa */

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "uhttp.h"
#include "log.h"

uhttp_t *uhttp_init(void)
{
    uhttp_t *uhttp = calloc(1, sizeof(uhttp_t));
    uhttp->port = 80; /* default http port */

    return uhttp;
}

/* open a TCP connection to uhttp->hostname on port uhttp->port
*/
int uhttp_open(uhttp_t *uhttp)
{
    struct hostent *he;

    uhttp->fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(uhttp->fd == -1)
    {
        DEBUG("%s", strerror(errno));
        return -1;
    }

    uhttp->raddr.sin_family = AF_INET;
    uhttp->raddr.sin_port = htons(uhttp->port);

    if(inet_aton(uhttp->hostname, &uhttp->raddr.sin_addr) == 0)
    {
        he = gethostbyname(uhttp->hostname);
        if(!he)
        {
            DEBUG("%s: %s", uhttp->hostname, hstrerror(h_errno));
            return -1;
        }

        memcpy(&uhttp->raddr.sin_addr, he->h_addr, he->h_length);
    }

    DEBUG("Connecting to %s:%d",
            inet_ntoa(uhttp->raddr.sin_addr), ntohs(uhttp->raddr.sin_port));
    if(connect(uhttp->fd, (struct sockaddr *)&uhttp->raddr,
                sizeof(uhttp->raddr)) != 0)
    {
        DEBUG("%s", strerror(errno));
        return -1;
    }

    return 0;
}

int uhttp_open_url(uhttp_t *uhttp, const char *urlstr)
{
    if(strncmp(urlstr, "http://", 7) == 0)
        urlstr += 7;

    char *xurl = strdup(urlstr);
    char *e = strchr(xurl, ':');
    if(e)
    {
        *e = 0;
        uhttp->port = atoi(e+1);
        e = strchr(e+1, '/');
    }
    else
        e = strchr(xurl, '/');

    if(e && *e)
    {
        *e = 0;
        if(e[1])
            uhttp->directory = strdup(e + 1);
    }

    uhttp->hostname = strdup(xurl);
    free(xurl);

    DEBUG("hostname: %s", uhttp->hostname);
    DEBUG("port: %i", uhttp->port);
    DEBUG("directory: %s", uhttp->directory);

    return uhttp_open(uhttp);
}

void uhttp_free_headers(uhttp_t *uhttp)
{
    if(uhttp)
    {
        int i;

        for(i = 0; i < uhttp->nheaders; i++)
            free(uhttp->headers[i]);
        free(uhttp->headers);
        uhttp->headers = 0;
        uhttp->nheaders = 0;
    }
}

void uhttp_cleanup(uhttp_t *uhttp)
{
    if(uhttp)
    {
        free(uhttp->hostname);
        free(uhttp->directory);
        close(uhttp->fd);
        uhttp_free_headers(uhttp);
        free(uhttp);
    }
}

static char *trim(char *str, const char *set)
{
    if(str == NULL)
        return NULL;

    char *end = strrchr(str, 0);

    if(set == NULL)
        set = " \t\n\r";

    while(--end >= str)
    {
        if(strchr(set, *end) == NULL)
            break;
        *end = 0;
    }

    return str;

}

int uhttp_add_header(uhttp_t *uhttp, const char *fmt, ...)
{
    char *str;
    va_list ap;

    va_start(ap, fmt);
    int num_returned_bytes = vasprintf(&str, fmt, ap);
    if (num_returned_bytes == -1)
        DEBUG("vasprintf did not return anything");

    uhttp->headers = (char **)realloc(uhttp->headers,
            (uhttp->nheaders+1) * sizeof(char *));
    trim(str, "\r\n"); /* CRLF is added in uhttp_send_headers() */

    char *e = str;
    while(e && *e)
    {
        *e = tolower(*e);
        ++e;
    }

    uhttp->headers[uhttp->nheaders] = str;
    uhttp->nheaders++;
    uhttp->headers_sent = 1;
    va_end(ap);

    return 0;
}

int uhttp_send_headers(uhttp_t *uhttp)
{
    int i;

    for(i = 0; i < uhttp->nheaders; i++)
    {
        uhttp_send_string(uhttp, "%s", uhttp->headers[i]);
        /* DEBUG("Sending header: %s", uhttp->headers[i]); */
    }
    uhttp_free_headers(uhttp);
    uhttp->headers_sent = 1;
    return 0;
}

static void uhttp_add_standard_headers(uhttp_t *uhttp)
{
    uhttp_add_header(uhttp, "User-Agent: uhttp/0.2");
    uhttp_add_header(uhttp, "Host: %s:%u", uhttp->hostname, uhttp->port);
}

int uhttp_get(uhttp_t *uhttp)
{
    uhttp_send_string(uhttp, "GET /%s HTTP/1.1",
            uhttp->directory ? uhttp->directory : "");
    uhttp_add_standard_headers(uhttp);
    uhttp_send_headers(uhttp);
    uhttp_send_string(uhttp, "");
    return 0;
}

/* if length == 0 this is the last chunk, and trailing headers are sent
 *
 * the headers User-Agent, Host and Transfer-Encoding are set automatically
 */
int uhttp_write_chunk(uhttp_t *uhttp, void *data, unsigned int length)
{
    if(!uhttp->headers_sent)
    {
        uhttp_send_string(uhttp, "POST /%s HTTP/1.1",
                uhttp->directory);
        uhttp_add_standard_headers(uhttp);
        uhttp_add_header(uhttp, "Transfer-Encoding: chunked");
        uhttp_send_headers(uhttp);
        uhttp_send_string(uhttp, "");
    }

     DEBUG("Sending %u bytes", length);
    /* send the content length in hex */
    uhttp_send_string(uhttp, "%X", length);
    if(length)
    {
        if(send(uhttp->fd, data, length, 0) == -1)
        {
            DEBUG("uhttp: %s", strerror(errno));
            return -1;
        }
    } else
        uhttp_send_headers(uhttp);
    uhttp_send_string(uhttp, "");

    return 0;
}

int uhttp_read_response_headers(uhttp_t *uhttp)
{
    char tmp[1024];

    uhttp_free_headers(uhttp);

    while(uhttp_read_string(uhttp, tmp, 1024) != 0)
    {
        trim(tmp, "\r\n");
        if(tmp[0] == 0)
            break; /* end of headers */
        uhttp_add_header(uhttp, "%s", tmp);
    }

    if(uhttp->nheaders == 0)
        return -1;

    /* Return HTTP status code. */
    int status = -1;
    if(sscanf(uhttp->headers[0], "http/%*i.%*i %i", &status) == 1)
	return status;

    return -1;
}

const char *uhttp_get_header(uhttp_t *uhttp, const char *header)
{
    int i;

    for(i = 1; i < uhttp->nheaders; i++)
    {
        const char *e = strchr(uhttp->headers[i], ':');
        if(e)
        {
            if(strncmp(header, uhttp->headers[i], e - uhttp->headers[i]) == 0)
            {
                while(*++e == ' ') /* do nothing */ ;
                DEBUG("found [%s]: %s", header, e);
                return e;
            }
        }
    }

    return NULL;
}

int uhttp_read_response_cb(uhttp_t *uhttp,
        uhttp_read_cb_t read_cb, void *user_data)
{
    return_val_if_fail(uhttp, -1);
    return_val_if_fail(read_cb, -1);

    const char *transfer_encoding = uhttp_get_header(uhttp, "transfer-encoding");
    if(transfer_encoding && strcmp(transfer_encoding, "chunked") == 0)
    {
        DEBUG("Detected chunked encoding");
        uhttp->chunked_encoding = 1;
    }

    const char *content_length = uhttp_get_header(uhttp, "content-length");
    if(content_length) 
    {
        uhttp->content_length = atoi(content_length); /* FIXME: use strtol +
                                                         error checking */
        uhttp->chunked_encoding = 0;
    }

    if(uhttp->chunked_encoding)
    {
        char tmp[128];
        uhttp_read_string(uhttp, tmp, 128);
        uhttp->content_length = strtoul(tmp, NULL, 16);
        if(uhttp->content_length == 0)
        {
            DEBUG("End of chunked encoding, reading trailing headers");
            /* read trailing headers */
            uhttp_read_response_headers(uhttp);
            return 0;
        }
    }

    DEBUG("Reading %u bytes of response data", uhttp->content_length);

    char buf[4096];
    size_t i = 0;

    while(i < uhttp->content_length)
    {
        size_t len;

        /* how much should we read? */
        len = uhttp->content_length - i;
        if(len > sizeof(buf))
            len = sizeof(buf);

        ssize_t rc = recv(uhttp->fd, buf, len, 0);
        if(rc < 0)
        {
            WARNING("Error reading response: %s", strerror(errno));
            return -1;
        }
        else if(rc == 0)
        {
            WARNING("EOF reading response");
            return -1;
        }

        i += rc;
        if(read_cb(uhttp, buf, rc, i, user_data) != 0)
            return -1;
    }

    if(uhttp->chunked_encoding)
    {
        /* read the blank line after each chunk */
        char tmp[128];
        uhttp_read_string(uhttp, tmp, 128);
        if(tmp[0] != 0)
            return -1;
    }

    return uhttp->chunked_encoding ? 1 : 0;
}

struct uhttp_buf
{
    void *data;
    size_t length;
};

static int uhttp_read_response_data_cb(uhttp_t *uhttp, void *data, size_t length,
        size_t index, void *user_data)
{
    struct uhttp_buf *buf = user_data;
    return_val_if_fail(buf, -1);

    void *tmp = realloc(buf->data, buf->length + length);
    return_val_if_fail(tmp, -1);
    buf->data = tmp;

    memmove(buf->data + buf->length, data, length);
    buf->length += length;

    return 0;
}

int uhttp_read_response_data(uhttp_t *uhttp, void **p_data, size_t *p_length)
{
    struct uhttp_buf buf = {NULL, 0};

    for(;;)
    {
        int rc = uhttp_read_response_cb(uhttp,
                uhttp_read_response_data_cb, &buf);
        if(rc == 0)
            break;
        else if(rc == -1)
        {
            free(buf.data);
            return -1;
        }
    }

    *p_data = buf.data;
    *p_length = buf.length;
    return 0;
}

static int uhttp_read_response_fp_cb(uhttp_t *uhttp, void *data, size_t length,
        size_t index, void *user_data)
{
    FILE *fp = user_data;
    return_val_if_fail(fp, -1);

    size_t rc = fwrite(data, 1, length, fp);
    if(rc < length)
    {
        WARNING("%s", strerror(errno));
        return -1;
    }

    return 0;
}

int uhttp_read_response_fp(uhttp_t *uhttp, FILE *fp)
{

    for(;;)
    {
        int rc = uhttp_read_response_cb(uhttp, uhttp_read_response_fp_cb, fp);
        if(rc == 0)
            break;
        else if(rc == -1)
            return -1;
    }

    return 0;
}

int uhttp_read_response_file(uhttp_t *uhttp, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if(fp == NULL)
    {
        WARNING("%s: %s", filename, strerror(errno));
        return -1;
    }

    int rc = uhttp_read_response_fp(uhttp, fp);
    fclose(fp);
    return rc;
}

int uhttp_send_string(uhttp_t *uhttp, const char *fmt, ...)
{
    va_list ap;
    char *str;
    int rc;

    va_start(ap, fmt);
    int num_returned_bytes = vasprintf(&str, fmt, ap);
    if (num_returned_bytes == -1)
        DEBUG("vasprintf did not return anything");
    va_end(ap);

    size_t l = strlen(str);
    DEBUG("sending %lu bytes: [%s]", l, str);
    rc = send(uhttp->fd, str, l, 0);
    if(rc != -1)
        rc = send(uhttp->fd, "\r\n", 2, 0);

    if(rc == -1)
        DEBUG("uhttp: %s", strerror(errno));
    free(str);
    return rc;
}

char *uhttp_read_string(uhttp_t *uhttp, char *buf, unsigned len)
{
    unsigned i = 0;
    /* FIXME: inefficient implementation */

    while(i < len - 1)
    {
        if(recv(uhttp->fd, &buf[i], 1, 0) != 1)
            break;
        if(buf[i] == '\n')
            break;
        i++;
    }

    buf[i] = 0;
    trim(buf, "\r\n");

    DEBUG("Got response: %s", buf);

    return buf;
}

