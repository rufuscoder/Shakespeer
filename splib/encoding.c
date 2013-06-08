/*
 * Copyright 2005 Martin Hedenfalk <martin@bzero.se>
 *
 * This file is part of ShakesPeer.
 *
 * ShakesPeer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ShakesPeer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ShakesPeer; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/types.h>
#include <netinet/in.h>

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "nfkc.h"
#include "iconv_string.h"
#include "log.h"
#include "encoding.h"
#include "dstring.h"

static int xdigit_value(char c)
{
    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if(c >= '0' && c <= '9')
        return c - '0';
    return -1;
}

#define get_ucs4_16(p) \
    ntohs( \
            (xdigit_value(p[0]) << 12) + \
            (xdigit_value(p[1]) << 8) + \
            (xdigit_value(p[2]) << 4) + \
            (xdigit_value(p[3])))

#define get_ucs4_32(p) \
    ntohl( \
            (xdigit_value(p[0]) << 28) + \
            (xdigit_value(p[1]) << 24) + \
            (xdigit_value(p[2]) << 20) + \
            (xdigit_value(p[3]) << 16) + \
            (xdigit_value(p[4]) << 12) + \
            (xdigit_value(p[5]) << 8) + \
            (xdigit_value(p[6]) << 4) + \
            (xdigit_value(p[7])))

int str_need_unescape_unicode(const char *utf8_string)
{
    if(utf8_string == NULL)
        return 0;

    const char *bs = utf8_string;
    do
    {
        bs = strchr(bs, '\\');
        if(bs && (bs[1] == 'u' || bs[1] == 'U'))
        {
            return 1;
        }
    } while(bs++ && *bs);

    return 0;
}

static int is_hex_string(const char *p, int len)
{
    if(p == 0)
        return 0;

    int i;
    for(i = 0; p && *p && (len == -1 || i < len); p++, i++)
    {
        if(!isxdigit(*p))
        {
            return 0;
        }
    }
    return ((len == -1) || (i == len)) ? 1 : 0;
}

/* Converts \uxxxx or \Uxxxxyyyy escapes into their UTF-8 equivalents.
 * Returns a newly allocated string. The input string must be valid utf-8.
 */
char *str_unescape_unicode(const char *utf8_string)
{
    if(utf8_string == NULL)
        return NULL;

    dstring_t *unescaped = dstring_new(NULL);
    const char *p = utf8_string;
    while(p && *p)
    {
        gunichar uc = g_utf8_get_char(p);
        p = g_utf8_next_char(p);
        if(uc == '\\')
        {
            if(*p == 'u' && is_hex_string(p+1, 4))
            {
                p++;
                uc = htons(get_ucs4_16(p));
                p += 4;
            }
            else if(*p == 'U' && is_hex_string(p+1, 8))
            {
                p++;
                uc = htonl(get_ucs4_32(p));
                p += 8;
            }

            if(!g_unichar_validate(uc))
            {
                continue;
            }
        }

        char utf8_buf[6];
        int utf8_len = g_unichar_to_utf8(uc, utf8_buf);
        dstring_append_len(unescaped, utf8_buf, utf8_len);
    }
    char *result = unescaped->string;
    dstring_free(unescaped, 0);

    return result;
}

/* Convert a string in legacy encoding to (decomposed?) UTF-8.  Returns NULL if
 * conversion failed.
 */
static char *str_legacy_to_utf8_internal(const char *string,
        const char *legacy_encoding, int replacement_char)
{
    if(string == NULL || legacy_encoding == NULL)
    {
        return NULL;
    }

    if(g_utf8_validate(string, -1, NULL))
    {
        return strdup(string);
    }

    char *utf8_string = iconv_string_full(string, -1,
            legacy_encoding, "UTF-8", NULL, NULL, replacement_char);

    return utf8_string;
}

char *str_legacy_to_utf8(const char *string, const char *legacy_encoding)
{
    return str_legacy_to_utf8_internal(string, legacy_encoding, -1);
}

/* Convert a string in legacy encoding to (decomposed?) UTF-8 replacing illegal
 * characters with a '?'. Returns NULL if conversion fails, otherwise a newly
 * allocated string.
 */
char *str_legacy_to_utf8_lossy(const char *string, const char *legacy_encoding)
{
    return str_legacy_to_utf8_internal(string, legacy_encoding, '?');
}

char *str_convert_to_unescaped_utf8(const char *string, const char *legacy_encoding)
{
    if(string == NULL || legacy_encoding == NULL)
        return NULL;

    char *utf8_string = str_legacy_to_utf8(string, legacy_encoding);
    if(utf8_string)
    {
        if(str_need_unescape_unicode(utf8_string))
        {
            char *unescaped_utf8 = str_unescape_unicode(utf8_string);
            free(utf8_string);
            return unescaped_utf8;
        }
        else
            return utf8_string;
    }

    return NULL;
}

char *str_convert_to_escaped_utf8(const char *string, const char *legacy_encoding)
{
    if(string == NULL || legacy_encoding == NULL)
        return NULL;

    char *utf8_string = str_legacy_to_utf8(string, legacy_encoding);
    if(utf8_string == NULL)
        return NULL;

    char *legacy_fallback = str_utf8_to_escaped_legacy(utf8_string,
            legacy_encoding);
    free(utf8_string);
    if(legacy_fallback == NULL)
        return NULL;

    utf8_string = str_legacy_to_utf8(legacy_fallback, legacy_encoding);
    free(legacy_fallback);

    return utf8_string;
}

/* Convert a string in UTF-8 to <legacy_encoding>.  Returns NULL if conversion
 * failed.
 */
char *str_utf8_to_legacy(const char *string, const char *legacy_encoding)
{
    if(string == NULL || legacy_encoding == NULL)
        return NULL;

    /* convert the (possibly) decomposed utf-8 string to composed form (eg,
     * &Auml; is converted to a single precomposed character instead of a base
     * character with a combining accent). This is required for the following
     * conversion to Windows-1252 legacy_encoding.
     */
    char *utf8_composed_string = g_utf8_normalize(string, -1, G_NORMALIZE_DEFAULT_COMPOSE);
    if(utf8_composed_string == NULL)
    {
        return NULL;
    }

    char *legacy_string = iconv_string(utf8_composed_string, -1,
            "UTF-8", legacy_encoding);
    free(utf8_composed_string);

    return legacy_string;
}

/* Convert a string in UTF-8 to legacy encoding, escaping those characters that
 * fail to convert.
 */
char *str_utf8_to_escaped_legacy(const char *string,
        const char *legacy_encoding)
{
    if(string == NULL || legacy_encoding == NULL)
    {
        return NULL;
    }

    /* Convert the (possibly) decomposed UTF-8 string to composed form (eg,
     * &Auml; is converted to a single precomposed character instead of a base
     * character with a combining accent). This is required for the following
     * conversion to legacy (Windows-1252) encoding.
     */
    char *utf8_composed_string = g_utf8_normalize(string, -1, G_NORMALIZE_DEFAULT_COMPOSE);
    if(utf8_composed_string == NULL)
    {
        WARNING("input string not valid UTF-8");
        return NULL;
    }

    char *legacy_string = iconv_string_escaped(utf8_composed_string, -1,
            "UTF-8", legacy_encoding);

    free(utf8_composed_string);

    return legacy_string;
}

#ifdef TEST

# include <stdio.h>

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
    sp_log_set_level("debug");

    /*
     * str_legacy_to_utf8_lossy
     */
    char *w = str_legacy_to_utf8_lossy("ascii", "MS-ANSI");
    fail_unless(w);
    fail_unless(strcmp(w, "ascii") == 0);

#if 1
    w = str_legacy_to_utf8_lossy("abc \x40, should fail: \x81 \x8D \x8F \x9D \x9D", "MS-ANSI");
    fail_unless(w);
    fail_unless(strcmp(w, "abc @, should fail: ? ? ? ? ?") == 0);
#endif

    /*
     * str_utf8_to_legacy
     */

    /* See http://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1252.TXT
     */

    /* this is "aring auml ouml" in composed UTF-8  */
    w = str_utf8_to_legacy("\xc3\xa5\xc3\xa4\xc3\xb6", "WINDOWS-1252");
    fail_unless(w);
    fail_unless(strcmp(w, "\xe5\xe4\xf6") == 0);
    free(w);

    /* this is "aring auml ouml" in de-composed UTF-8  */
    const char *utf8_decomposed_string = "a\xcc\x8a""a\xcc\x88""o\xcc\x88";
    fail_unless(strlen(utf8_decomposed_string) == 9);
    w = str_utf8_to_legacy(utf8_decomposed_string, "WINDOWS-1252");
    fail_unless(w);
    fail_unless(strcmp(w, "\xe5\xe4\xf6") == 0);
    free(w);

    /*
     * str_unescape_unicode
     */
    w = str_unescape_unicode("\\U000000e5\\u00e4\\U000000F6");
    fail_unless(w);
    fail_unless(strcmp(w, "\xc3\xa5\xc3\xa4\xc3\xb6") == 0);
    free(w);




    const char *user_command = "\xa8\xb0\xba\xa4\xf8";
    printf("user_command = [%s]\n", user_command);
    char *c1 = str_legacy_to_utf8(user_command, "WINDOWS-1252");
    fail_unless(c1);
    printf("converted string = [%s]\n", c1);

    const char *c1_utf8 = "\xc2\xa8\xc2\xb0\xc2\xba\xc2\xa4\xc3\xb8";
    fail_unless(strcmp(c1, c1_utf8) == 0);

    char *c2 = str_utf8_to_escaped_legacy(c1_utf8, "WINDOWS-1252");
    fail_unless(c2);
    printf("converted string = [%s]\n", c2);

    fail_unless(strcmp(c2, user_command) == 0);
    
    free(c1);
    free(c2);

    return 0;
}

#endif

