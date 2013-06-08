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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dstring.h"
#include "xstr.h"
#include "log.h"

static void dstring_expand(dstring_t *dstring, unsigned needed)
{
    assert(dstring);

    if(dstring->length + needed > dstring->allocated)
    {
        /* round to nearest power of 2 */

        unsigned new_allocated = 1;
        while(new_allocated < dstring->length + needed)
        {
            new_allocated <<= 1;
        }
        assert(new_allocated > dstring->allocated);

        char *tmp = realloc(dstring->string, new_allocated);
        assert(tmp);

        dstring->string = tmp;
        dstring->string[dstring->allocated] = 0;
        dstring->allocated = new_allocated;
    }
}

dstring_t *dstring_new(const char *initial)
{
    dstring_t *s = calloc(1, sizeof(dstring_t));
    if(initial)
    {
        dstring_append(s, initial);
    }
    return s;
}

char *dstring_free(dstring_t *dstring, int free_data)
{
    char *ret = NULL;
    if(dstring)
    {
        if(free_data)
        {
            free(dstring->string);
        }
        else
        {
            ret = dstring->string;
        }
        free(dstring);
    }

    return ret;
}

void dstring_append_len(dstring_t *dstring, const char *data, size_t len)
{
    dstring_expand(dstring, len + 1);
    strlcat(dstring->string, data, dstring->length + len + 1);
    dstring->length += len;
}

void dstring_append(dstring_t *dstring, const char *cstring)
{
    dstring_append_len(dstring, cstring, strlen(cstring));
}

void dstring_append_format(dstring_t *dstring, const char *fmt, ...)
{
    va_list ap;
    char *tmp = 0;

    va_start(ap, fmt);
    int num_returned_bytes = vasprintf(&tmp, fmt, ap);
    if (num_returned_bytes == -1)
        DEBUG("vasprintf did not return anything");
    va_end(ap);

    dstring_append(dstring, tmp);
    free(tmp);
}

void dstring_append_char(dstring_t *dstring, char c)
{
    dstring_expand(dstring, 2);
    char *e = dstring->string + dstring->length;
    *e++ = c;
    *e = 0;
    dstring->length++;
}

#ifdef TEST

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
    dstring_t *s = dstring_new(NULL);
    fail_unless(s);
    fail_unless(s->length == 0);
    fail_unless(s->allocated == 0);

    dstring_append(s, "foo");
    fail_unless(s->string);
    fail_unless(s->length == 3);
    fail_unless(s->allocated == 4);
    fail_unless(strcmp(s->string, "foo") == 0);

    dstring_append(s, " bar");
    fail_unless(s->string);
    fail_unless(s->length == 7);
    fail_unless(s->allocated == 8);
    fail_unless(strcmp(s->string, "foo bar") == 0);

    dstring_append_format(s, "[%i]", 17);
    fail_unless(s->string);
    fail_unless(s->length == 11);
    fail_unless(s->allocated == 16);
    fail_unless(strcmp(s->string, "foo bar[17]") == 0);

    dstring_append_format(s, "-%s\n", "gazonk");
    fail_unless(s->string);
    fail_unless(s->length == 19);
    fail_unless(s->allocated == 32);
    fail_unless(strcmp(s->string, "foo bar[17]-gazonk\n") == 0);

    fail_unless(dstring_free(s, 1) == NULL);

    s = dstring_new("initial");
    fail_unless(s);
    fail_unless(s->string);
    fail_unless(s->length == 7);
    fail_unless(s->allocated == 8);
    fail_unless(strcmp(s->string, "initial") == 0);

    dstring_append_char(s, '1');
    fail_unless(s);
    fail_unless(s->string);
    fail_unless(s->length == 8);
    fail_unless(s->allocated == 16);
    fail_unless(strcmp(s->string, "initial1") == 0);

    dstring_append_char(s, '2');
    fail_unless(s);
    fail_unless(s->string);
    fail_unless(s->length == 9);
    fail_unless(s->allocated == 16);
    fail_unless(strcmp(s->string, "initial12") == 0);

    char *str = s->string;
    char *ret = dstring_free(s, 0);
    fail_unless(ret == str);
    fail_unless(strcmp(str, "initial12") == 0);
    free(str);

    s = dstring_new(NULL);
    dstring_append_len(s, "aaa", 2);
    dstring_append_len(s, "bbb", 2);
    dstring_append_len(s, "ccc", 2);
    fail_unless(s->string);
    fail_unless(s->length == 6);
    fail_unless(s->allocated == 8);
    fail_unless(strcmp(s->string, "aabbcc") == 0);
    fail_unless(dstring_free(s, 1) == NULL);

    return 0;
}

#endif

