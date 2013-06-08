/*
 * Copyright 2004-2005 Martin Hedenfalk <martin@bzero.se>
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

#include <ctype.h>
#include <locale.h>
#include <stdio.h>

#include "shakespeer.h"
#include "quote.h"
#include "xstr.h"
#include "log.h"

extern sp_t *sp;

static GetLine *gl;              /* The line editor */

static ui_cmd_t cmds[] = {
    {CTX_ALL, "hashprio", 1, func_set_hash_prio, cpl_none, "set hashing priority (1-5)"},
    {CTX_ALL, "debug", 1, func_debug, cpl_none, "change debug level"},
    {CTX_ALL, "hublist", 0, func_hublist, cpl_none, "enter hublist context"},
    {CTX_ALL, "qls", 0, func_queue_ls, cpl_none, "list download queue"},
    {CTX_ALL, "qrm", 1, func_queue_remove, cpl_none, "remove a file from the download queue"},
    {CTX_ALL, "qrmsource", 2, func_queue_remove_source, cpl_none, "remove a source for a file from the download queue"},
    {CTX_ALL, "qrmnick", 1, func_queue_remove_nick, cpl_none, "remove all files for a user from the download queue"},
    {CTX_ALL, "qrmfilelist", 1, func_queue_remove_filelist, cpl_none, "remove a request for a filelist from the download queue"},
    {CTX_ALL, "cancel", 1, func_cancel_transfer, cpl_transfer, "cancel an ongoing transfer"},
    {CTX_ALL, "port", 1, func_set_port, cpl_none, "change port for client connections and search results"},
    {CTX_ALL, "addpath", 1, func_add_shared_path, cpl_local_filename, "add a path to your share"},
    {CTX_ALL, "rmpath", 1, func_remove_shared_path, cpl_none, "remove a path from your share"},
    {CTX_ALL, "help", 1, func_help, cpl_command, "DON'T PANIC"},
    {CTX_ALL, "info", 0, func_info, cpl_none, "show some info"},
    {CTX_ALL, "passive", 0, func_set_passive, cpl_none, "toggle passive mode"},

    {CTX_MAIN, "connect", 1, func_connect, cpl_none, "connect to a hub"},
    {CTX_MAIN, "open", 1, func_connect, cpl_none, "connect to a hub"},
    {CTX_MAIN, "exit", 0, func_exit, cpl_none, "exit from ShakesPeer (any connected hubs are not disconnected)"},
    {CTX_MAIN, "hub", 0, func_hub, cpl_none, "enter the hub context"},
    {CTX_MAIN, "ls", 0, func_ls, cpl_none, "list connected hubs"},

    {CTX_HUB, "disconnect", 0, func_hub_disconnect, cpl_none, "disconnect from the current hub"},
    {CTX_HUB, "close", 0, func_hub_disconnect, cpl_none, "disconnect from the current hub"},
    {CTX_HUB, "exit", 0, func_hub_exit, cpl_none, "exit from the hub context (the hub is still connected)"},
    {CTX_HUB, "who", 0, func_hub_list_users, cpl_none, "list users on this hub"},
    {CTX_HUB, "search", 1, func_hub_search, cpl_none, "search for a file"},
    {CTX_HUB, "pm", 2, func_hub_msg, cpl_none, "send a private message to another user"},
    {CTX_HUB, "msg", 1, func_hub_hmsg, cpl_none, "send a public message to the hub"},
    {CTX_HUB, "browse", 1, func_hub_browse, cpl_nick, "download a filelist from a user"},
    {CTX_HUB, "sresults", 0, func_hub_search_results, cpl_none, "show search results from last search"},
    {CTX_HUB, "sget", 1, func_hub_sget, cpl_none, "download a file from the last search results"},
    {CTX_HUB, "lfilelists", 0, func_hub_lfilelists, cpl_none, "list all downloaded filelists"},
    {CTX_HUB, "rfilelist", 1, func_hub_rfilelist, cpl_none, "read a saved filelist into memory"},
    /* {CTX_HUB, "queue", 0, func_hub_queue, cpl_none}, */
    {CTX_HUB, "password", 1, func_hub_set_password, cpl_none, "set the password for the hub"},

    {CTX_FILELIST, "ls", 0, func_filelist_ls, cpl_filelist, "list files in the current directory"},
    {CTX_FILELIST, "lsdirs", 0, func_filelist_lsdirs, cpl_filelist_dir, "list only subdirectories in the current directory"},
    {CTX_FILELIST, "cd", 1, func_filelist_cd, cpl_filelist_dir, "change the current directory"},
    {CTX_FILELIST, "exit", 0, func_filelist_exit, cpl_none, "exit from the filelist context"},
    {CTX_FILELIST, "get", 1, func_filelist_get, cpl_filelist, "download a file from the filelist"},
    {CTX_FILELIST, "getdir", 1, func_filelist_get_directory, cpl_filelist_dir, "download a directory from the filelist"},
    {CTX_FILELIST, "search", 1, func_filelist_search, cpl_none, "search within this filelist"},

    /* {CTX_QUEUE, "refresh", 0, func_queue_refresh, cpl_none}, */
    /* {CTX_QUEUE, "ls", 0, func_queue_ls, cpl_none}, */
    /* {CTX_QUEUE, "rm", 1, func_queue_remove, cpl_none}, */
    /* {CTX_QUEUE, "rmnick", 1, func_queue_remove_nick, cpl_none}, */
    /* {CTX_QUEUE, "exit", 0, func_queue_exit, cpl_none}, */

    {CTX_HUBLIST, "refresh", 0, func_hublist_refresh, cpl_none, "refresh the list of hubs"},
    {CTX_HUBLIST, "ls", 0, func_hublist_ls, cpl_none, "list all public hubs"},
    {CTX_HUBLIST, "filter", 0, func_hublist_filter, cpl_none, "set a filter for the 'ls' command"},
    {CTX_HUBLIST, "exit", 0, func_hublist_exit, cpl_none, "exit the hublist context"},
    {CTX_HUBLIST, "connect", 1, func_connect, cpl_hublist, "connect to a hub"},
    {CTX_HUBLIST, "open", 1, func_connect, cpl_hublist, "connect to a hub"},

    {0, 0, 0, NULL, cpl_none, NULL} /* end marker */
};

ui_cmd_t *get_cmd(const char *name)
{
    int i;
    ui_cmd_t *cmd = 0;

    for(i = 0; cmds[i].name; ++i)
    {
        if(cmds[i].context == context || cmds[i].context == CTX_ALL)
        {
            if(str_has_prefix(cmds[i].name, name))
            {
                /* is this a complete command? */
                if(strcmp(cmds[i].name, name) == 0)
                {
                    return &cmds[i];
                }

                if(cmd)
                {
                    /* found ambigous command */
                    return (ui_cmd_t *)-1;
                }
                cmd = &cmds[i];
            }
        }
    }
    return cmd;
}

static int cmd_complete_commands(WordCompletion *cpl, const char *line, int word_start, int word_end)
{
    int cmd_index = 0;
    int len = word_end - word_start;

    char *e;
    while((e = cmds[cmd_index].name) != 0)
    {
        ui_cmd_t *cmd = &cmds[cmd_index];
        cmd_index++;
        if((cmd->context == context || cmd->context == CTX_ALL) &&
                strncasecmp(e, line + word_start, len) == 0)
        {
            cpl_add_completion(cpl, line, word_start, word_end, e + len, "", " ");
        }
    }

    return 0;
}

static int cmd_complete_nick(WordCompletion *cpl, const char *line,
        int word_start, int word_end)
{
    int len = word_end - word_start;

    char *partial = xstrndup(line + word_start, len);

    sp_user_t *user;
    LIST_FOREACH(user, &current_hub->users, link)
    {
        if(str_has_prefix(user->nick, partial))
        {
            cpl_add_completion(cpl, line,
                    word_start, word_end, user->nick + len, "", " ");
        }
    }
    free(partial);

    return 0;
}

static int cmd_complete_transfers(WordCompletion *cpl, const char *line,
        int word_start, int word_end)
{
    int len = word_end - word_start;
    char *partial = xstrndup(line + word_start, len);

    transfer_t *tr;
    LIST_FOREACH(tr, &transfers, link)
    {
        if(tr && str_has_prefix(tr->local_filename, partial))
        {
            char *q = str_quote_backslash(tr->local_filename + len, " \t\r\n");
            cpl_add_completion(cpl, line, word_start, word_end, q, NULL, " ");
            free(q);
        }
    }
    free(partial);

    return 0;
}

static CPL_MATCH_FN(cmd_completion_cb)
{
    cpl_t cpltype = cpl_command;

    const char *e = str_find_word_start(line, line + word_end, " ");
    int word_start = e - line;

    if(word_start > 0)
    {
        e = strchr(line, ' ');
        char *cmdname = xstrndup(line, e - line);
        ui_cmd_t *cmd = get_cmd(cmdname);
        if(cmd && cmd != (ui_cmd_t *)-1)
        {
            cpltype = cmd->cpl_type;
        }
        free(cmdname);
    }

    switch(cpltype)
    {
        case cpl_command:
            return cmd_complete_commands(cpl, line, word_start, word_end);
            break;
        case cpl_nick:
            return cmd_complete_nick(cpl, line, word_start, word_end);
            break;
        case cpl_transfer:
            return cmd_complete_transfers(cpl, line, word_start, word_end);
            break;
        case cpl_local_filename:
            return cpl_file_completions(cpl, data, line, word_end);
            break;
        case cpl_filelist:
            return cmd_complete_filelist(cpl, line, word_start, word_end);
            break;
        case cpl_filelist_dir:
            return cmd_complete_filelist_directories(cpl, line, word_start, word_end);
            break;
        case cpl_hublist:
            return cmd_complete_hublist(cpl, line, word_start, word_end);
            break;
        case cpl_filelists:
        case cpl_none:
            break;
    }

    return 0;
}

static void cmd_dispatch(char *cmdline, void *user_data)
{
    sp_t *sp = user_data;
    arg_t *args = arg_create_quoted(cmdline);
    if(!args)
        return;
    ui_cmd_t *cmd = get_cmd(args->argv[0]);
    if(cmd)
    {
        if(cmd == (ui_cmd_t *)-1)
        {
            msg("ambigous command: %s", args->argv[0]);
        }
        else
        {
            if(args->argc - 1 >= cmd->minargs)
            {
                if(cmd->func(sp, args) != 0)
                {
                    arg_free(args);
                    return;
                }
            }
            else
            {
                msg("function '%s' requires at least %d arguments", cmd->name, cmd->minargs);
            }
        }
    }
    else
    {
        msg("unknown command: %s", args->argv[0]);
    }
    arg_free(args);
}

static char *cmd_set_prompt(void)
{
    char *prompt = 0;
    switch(context)
    {
        case CTX_MAIN:
            asprintf(&prompt, "ShakesPeer$ ");
            break;
        case CTX_HUB:
            asprintf(&prompt, "ShakesPeer:%s@%s$ ",
                    current_hub->nick, current_hub->address);
            break;
        case CTX_FILELIST:
            asprintf(&prompt, "ShakesPeer:browse(%s) \\%s$ ",
                    current_filelist->nick,
                    filelist_cdir ? filelist_cdir->path : "");
            break;
        case CTX_HUBLIST:
            prompt = strdup("ShakesPeer:hublist$ ");
            break;
        case CTX_QUEUE:
            prompt = strdup("ShakesPeer:queue$ ");
            break;
        default:
            prompt = strdup("$ ");
            break;
    }

    return prompt;
}

static int cmd_get_line(void *user_data)
{
    static char *prompt = 0;
    static int ctx = -1;

    if(prompt == 0)
    {
        prompt = cmd_set_prompt();
    }
    else if(context != ctx)
    {
        free(prompt);
        prompt = cmd_set_prompt();
        gl_replace_prompt(gl, prompt);
    }
    ctx = context;
    
    char *buf = gl_get_line(gl, prompt, NULL, 0);
    if(gl_return_status(gl) == GLR_BLOCKED)
        return 1;

    if(buf == NULL)
        buf = "exit";
    else
        str_trim_end_inplace(buf, NULL);

    if(*buf)
    {
        cmd_dispatch(buf, user_data);
        free(prompt);
        prompt = 0;
    }

    return 1;
}

void cmd_init(void)
{
    int major, minor, micro;  /* The version number of the library */

    gl = new_GetLine(500, 5000);
    if(gl)
    {
        setlocale(LC_CTYPE, "");

        libtecla_version(&major, &minor, &micro);
        printf("using tecla library version %d.%d.%d\n", major, minor, micro);

        gl_load_history(gl, "~/.shakespeer_history", "#");

        gl_customize_completion(gl, NULL, cmd_completion_cb);
        gl_prompt_style(gl, GL_FORMAT_PROMPT);
    }
}

void cmd_add_watch(int fd, int (*cmd_callback)(void *gl, void *data,
            int fd, int event), void *user_data)
{
    if(gl_watch_fd(gl, fd, GLFD_READ, (GlFdEventFn *)cmd_callback, user_data) != 0)
    {
        g_warning("failed to add file descriptor to event loop");
    }
}

void cmd_run_loop(void *user_data)
{
    while(1)
    {
        cmd_get_line(user_data);
    }
}

void cmd_fini(void)
{
    gl_save_history(gl, "~/.shakespeer_history", "#", -1);
    gl = del_GetLine(gl);
}

void cmd_normal_io(void)
{
    gl_normal_io(gl);
}

void cmd_raw_io(void)
{
    gl_raw_io(gl);
}

void cmd_display_text(const char *str)
{
    gl_display_message(gl, str);
}

