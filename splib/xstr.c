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
/* strlcpy and strlcat below are from OpenBSD 3.8
 *
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "xstr.h"

char *xstrdup(const char *s)
{
    if(s)
        return strdup(s);
    return NULL;
}

char *xstrndup(const char *s, size_t len)
{
    char *r;
  
    if(s == NULL)
        return NULL;
 
    r = (char *)malloc(len + 1);
    strlcpy(r, s, len + 1);

    return r;
}

/* bounded version of strnchr
 * */
const char *xstrnchr(const char *s, unsigned maxlen, int c)
{
    const char *e = s + maxlen;
    while(s && *s && s < e)
    {
        if(*s == c)
            return s;
        ++s;
    }

    return NULL;
}

#if defined(linux)

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t strlcpy(char *dst, const char *src, size_t siz)
{
        char *d = dst;
        const char *s = src;
        size_t n = siz;

        /* Copy as many bytes as will fit */
        if (n != 0 && --n != 0) {
                do {
                        if ((*d++ = *s++) == 0)
                                break;
                } while (--n != 0);
        }

        /* Not enough room in dst, add NUL and traverse rest of src */
        if (n == 0) {
                if (siz != 0)
                        *d = '\0';              /* NUL-terminate dst */
                while (*s++)
                        ;
        }

        return(s - src - 1);    /* count does not include NUL */
}
/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t
strlcat(char *dst, const char *src, size_t siz)
{
        char *d = dst;
        const char *s = src;
        size_t n = siz;
        size_t dlen;

        /* Find the end of dst and adjust bytes left but don't go past end */
        while (n-- != 0 && *d != '\0')
                d++;
        dlen = d - dst;
        n = siz - dlen;

        if (n == 0)
                return(dlen + strlen(s));
        while (*s != '\0') {
                if (n != 1) {
                        *d++ = *s;
                        n--;
                }
                s++;
        }
        *d = '\0';

        return(dlen + (s - src));       /* count does not include NUL */
}

/*
 * Find the first occurrence of find in s, ignore case.
 */
char *strcasestr(const char *s, const char *find)
{
        char c, sc;
        size_t len;

        if ((c = *find++) != 0) {
                c = tolower((unsigned char)c);
                len = strlen(find);
                do {
                        do {
                                if ((sc = *s++) == 0)
                                        return (NULL);
                        } while ((char)tolower((unsigned char)sc) != c);
                } while (strncasecmp(s, find, len) != 0);
                s--;
        }
        return ((char *)s);
}

#endif /* linux */


char *str_trim_end_inplace(char *str, const char *set)
{
    if(str == NULL)
        return NULL;

    char *end = strrchr(str, 0);
    assert(end);

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

char *str_trim_end(const char *str, const char *set)
{
    char *copy = xstrdup(str);
    return str_trim_end_inplace(copy, set);
}

/* Returns 1 if the string @s has @prefix as prefix, otherwise 0.
 */
int str_has_prefix(const char *s, const char *prefix)
{
    if(s == NULL || prefix == NULL)
        return 0;

    return strncmp(s, prefix, strlen(prefix)) == 0 ? 1 : 0;
}

int str_has_suffix(const char *s, const char *suffix)
{
    if(s == NULL || suffix == NULL)
        return 0;

    const char *e = s + strlen(s) - strlen(suffix);
    return e >= s && strncmp(e, suffix, strlen(suffix)) == 0;
}

char *str_replace_set(char *string, const char *set, char replacement)
{
    if(string == NULL || set == NULL)
        return NULL;

    char *e = string;
    while(*e)
    {
        if(strchr(set, *e) != NULL)
            *e = replacement;
        ++e;
    }

    return string;
}

char *q_strsep(char **pp, const char *set)
{
	if(pp == NULL || *pp == NULL || set == NULL)
		return 0;

	char *p = *pp;
	char *op = p;
	char *np = p;

	for(; *p; p++)
	{
		if(*p == '\\')
		{
			if(++p == 0)
				break;
		}
		else if(strchr(set, *p) != NULL)
		{
			++p;
			break;
		}

		*np++ = *p;
	}

	*np = '\0';
	*pp = p;

	return op;
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
    /* xstrdup
     */
    fail_unless(xstrdup(NULL) == NULL);

    const char *orig = "dup";
    char *dup = xstrdup(orig);
    fail_unless(dup);
    fail_unless(dup != orig);
    fail_unless(strcmp(dup, orig) == 0);
    free(dup);

    /* xstrndup
     */
    fail_unless(xstrndup(NULL, 17) == NULL);

    dup = xstrndup(orig, 2);
    fail_unless(dup);
    fail_unless(dup != orig);
    fail_unless(strcmp(dup, "du") == 0);
    free(dup);

    dup = xstrndup(orig, 1);
    fail_unless(dup);
    fail_unless(dup != orig);
    fail_unless(strcmp(dup, "d") == 0);
    free(dup);

    dup = xstrndup(orig, 0);
    fail_unless(dup);
    fail_unless(dup != orig);
    fail_unless(dup[0] == 0);
    free(dup);

    dup = xstrndup(orig, 4);
    fail_unless(dup);
    fail_unless(dup != orig);
    fail_unless(strcmp(dup, orig) == 0);
    free(dup);

    dup = xstrndup(orig, 42);
    fail_unless(dup);
    fail_unless(dup != orig);
    fail_unless(strcmp(dup, orig) == 0);
    free(dup);

    /* xstrnchr
     */
    char *s = "foo bar";
    fail_unless(xstrnchr(s, 42, 'b') == s + 4);
    fail_unless(xstrnchr(s, 5, 'b') == s + 4);
    fail_unless(xstrnchr(s, 4, 'b') == NULL);

    /* str_trim_end_inplace and str_trim_end
     */
    char *trimmed = str_trim_end("trim me \t\r   \n", NULL);
    fail_unless(trimmed);
    fail_unless(strcmp(trimmed, "trim me") == 0);
    free(trimmed);

    trimmed = xstrdup("trim me too   \t");
    fail_unless(str_trim_end_inplace(trimmed, " \t\r\n") == trimmed);
    fail_unless(strcmp(trimmed, "trim me too") == 0);
    fail_unless(str_trim_end_inplace(trimmed, "o t") == trimmed);
    fail_unless(strcmp(trimmed, "trim me") == 0);
    free(trimmed);

    /* str_has_prefix
     */
    fail_unless(str_has_prefix(NULL, "foo") == 0);
    fail_unless(str_has_prefix("bar", NULL) == 0);
    fail_unless(str_has_prefix("bar", "foo") == 0);
    fail_unless(str_has_prefix("bar", "barf") == 0);
    fail_unless(str_has_prefix("foobar", "foo") == 1);

    /* str_has_suffix
     */
    fail_unless(str_has_suffix(NULL, "foo") == 0);
    fail_unless(str_has_suffix("bar", NULL) == 0);
    fail_unless(str_has_suffix("bar", "foo") == 0);
    fail_unless(str_has_suffix("foobar", "fooba") == 0);
    fail_unless(str_has_suffix("foobar", "bar") == 1);
    fail_unless(str_has_suffix("foobar", "foobar") == 1);
    fail_unless(str_has_suffix("foobar", "xxfoobar") == 0);


    /* str_replace_set
     */
    char *repl = strdup("replacement string");
    fail_unless(str_replace_set(repl, "e ", '-') == repl);
    fail_unless(strcmp(repl, "r-plac-m-nt-string") == 0);
    free(repl);

	/* q_strcspn
	 */
	char qbuf[] = "one:two\\:two=three";
	char *p;

	p = qbuf;
	char *one = q_strsep(&p, ":");
	fail_unless(one == qbuf);
	fail_unless(strcmp(one, "one") == 0);
	fail_unless(p == qbuf + 4);
	fail_unless(strcmp(p, "two\\:two=three") == 0);

	char *twotwo = q_strsep(&p, ":=");
	fail_unless(twotwo);
	printf("twotwo == [%s]\n", twotwo);
	fail_unless(twotwo == qbuf + 4);
	fail_unless(strcmp(twotwo, "two:two") == 0);

	char *three = q_strsep(&p, ":=");
	fail_unless(three == qbuf + 13);
	fail_unless(strcmp(three, "three") == 0);

	fail_unless(p);
	fail_unless(*p == 0);

	char *four = q_strsep(&p, ":");
	fail_unless(four == p);
	fail_unless(q_strsep(&p, ":") == p);

	/* check multiple separators */
	char qbuf2[] = "five:::six";
	p = qbuf2;
	char *five = q_strsep(&p, ":");
	fail_unless(five == qbuf2);
	fail_unless(strcmp(five, "five") == 0);
	fail_unless(p == qbuf2 + 5);

	char *empty = q_strsep(&p, ":");
	fail_unless(empty == qbuf2 + 5);
	fail_unless(empty[0] == '\0');
	empty = q_strsep(&p, ":");
	fail_unless(empty == qbuf2 + 6);
	fail_unless(empty[0] == '\0');

	char *six = q_strsep(&p, ":");
	fail_unless(six);
	fail_unless(strcmp(six, "six") == 0);

	return 0;
}

#endif

