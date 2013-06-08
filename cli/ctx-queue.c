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

queue_list_t queue_head = LIST_HEAD_INITIALIZER();

int func_queue_refresh(sp_t *sp, arg_t *args)
{
    return 0;
}

static char *abbr_sources(queue_source_list_t *sources)
{
    queue_source_item_t *qs;
    int n = 0;
    LIST_FOREACH(qs, sources, link)
    {
        ++n;
    }

    char *abbr = 0;

#if 0
    if(n == 1)
    {
        asprintf(&abbr, "1 source (%s)",
                ((queue_source_item_t *)sources->data)->nick);
    }
    else if(n == 2)
    {
        asprintf(&abbr, "2 sources (%s, %s)",
                ((queue_source_item_t *)sources->data)->nick,
                ((queue_source_item_t *)sources->next->data)->nick);
    }
    else if(n == 3)
    {
        asprintf(&abbr, "3 sources (%s, %s, %s)",
                ((queue_source_item_t *)sources->data)->nick,
                ((queue_source_item_t *)sources->next->data)->nick,
                ((queue_source_item_t *)sources->next->next->data)->nick);
    }
    else
#endif
    {
        asprintf(&abbr, "%d sources", n);
    }

    return abbr;
}

int func_queue_ls(sp_t *sp, arg_t *args)
{
    int i = 0;

    char *download_directory = tilde_expand_path(cfg_getstr(cfg, "download-directory"));
    int ddirlen = strlen(download_directory) + 1;
    free(download_directory);

    queue_item_t *qi;
    LIST_FOREACH(qi, &queue_head, link)
    {
        char *srcs = abbr_sources(&qi->sources);
        char *short_filename = str_shorten_path(qi->local_filename + ddirlen, 40);
        msg("%02d: %-40s %10s, tth: %s, %s",
                ++i, short_filename, str_size_human(qi->size),
                qi->tth ? qi->tth : "none",
                srcs);
        free(short_filename);
        free(srcs);
    }
    return 0;
}

int func_queue_remove(sp_t *sp, arg_t *args)
{
    queue_item_t *qi = 0;
    int num = atoi(args->argv[1]);
    if(num > 0)
    {
        int i = 0;
        LIST_FOREACH(qi, &queue_head, link)
        {
            if(++i == num)
                break;
        }
    }

    if(qi)
        sp_send_queue_remove_target(sp, qi->local_filename);
    else
        msg("no such queue item");

    return 0;
}

int func_queue_remove_source(sp_t *sp, arg_t *args)
{
    queue_item_t *qi = 0;
    int num = atoi(args->argv[1]);
    if(num > 0)
    {
        int i = 0;
        LIST_FOREACH(qi, &queue_head, link)
        {
            if(++i == num)
                break;
        }
    }

    if(qi)
    {
        char *nick = args->argv[2];
        sp_send_queue_remove_source(sp, qi->local_filename, nick);
    }
    else
        msg("no such queue item");

    return 0;
}

int func_queue_remove_filelist(sp_t *sp, arg_t *args)
{
    sp_send_queue_remove_filelist(sp, args->argv[1]);
    return 0;
}

int func_queue_remove_nick(sp_t *sp, arg_t *args)
{
    char *nick = args->argv[1];
    sp_send_queue_remove_nick(sp, nick);

    return 0;
}

int func_queue_exit(sp_t *sp, arg_t *args)
{
    context = CTX_HUB;
    return 0;
}

