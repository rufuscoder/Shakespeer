/*
 * Copyright 2004-2006 Martin Hedenfalk <martin@bzero.se>
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

#include <stdio.h>
#include <string.h>

#include "dstring.h"
#include "log.h"

static int nmdc_char_need_quoting(char c)
{
    return (c == 0 || c == 5 ||
            c == 36 || c == 96 ||
            c == 124 || c == 126);
}

static char *nmdc_quote(char *str, int len)
{
    dstring_t *qstr = dstring_new(NULL);

    int i;
    for(i = 0; i < len; i++)
    {
        if(nmdc_char_need_quoting(str[i]))
        {
            dstring_append_format(qstr, "/%%DCN%03d%%/", str[i]);
        }
        else
        {
            dstring_append_char(qstr, str[i]);
        }
    }

    char *result = qstr->string;
    dstring_free(qstr, 0);

    return result;
}

char *nmdc_makelock_pk(const char *id, const char *version)
{
    char *lock_pk;
    int num_returned_bytes = asprintf(&lock_pk, "EXTENDEDPROTOCOLABCABCABCABCABCABC Pk=%s%sABCABC", id, version);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");

    return lock_pk;
}

char *nmdc_lock2key(const char *lock)
{
    int i;
    int len;
    char *key;
    char *qkey;

    len = strlen(lock);
    if(len < 3)
    {
        return strdup("");
    }

    key = malloc(len + 1);

    for(i = 1; i < len; i++)
        key[i] = lock[i] ^ lock[i-1];
    key[0] = lock[0] ^ lock[len-1] ^ lock[len-2] ^ 5;
    for(i = 0; i < len; i++)
        key[i] = ((key[i] << 4) & 0xF0) | ((key[i] >> 4) & 0x0F);

    qkey = nmdc_quote(key, len);
    free(key);
    
    return qkey;
}

char *nmdc_escape(const char *str)
{
    if(str == NULL)
        return NULL;

    dstring_t *esc = dstring_new(NULL);
    const char *s = str;
    while(*s)
    {
        if(*s == '|')
            dstring_append(esc, "&#124;");
        else if(*s == '$')
            dstring_append(esc, "&#36;");
        else
            dstring_append_char(esc, *s);
        ++s;
    }
    char *result = esc->string;
    dstring_free(esc, 0);
    return result;
}

char *nmdc_unescape(const char *str)
{
    static struct
    {
        char *esc;
        char ch;
    } esc_chars[] = {
        {"&#124;", '|'},
        {"&#36;", '$'},
        {"&amp;", '&'},
        {"&lt;", '<'},
        {"&gt;", '>'},
        {"&apos;", '\''},
        {"&quot;", '\"'},
        {NULL, 0}
    };

    if(str == NULL)
        return NULL;

    dstring_t *unesc = dstring_new(NULL);
    const char *s = str;
    while(*s)
    {
        char ins = *s;
        int i;
        for(i = 0; esc_chars[i].esc; i++)
        {
            int len = strlen(esc_chars[i].esc);
            if(strncmp(s, esc_chars[i].esc, len) == 0)
            {
                ins = esc_chars[i].ch;
                s += len - 1;
                break;
            }
        }
        dstring_append_char(unesc, ins);
        ++s;
    }

    char *result = unesc->string;
    dstring_free(unesc, 0);

    return result;
}

#ifdef TEST

#include "unit_test.h"

int main(void)
{
    /*
     * nmdc_escape & nmdc_unescape
     */
    const char *test_message = "foo|bar$baz";
    const char *test_message_escaped = "foo&#124;bar&#36;baz";
    char *result = nmdc_escape(test_message);
    fail_unless(result);
    fail_unless(strcmp(result, test_message_escaped) == 0);
    free(result);
    result = nmdc_unescape(test_message_escaped);
    fail_unless(result);
    fail_unless(strcmp(result, test_message) == 0);
    free(result);

    char *q = nmdc_quote("foo", 3);
    fail_unless(q);
    fail_unless(strcmp(q, "foo") == 0);
    free(q);

    q = nmdc_quote("foo\x05\x05""bar", 8);
    fail_unless(q);
    fail_unless(strcmp(q, "foo/%DCN005%//%DCN005%/bar") == 0);
    free(q);

    char *key = nmdc_lock2key("xx");
    fail_unless(key);
    fail_unless(*key == 0);
    free(key);

    key = nmdc_lock2key("xx");
    fail_unless(key);
    fail_unless(*key == 0);
    free(key);

    return 0;
}

#endif

