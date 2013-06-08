/*
 * Copyright (c) 2006 Martin Hedenfalk <martin@bzero.se>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#include "rx.h"
#include "xstr.h"

void rx_free(const void *rx)
{
    if(rx)
    {
        regfree((regex_t *)rx);
        free((void *)rx);
    }
}

const void *rx_compile(const char *pattern)
{
    regex_t *rx = (regex_t *)calloc(1, sizeof(regex_t));
    int rc = regcomp(rx, pattern, REG_EXTENDED | REG_ICASE);
    if(rc != 0)
    {
        // char errbuf[128] = "";
        // regerror(rc, rx, errbuf, sizeof(errbuf));
        rx_free(rx);
    }

    return rx;
}

static int rx_num_valid_subexprs(regmatch_t *pmatch, int max)
{
    int i;
    for(i = 0; i < max; i++)
    {
        if(pmatch[i].rm_so == -1)
        {
            break;
        }
    }

    return i;
}

rx_subs_t *rx_search_precompiled(const char *string, const void *rx)
{
    regmatch_t pmatch[60];
    int rc = regexec((const regex_t *)rx, string, 60, pmatch, 0);

    int nsubs = rx_num_valid_subexprs(pmatch, 60);
    if(rc == 0 && nsubs == ((regex_t *)rx)->re_nsub + 1)
    {
        if(nsubs > 1)
        {
            int i;

            rx_subs_t *subs = malloc(sizeof(rx_subs_t));
            subs->nsubs = nsubs - 1;
            subs->subs = (char **)malloc(sizeof(char *) * subs->nsubs);

            for(i = 1; i < nsubs; i++)
            {
                size_t l = pmatch[i].rm_eo - pmatch[i].rm_so;
                subs->subs[i - 1] = malloc(l + 1);
                strlcpy(subs->subs[i - 1], string + pmatch[i].rm_so, l + 1);
            }

            return subs;
        }

        return NULL;
    }

    return NULL;
}

rx_subs_t *rx_search(const char *string, const char *pattern)
{
    const void *rx = rx_compile(pattern);
    rx_subs_t *subs = NULL;
    if(rx)
    {
        subs = rx_search_precompiled(string, rx);
        rx_free(rx);
    }
    return subs;
}

void rx_free_subs(rx_subs_t *subs)
{
    if(subs)
    {
        int i;
        for(i = 0; i < subs->nsubs; i++)
        {
            free(subs->subs[i]);
        }
        free(subs->subs);
        free(subs);
    }
}

#ifdef TEST

#include <stdio.h>

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
    rx_subs_t *subs = rx_search("foo bar baz",
            "([a-zA-Z]+) {1,}(b.?r*) +([[:alpha:]]+)");

    fail_unless(subs);
    fail_unless(subs->nsubs == 3);
    fail_unless(subs->subs[0]);
    fail_unless(strcmp(subs->subs[0], "foo") == 0);
    fail_unless(subs->subs[1] && strcmp(subs->subs[1], "bar") == 0);
    fail_unless(subs->subs[2] && strcmp(subs->subs[2], "baz") == 0);

    rx_free_subs(subs);

    return 0;
}

#endif

