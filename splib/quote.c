/*
 * Copyright (c) 2006-2007 Martin Hedenfalk <martin@bzero.se>
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

#include <string.h>

#include "dstring.h"

#define QUOTE_DEFAULT_CHARS " \t\n[](){}!"

/*
 * Quotes <string> using backslashes. <quotechars> is a string with characters
 * that need to be quoted.  Returns a new string (should be freed by the
 * caller). If <quotechars> is NULL, a default set of characters are used.
 * The quote character itself (backslash) is always quoted.
 */
char *str_quote_backslash(const char *string, const char *quotechars)
{
    if(string == NULL)
        return NULL;

    if(quotechars == NULL)
        quotechars = QUOTE_DEFAULT_CHARS;

    dstring_t *ds = dstring_new(NULL);
    const char *p;
    for(p = string; *p; p++)
    {
        char c = *p;
        if(c == '\\' || strchr(quotechars, c) != NULL)
        {
            dstring_append_char(ds, '\\');
        }
        dstring_append_char(ds, c);
    }

    char *ret = ds->string;
    dstring_free(ds, 0);

    return ret;
}

char *str_unquote(const char *str, int len)
{
    if(len <= 0)
        len = strlen(str);

    char *rs = malloc(len + 1);
    const char *pstr = str;
    const char *str_end = str + len;
    char *prs = rs;
    int state = 0;

    while(*pstr && pstr < str_end)
    {
        switch(state)
        {
            case 0:
                if(*pstr == '\'')
                    state = 1;
                else if(*pstr == '\"')
                    state = 2;
                else
                {
                    if(*pstr == '\\' && *++pstr)
                        ;
                    *prs++ = *pstr;
                }
                break;
            case 1:
                if(*pstr == '\'')
                    state = 0;
                else
                    *prs++ = *pstr;
                break;
            case 2:
                if(*pstr == '\"')
                    state = 0;
                else
                {
                    if(*pstr == '\\' && *++pstr)
                        ;
                    *prs++ = *pstr;
                }
                break;
        }
        pstr++;
    }

    *prs = 0;

    return rs;
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
    char *u = str_unquote("a\\ quoted\\ string", 9);
    fail_unless(u);
    fail_unless(strcmp(u, "a quoted") == 0);
    free(u);

    u = str_unquote("a\\ quoted\\ string", -1);
    fail_unless(u);
    fail_unless(strcmp(u, "a quoted string") == 0);
    free(u);

    u = str_unquote("a 'quoted' \"string\"", -1);
    fail_unless(u);
    fail_unless(strcmp(u, "a quoted string") == 0);
    free(u);

    u = str_quote_backslash("an unquoted   string", " q");
    fail_unless(u);
    fail_unless(strcmp(u, "an\\ un\\quoted\\ \\ \\ string") == 0);
    free(u);

    return 0;
}

#endif

