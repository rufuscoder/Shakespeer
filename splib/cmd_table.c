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
#include <string.h>

#include "args.h"
#include "cmd_table.h"

static cmd_t *cmd_find(const char *cmdline, int len, cmd_t *cmds)
{
    int i;

    if(cmdline == NULL || cmds == NULL)
        return NULL;

    for(i = 0; cmds[i].name; i++)
    {
        if(strncmp(cmdline, cmds[i].name, len) == 0 && len == strlen(cmds[i].name))
            return &cmds[i];
    }

    return NULL;
}

int cmd_dispatch(const char *cmdline,
        const char *delimiters,
        int allow_null_elements,
        cmd_t *command_table,
        void *user_data)
{
    if(cmdline == NULL || delimiters == NULL)
        return -1;

    int cmdlen = strcspn(cmdline, delimiters);
    if(cmdlen == 0)
    {
        /* got empty command */
        return 0;
    }

    cmd_t *cmd = cmd_find(cmdline, cmdlen, command_table);
    if(cmd == 0)
    {
        /* unknown command */
        return 0;
    }

    int max_args = -1;
    if(cmd->required_arguments < 0)
    {
        max_args = -cmd->required_arguments;
    }

    /* skip past the command name + one delimiter */
    cmdline += cmdlen;
    if(*cmdline)
    {
        cmdline++;
    }

    arg_t *args;
    if(*cmdline)
    {
        args = arg_create_max(cmdline, delimiters,
                allow_null_elements, max_args);
    }
    else
    {
        args = arg_create_from_argv(0, NULL);
    }
    if(args == NULL)
    {
        /*WARNING("unable to split arguments in [%s] by [%s], max %i",
                cmdline, delimiters, max_args);*/
        return -1;
    }

    int req_args = cmd->required_arguments;
    if(req_args < 0)
    {
        req_args = -req_args;
    }

    if(args->argc < req_args)
    {
        /*WARNING("command '%s' requires %i arguments, only %i given: [%s]",
                cmd->name, req_args, args->argc, cmdline);*/
        arg_free(args);
        return 0;
    }

    int rc = cmd->func(user_data, args->argc, args->argv);

    arg_free(args);

    return rc;
}


#ifdef TEST

#include <stdio.h>
#include <stdlib.h>

#define fail_unless(test) \
    do { if(!(test)) { \
        fprintf(stderr, \
                "----------------------------------------------\n" \
                "%s:%d: test FAILED: %s\n" \
                "----------------------------------------------\n", \
                __FILE__, __LINE__, #test); \
        exit(1); \
    } } while(0)

int expected_args = -1;

int cmd_test_cb(void *user_data, int argc, char **argv)
{
    fail_unless(argc == expected_args);
    fail_unless(user_data == (void *)0xDEADBEEF);
    fail_unless(strcmp(argv[0], "foobar") == 0);

    return 17;
}

int cmd_test_cb2(void *user_data, int argc, char **argv)
{
    fail_unless(argc == expected_args);
    fail_unless(user_data == (void *)0xDEADBEEF);
    fail_unless(strcmp(argv[0], "foo bar") == 0);

    return 42;
}

int main(void)
{
    cmd_t cmds[] = {
        {"test", cmd_test_cb, 1},
        {"test2", cmd_test_cb2, -1},
        {0, 0, -1}
    };

    int rc;

    rc = cmd_dispatch(NULL, " ", 0, cmds, (void *)0xDEADBEEF);
    fail_unless(rc == -1);

    rc = cmd_dispatch("foo", NULL, 0, cmds, (void *)0xDEADBEEF);
    fail_unless(rc == -1);

    expected_args = 1;
    rc = cmd_dispatch("test foobar", " ", 0, cmds, (void *)0xDEADBEEF);
    fail_unless(rc == 17);

    expected_args = 0;
    rc = cmd_dispatch("test", " ", 0, cmds, (void *)0xDEADBEEF);
    fail_unless(rc == 0);

    expected_args = -1;
    rc = cmd_dispatch("fail foobar", " ", 0, cmds, (void *)0xDEADBEEF);
    fail_unless(rc == 0);

    expected_args = 1;
    rc = cmd_dispatch("test2 foo bar", " ", 0, cmds, (void *)0xDEADBEEF);
    fail_unless(rc == 42);

    rc = cmd_dispatch("t foobar", " ", 0, cmds, (void *)0xDEADBEEF);
    fail_unless(rc == 0);

    return 0;
}

#endif

