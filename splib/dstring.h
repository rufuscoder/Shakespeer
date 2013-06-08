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

#ifndef _dstring_h_
#define _dstring_h_

#include <stdlib.h>

typedef struct dstring dstring_t;
struct dstring
{
    unsigned length;
    unsigned allocated;
    char *string;
};

dstring_t *dstring_new(const char *initial);
char *dstring_free(dstring_t *dstring, int free_data);
void dstring_append(dstring_t *dstring, const char *cstring);
void dstring_append_len(dstring_t *dstring, const char *data, size_t len);
void dstring_append_format(dstring_t *dstring, const char *fmt, ...);
void dstring_append_char(dstring_t *dstring, char c);

#endif

