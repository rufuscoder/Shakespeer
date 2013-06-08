/*
 * Copyright 2005-2006 Martin Hedenfalk <martin@bzero.se>
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
#include <unistd.h>

#include "shakespeer.h"
#include "hublist.h"
#include "xstr.h"
#include "nfkc.h"

static arg_t *filter = 0;
static hublist_t *hublist = 0;

static int hub_matches_filter(hublist_hub_t *h)
{
    if(filter)
    {
        int i;
        for(i = 0; i < filter->argc; i++)
        {
            if(strcasestr(h->name, filter->argv[i]) == NULL &&
                    (h->description == NULL ||
                     strcasestr(h->description, filter->argv[i]) == NULL))
            {
                return 0; /* no match */
            }
        }
    }

    return 1;
}

int cmd_complete_hublist(WordCompletion *cpl, const char *line,
        int word_start, int word_end)
{
    int len = word_end - word_start;
    char *partial = xstrndup(line + word_start, len);

    hublist_hub_t *h;
    LIST_FOREACH(h, hublist, link)
    {
        if(h && hub_matches_filter(h) && str_has_prefix(h->address, partial))
        {
            cpl_add_completion(cpl, line, word_start, word_end,
                    h->address + len, "", " ");
        }
    }

    return 0;
}

int func_hublist_refresh(sp_t *sp, arg_t *args)
{
    hl_free(hublist);

    char *tmp;
    asprintf(&tmp, "%s/PublicHubList.xml", working_directory);
    xerr_t *err = 0;
    if(access(tmp, F_OK) == 0)
        hublist = hl_parse_file(tmp, &err);
    else
        hublist = hl_parse_url(cfg_getstr(cfg, "hublist-address"),
                working_directory, &err);
    free(tmp);

    if(err)
    {
        msg("failed to load hublist: %s", xerr_msg(err));
        xerr_free(err);
    }

    return 0;
}

int func_hublist_ls(sp_t *sp, arg_t *args)
{
    if(hublist == NULL)
    {
        msg("No hublist downloaded, use the 'refresh' command");
        return 0;
    }

    hublist_hub_t *h;
    LIST_FOREACH(h, hublist, link)
    {
        if(h == NULL)
        {
            msg("BLARGH!!!");
        }
        else
        {
            if(hub_matches_filter(h))
            {
                char *desc = 0;
                if(g_utf8_strlen(h->description, -1) > 40)
                {
                    desc = malloc(strlen(h->description) + 1);
                    g_utf8_strncpy(desc, h->description, 40);
                }
                char *name = 0;
                if(g_utf8_strlen(h->name, -1) > 40)
                {
                    name = malloc(strlen(h->name) + 1);
                    g_utf8_strncpy(name, h->name, 40);
                }
                msg("%-40s  %-40s  %-40s  %u",
                        h->address, name ? name : h->name,
                        desc ? desc : h->description, h->max_users);
                free(name);
                free(desc);
            }
        }
    }
    return 0;
}

int func_hublist_filter(sp_t *sp, arg_t *args)
{
    if(args->argc == 1)
    {
        arg_free(filter);
        filter = 0;
    }
    else
    {
        filter = arg_create_from_argv(args->argc - 1, args->argv + 1);
    }

    return 0;
}

int func_hublist_exit(sp_t *sp, arg_t *args)
{
    context = CTX_MAIN;
    return 0;
}

