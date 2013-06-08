/*
 * Copyright 2004 Martin Hedenfalk <martin@bzero.se>
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

#include "shakespeer.h"
#include "xstr.h"

int func_hub_search(sp_t *sp, arg_t *args)
{
    char *words = arg_join(args, 1, -1, " ");
    /* g_list_free(current_hub->search_results); */
    /* current_hub->search_results = 0; */
    sp_send_search_all(sp, words, 0L, SHARE_SIZE_NONE, SHARE_TYPE_ANY, 17);
    free(words);
    return 0;
}

int func_hub_msg(sp_t *sp, arg_t *args)
{
    char *msg = arg_join(args, 2, -1, " ");
    sp_send_private_message(sp, current_hub->address, args->argv[1], msg);
    free(msg);
    return 0;
}

int func_hub_hmsg(sp_t *sp, arg_t *args)
{
    char *msg = arg_join(args, 1, -1, " ");
    sp_send_public_message(sp, current_hub->address, msg);
    free(msg);
    return 0;
}

int func_hub_browse(sp_t *sp, arg_t *args)
{
    const char *nick = args->argv[1];
    sp_filelist_t *fl = fl_lookup_nick(nick);
    if(fl == 0)
    {
        sp_send_download_filelist(sp, current_hub->address, nick, 1, 0);
    }
    else
    {
        filelist_cdir = fl->root;
        current_filelist = fl;
        sort_filelist(fl);
        context = CTX_FILELIST;
    }

    return 0;
}

int func_hub_search_results(sp_t *sp, arg_t *args)
{
    int i = 1;

    sr_t *sr;
    LIST_FOREACH(sr, &current_hub->search_results, link)
    {
        msg("%d: %-15s  %s (%s)",
                i++, sr->nick, sr->filename, str_size_human(sr->size));
    }
    
    return 0;
}

int func_hub_sget(sp_t *sp, arg_t *args)
{
    sr_t *sr;
    int n = atoi(args->argv[1]);
    int i = 0;
    LIST_FOREACH(sr, &current_hub->search_results, link)
    {
        if(++i == n)
            break;
    }

    if(sr == 0)
    {
        msg("search result %d doesn't exist", n);
        return 0;
    }

    char *slash = strrchr(sr->filename, '\\');
    if(slash++ == 0)
        slash = sr->filename;

    sp_send_download_file(sp, sr->hub_address, sr->nick, sr->filename,
            sr->size, slash, sr->tth);

    return 0;
}

int func_hub_lfilelists(sp_t *sp, arg_t *args)
{
    sp_filelist_t *fl;
    LIST_FOREACH(fl, &filelists, link)
    {
        msg("%s: %u files, %s shared",
                fl->nick, fl->root->nfiles, str_size_human(fl->root->size));
    }
    
    return 0;
}

void sort_filelist(sp_filelist_t *fl)
{
    /* fl_sort_recursive(fl->root); */
}

char *filelists_completion_function(const char *text, int state)
{
    static sp_filelist_t *fl = NULL;
    static int len;

    if(state == 0)
    {
        fl = LIST_FIRST(&filelists);
        len = strlen(text);
    }

    while(fl)
    {
        if(fl && str_has_prefix(fl->nick, text))
            return strdup(fl->nick);
        fl = LIST_NEXT(fl, link);
    }

    return NULL;
}

int func_hub_rfilelist(sp_t *sp, arg_t *args)
{
    msg("parsing filelist from file %s", args->argv[1]);

    sp_filelist_t *fl = sp_read_filelist(args->argv[1]);
    if(fl)
        LIST_INSERT_HEAD(&filelists, fl, link);
    else
        msg("failed to parse filelist from %s", args->argv[1]);

    return 0;
}

int func_hub_list_users(sp_t *sp, arg_t *args)
{
    unsigned int n = 0;
    char *filter = 0;

    if(args->argc > 1)
        filter = args->argv[1];

    sp_user_t *user;
    LIST_FOREACH(user, &current_hub->users, link)
    {
        if(filter == 0 ||
           (strstr(user->nick, filter) ||
            (user->tag && strstr(user->tag, filter))))
        {
            msg("%-25s %10s %-10s %s",
                    user->nick, str_size_human(user->size),
                    user->speed ? user->speed : "",
                    user->tag ? user->tag : "");
            ++n;
        }
    }
    msg("%u users", n);

    return 0;
}

int func_hub_disconnect(sp_t *sp, arg_t *args)
{
    sp_send_disconnect(sp, current_hub->address);
    sphub_t *xhub = current_hub;

    LIST_REMOVE(current_hub, link);
    /* hubs = g_list_remove(hubs, current_hub); */

    current_hub = LIST_FIRST(&hubs);
    if(current_hub)
    {
        msg("changed current hub to %s", current_hub->address);
    }
    else
    {
        context = CTX_MAIN;
    }

    /* g_list_free(xhub->users); */
    /* g_list_free(xhub->search_results); */
    free(xhub->name);
    free(xhub->address);
    free(xhub->nick);
    free(xhub);
    
    return 0;
}

int func_hub_exit(sp_t *sp, arg_t *args)
{
    context = CTX_MAIN;
    return 0;
}

int func_hub_queue(sp_t *sp, arg_t *args)
{
    context = CTX_QUEUE;
    return 0;
}

int func_hub_set_password(sp_t *sp, arg_t *args)
{
    sp_send_set_password(sp, current_hub->address, args->argv[1]);
    return 0;
}

