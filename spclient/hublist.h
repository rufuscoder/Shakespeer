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

#ifndef _hublist_h_
#define _hublist_h_

#include "sys_queue.h"
#include <stdint.h>
#include "xerr.h"

typedef struct hublist_hub hublist_hub_t;
struct hublist_hub
{
    LIST_ENTRY(hublist_hub) link;

    char *name;
    char *address;
    char *description;
    unsigned max_users;
    unsigned current_users;
    unsigned reliability;
    char *country;
    int min_slots;
    int max_hubs;
    uint64_t min_share;
};

typedef struct hublist hublist_t;
LIST_HEAD(hublist, hublist_hub);

void hl_free(hublist_t *hlist);
hublist_t *hl_parse_file(const char *filename, xerr_t **err);
hublist_t *hl_parse_url(const char *address, const char *workdir, xerr_t **err);
char *hl_get_current(void);

#endif

