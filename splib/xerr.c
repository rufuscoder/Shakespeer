/*
 * Copyright (c) 2006 Martin Hedenfalk <martin.hedenfalk@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "xerr.h"
#include "log.h"

const char *xerr_msg(xerr_t *xerr)
{
    return xerr && xerr->message ? xerr->message : "unknown error";
}

void xerr_set(xerr_t **xerr, int code, const char *fmt, ...)
{
    if(xerr == NULL)
        return;

    assert(*xerr == NULL);
    *xerr = calloc(1, sizeof(xerr_t));

    free((*xerr)->message);
    (*xerr)->message = 0;
    va_list ap;
    va_start(ap, fmt);
    int num_returned_bytes = vasprintf(&(*xerr)->message, fmt, ap);
    if (num_returned_bytes == -1)
        DEBUG("vasprintf did not return anything");
    va_end(ap);

    (*xerr)->code = code;
}

void xerr_free(xerr_t *xerr)
{
    if(xerr)
    {
        free(xerr->message);
        free(xerr);
    }
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
    xerr_t *xerr = 0;

    fail_unless(strcmp(xerr_msg(NULL), "unknown error") == 0);
    xerr_set(&xerr, 42, "testing %s %i", "error handling", 17);
    fail_unless(xerr);
    fail_unless(strcmp(xerr_msg(xerr), "testing error handling 17") == 0);
    fail_unless(xerr->code == 42);
    xerr_free(xerr);

    return 0;
}

#endif

