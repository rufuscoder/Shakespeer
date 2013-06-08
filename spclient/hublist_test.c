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

#include "hublist.h"
#include "nfkc.h"
#include "unit_test.h"

void validate_hub(hublist_hub_t *hub)
{
    /* printf("%-40s %3i%%  %2s %i users\n", hub->address, hub->reliability, hub->country, hub->current_users); */
    /* printf("%s\n", hub->description); */
    fail_unless(hub);
    fail_unless(hub->address);
    if(hub->description)
        fail_unless(g_utf8_validate(hub->description, -1, NULL));
    if(hub->country)
        fail_unless(g_utf8_validate(hub->country, -1, NULL));
}

unsigned validate_hublist(hublist_t *hlist)
{
    unsigned n = 0;
    hublist_hub_t *h;
    LIST_FOREACH(h, hlist, link)
    {
        validate_hub(h);
        n++;
    }

    return n;
}

int main(void)
{
    unsigned nvalid;
    hublist_t *hublist;
    xerr_t *err = 0;

    hublist = hl_parse_file("PublicHubList.xml.bz2", &err);
    if(err)
        fprintf(stderr, "hl_parse_file: %s\n", xerr_msg(err));
    fail_unless(hublist);
    fail_unless(err == NULL);
    nvalid = validate_hublist(hublist);
    printf("parsed %i hubs from xml\n", nvalid);
    fail_unless(nvalid == 1499);
    hl_free(hublist);

    hublist = hl_parse_file("PublicHubList.config.bz2", &err);
    if(err)
        fprintf(stderr, "hl_parse_file: %s\n", xerr_msg(err));
    fail_unless(hublist);
    fail_unless(err == NULL);
    nvalid = validate_hublist(hublist);
    printf("parsed %i hubs from config\n", nvalid);
    fail_unless(nvalid == 1500);
    hl_free(hublist);

#if 0
    hublist = hl_parse_url("http://www.hublist.org/PublicHubList.xml.bz2", "/tmp", &err);
    if(err)
        fprintf(stderr, "hl_parse_url: %s\n", xerr_msg(err));
    fail_unless(hublist);
    fail_unless(err == NULL);
    /* fail_unless(g_list_length(hublist) == 1499); */
    validate_hublist(hublist);
    /* printf("parsed %u hubs\n", g_list_length(hublist)); */
    hl_free(hublist);
#endif

    return 0;
}

