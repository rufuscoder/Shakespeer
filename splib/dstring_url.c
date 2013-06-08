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

#include "dstring.h"
#include "uhttp.h"
#include "xstr.h"

static int read_callback(uhttp_t *uhttp, void *data, size_t length,
        size_t index, void *user_data)
{
    dstring_t *ds = user_data;
    dstring_append_len(ds, data, length);
    return 0;
}

dstring_t *dstring_new_from_http(const char *url)
{
    dstring_t *string = NULL;

    uhttp_t *uhttp = uhttp_init();
    if(uhttp_open_url(uhttp, url) == 0)
    {
        string = dstring_new(NULL);

        uhttp_get(uhttp);
        int rc = uhttp_read_response_headers(uhttp);
        if(rc == 200)
        {
            for(;;)
            {
                rc = uhttp_read_response_cb(uhttp, read_callback, string);
                if(rc == 0)
                    break;
                else if(rc == -1)
                {
                    dstring_free(string, 1);
                    string = NULL;
                    break;
                }
            }
        }
        else
        {
            dstring_free(string, 1);
            string = NULL;
        }
    }

    uhttp_cleanup(uhttp);

    return string;
}

#if 0
dstring_t *dstring_new_from_file(const char *filename)
{
    FILE *fp;
    dstring_t *string = NULL;
    struct statbuf sb;

    fp = fopen(filename, "r");
    if(fp == NULL)
        return NULL;


    return string;
}
#endif

dstring_t *dstring_new_from_url(const char *url)
{
    if(str_has_prefix(url, "http://"))
        return dstring_new_from_http(url);
    /* else if(str_has_prefix(url, "file://")) */
        /* return dstring_new_from_file(url + 7); */
    return NULL;
}

#ifdef TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

#define fail_unless(test) \
    do { if(!(test)) { \
        fprintf(stderr, \
                "----------------------------------------------\n" \
                "%s:%d: test FAILED: %s\n" \
                "----------------------------------------------\n", \
                __FILE__, __LINE__, #test); \
        exit(1); \
    } } while(0)

int main(void)
{
    //sp_log_set_level("debug");

    /* create a dstring from a file url pointing at this very file */
    /* dstring_t *ds = dstring_new_from_url("file://dstring_url.c"); */
    dstring_t *ds = dstring_new_from_url("http://www.google.se/");
    fail_unless(ds);
    /* make sure (sorta) that we actually read the file */
    fail_unless(strstr(ds->string, "<form action="));
    /* fail_unless(strstr(ds->string, "this text should be found in this file")); */
    dstring_free(ds, 1);
    
    return 0;
}

#endif

