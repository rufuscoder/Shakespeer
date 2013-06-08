/*
 * Copyright 2006 Martin Hedenfalk <martin@bzero.se>
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

#include <string.h>
#include <stdio.h>

#include "dstring_url.h"
#include "rx.h"
#include "test_connection.h"
#include "log.h"

/* please don't abuse, otherwise I'll have to disable this feature */
#define TC_BASE_URL "http://shakespeer.bzero.se/checkport"

static int test_connection_string(const char *string)
{
    if(string == NULL)
    {
        return TC_RET_INTERNAL;
    }

    int rc = TC_RET_INTERNAL;

    rx_subs_t *subs = rx_search(string, "tcp:([0-9]+)[[:space:]]*udp:([0-9]+)");
    if(subs && subs->nsubs == 2)
    {
        rc = TC_RET_TCP_FAIL | TC_RET_UDP_FAIL;

        if(strcmp(subs->subs[0], "0") == 0)
        {
            if(strcmp(subs->subs[1], "0") == 0)
            {
                rc = TC_RET_OK;
            }
            else
            {
                rc = TC_RET_UDP_FAIL;
            }
        }
    }

    return rc;
}

int test_connection(int port)
{
    if (port <= 1024)
        return TC_RET_PRIVPORT;

    char *url = NULL;
    int num_returned_bytes = asprintf(&url, "%s?port=%u", TC_BASE_URL, port);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");

    if (url) {
        dstring_t *ds = dstring_new_from_url(url);
        free(url);

        if (ds) {
            int rc = test_connection_string(ds->string);
            dstring_free(ds, 1);
            return rc;
        }
    }
    
    return TC_RET_INTERNAL;
}

#ifdef TEST

#include "unit_test.h"

int main(void)
{
    fail_unless(test_connection_string("tcp:0 udp:0") == TC_RET_OK);
    fail_unless(test_connection_string("tcp:0 udp:2") == TC_RET_UDP_FAIL);
    fail_unless(test_connection_string("tcp:1 udp:2") == (TC_RET_TCP_FAIL | TC_RET_UDP_FAIL));
    fail_unless(test_connection_string("strange error") == TC_RET_INTERNAL);
    fail_unless(test_connection(21) == TC_RET_PRIVPORT);

    return 0;
}

#endif

