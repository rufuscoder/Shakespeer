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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "args.h"
#include "quote.h"
#include "xstr.h"

static void arg_add_token(arg_t *args, const char *token, unsigned int len)
{
    args->argv = (char **)realloc(args->argv, (args->argc + 1) * sizeof(char *));
    args->argv[args->argc] = len ? malloc(len + 1) : NULL;
    if(len)
    {
        strncpy(args->argv[args->argc], token, len);
        args->argv[args->argc][len] = 0;
    }
    args->argc++;
}

/* Splits str into space-separated arguments. Dynamically allocates a
 * string array.
 */
arg_t *arg_create_max(const char *str, const char *sep,
        int allow_null_fields, int max_args)
{
    const char *token = str;

    if(sep == NULL || str == NULL)
    {
        return NULL;
    }

    arg_t *args = calloc(1, sizeof(arg_t));

    if(!allow_null_fields)
    {
        token += strspn(token, sep); /* skip all initial separators */
    }

    while(token)
    {
        size_t len;
        if(max_args >= 0 && args->argc + 1 >= max_args)
        {
            len = strlen(token);
        }
        else
        {
            len = strcspn(token, sep);
        }

        if(len || allow_null_fields)
        {
            arg_add_token(args, token, len);
            token += len;
        }

        if(*token == 0)
        {
            break;
        }

        if(allow_null_fields)
        {
            ++token; /* skip one separator */
        }
        else
        {
            token += strspn(token, sep); /* skip all separators */
        }
    }

    return args;
}

arg_t *arg_create(const char *str, const char *sep, int allow_null_fields)
{
    return arg_create_max(str, sep, allow_null_fields, -1);
}

arg_t *arg_create_from_argv(int argc, char **argv)
{
    arg_t *args = calloc(1, sizeof(arg_t));
    int i;
    for(i = 0; i < argc; i++)
    {
        arg_add_token(args, argv[i], strlen(argv[i]));
    }
    return args;
}

arg_t *arg_dup(const arg_t *arg)
{
    return arg_create_from_argv(arg->argc, arg->argv);
}

int arg_find_token_length(const char *token)
{
    static const char *blank = " \t\n";
    static const char *blank_or_quote = " \t\n'\"\\";
    size_t total_len = 0;

    while(*token)
    {
        size_t len = 0;

        if(strchr(blank, *token) != NULL)
        {
            break;
        }
        else if(*token == '\\')
        {
            if(token[1])
                len = 2;
            else
                return total_len + 1;
        }
        else if(*token == '\'')
        {
            len = strcspn(token + 1, "\'"); /* find next ' */
            len += 2;
        }
        else if(*token == '\"')
        {
            len = strcspn(token + 1, "\""); /* find next " */
            len += 2;
        }
        else
        {
            len += strcspn(token, blank_or_quote);
        }

        token += len;
        total_len += len;
    }

    return total_len;
}

arg_t *arg_create_quoted(const char *str)
{
    const char *sep = " \t\n";
    const char *token = str;
    arg_t *args = calloc(1, sizeof(arg_t));

    while(token && *token)
    {
        token += strspn(token, sep); /* skip all initial token separators */

        size_t len = arg_find_token_length(token);

        if(len)
        {
            args->argv = (char **)realloc(args->argv, (args->argc + 1) * sizeof(char *));
            args->argv[args->argc++] = str_unquote(token, len);
            token += len;
        }
    }

    return args;
}

void arg_free(arg_t *args)
{
    int i;

    if(args)
    {
        if(args->argv)
        {
            for(i = 0; i < args->argc; i++)
            {
                free(args->argv[i]);
            }
            free(args->argv);
        }
        free(args);
    }
}

char *arg_join(arg_t *args, int first, int last, const char *sep)
{
    int i;
    char *e;
    size_t s = 0;

    if(last == -1 || last > args->argc)
    {
        last = args->argc;
    }

    for(i = first; i < last; i++)
    {
        s += strlen(args->argv[i]) + strlen(sep);
    }

    e = (char *)malloc(s + 1);
    memset(e, 0, s);

    for(i = first; i < last; i++)
    {
        strlcat(e, args->argv[i], s + 1);
        if(i+1 < last)
        {
            strlcat(e, sep, s + 1);
        }
    }
    return e;
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
    arg_t *args;

    args = arg_create("foo:bar", ":", 0);
    fail_unless(args);
    fail_unless(args->argc == 2);
    fail_unless(args->argv);
    fail_unless(args->argv[0]);
    fail_unless(args->argv[1]);
    fail_unless(strcmp(args->argv[0], "foo") == 0);
    fail_unless(strcmp(args->argv[1], "bar") == 0);
    arg_free(args);

    args = arg_create("foo:bar", ":", 1);
    fail_unless(args);
    fail_unless(args->argc == 2);
    fail_unless(args->argv);
    fail_unless(args->argv[0]);
    fail_unless(args->argv[1]);
    fail_unless(strcmp(args->argv[0], "foo") == 0);
    fail_unless(strcmp(args->argv[1], "bar") == 0);
    arg_free(args);

    args = arg_create("foo::bar:", ":", 1);
    fail_unless(args);
    fail_unless(args->argc == 4);
    fail_unless(args->argv);
    fail_unless(args->argv[0]);
    fail_unless(args->argv[1] == NULL);
    fail_unless(args->argv[2]);
    fail_unless(args->argv[3] == NULL);
    fail_unless(strcmp(args->argv[0], "foo") == 0);
    fail_unless(strcmp(args->argv[2], "bar") == 0);
    arg_free(args);

    args = arg_create("foo::bar:", ":", 0);
    fail_unless(args);
    fail_unless(args->argc == 2);
    fail_unless(args->argv);
    fail_unless(args->argv[0]);
    fail_unless(args->argv[1]);
    fail_unless(strcmp(args->argv[0], "foo") == 0);
    fail_unless(strcmp(args->argv[1], "bar") == 0);
    arg_free(args);

    args = arg_create("/foo:-bar/", ":/-", 1);
    fail_unless(args);
    fail_unless(args->argc == 5);
    fail_unless(args->argv);
    fail_unless(args->argv[0] == NULL);
    fail_unless(args->argv[1]);
    fail_unless(args->argv[2] == NULL);
    fail_unless(args->argv[3]);
    fail_unless(args->argv[4] == NULL);
    fail_unless(strcmp(args->argv[1], "foo") == 0);
    fail_unless(strcmp(args->argv[3], "bar") == 0);
    arg_free(args);

    args = arg_create("", ":", 0);
    fail_unless(args);
    fail_unless(args->argc == 0);
    fail_unless(args->argv == NULL);
    arg_free(args);

    args = arg_create("", ":", 1);
    fail_unless(args);
    fail_unless(args->argc == 1);
    fail_unless(args->argv);
    fail_unless(args->argv[0] == NULL);
    arg_free(args);

    args = arg_create(NULL, ":", 0);
    fail_unless(args == NULL);

    args = arg_create("foo:bar", "", 0);
    fail_unless(args);
    fail_unless(args->argc == 1);
    fail_unless(args->argv);
    fail_unless(args->argv[0]);
    fail_unless(strcmp(args->argv[0], "foo:bar") == 0);
    arg_free(args);

    /* test arg_create_max */

    args = arg_create_max("foo:bar:baz:", ":", 1, 1);
    fail_unless(args);
    fail_unless(args->argc == 1);
    fail_unless(args->argv);
    fail_unless(args->argv[0]);
    fail_unless(strcmp(args->argv[0], "foo:bar:baz:") == 0);
    arg_free(args);

    args = arg_create_max("foo:bar:baz:", ":", 1, 2);
    fail_unless(args);
    fail_unless(args->argc == 2);
    fail_unless(args->argv);
    fail_unless(args->argv[0]);
    fail_unless(args->argv[1]);
    fail_unless(strcmp(args->argv[0], "foo") == 0);
    fail_unless(strcmp(args->argv[1], "bar:baz:") == 0);
    arg_free(args);

    args = arg_create_max("foo:bar:baz:", ":", 1, 3);
    fail_unless(args);
    fail_unless(args->argc == 3);
    fail_unless(args->argv);
    fail_unless(args->argv[0]);
    fail_unless(args->argv[1]);
    fail_unless(args->argv[2]);
    fail_unless(strcmp(args->argv[0], "foo") == 0);
    fail_unless(strcmp(args->argv[1], "bar") == 0);
    fail_unless(strcmp(args->argv[2], "baz:") == 0);
    arg_free(args);

    args = arg_create_max("foo:bar:baz:", ":", 1, 17);
    fail_unless(args);
    fail_unless(args->argc == 4);
    fail_unless(args->argv);
    fail_unless(args->argv[0]);
    fail_unless(args->argv[1]);
    fail_unless(args->argv[2]);
    fail_unless(args->argv[3] == NULL);
    fail_unless(strcmp(args->argv[0], "foo") == 0);
    fail_unless(strcmp(args->argv[1], "bar") == 0);
    fail_unless(strcmp(args->argv[2], "baz") == 0);
    arg_free(args);

    /* the following test should return NULL */
    args = arg_create("foo:bar", NULL, 0);
    fail_unless(args == NULL);

    /* test arg_create_quoted */
    args = arg_create_quoted("this\\ is   \"a\"\t\t\n'quoted string' ");
    fail_unless(args);
    fail_unless(args->argc == 3);
    fail_unless(args->argv[0]);
    fail_unless(args->argv[1]);
    fail_unless(args->argv[2]);
    fail_unless(strcmp(args->argv[0], "this is") == 0);
    fail_unless(strcmp(args->argv[1], "a") == 0);
    fail_unless(strcmp(args->argv[2], "quoted string") == 0);

    return 0;
}

#endif

