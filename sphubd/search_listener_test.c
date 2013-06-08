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

#include <stdio.h>

#include "search_listener.h"
#include "notifications.h"
#include "unit_test.h"

static int got_response = 0;

/* ouch! */
int extra_slots_get_for_user(const char *nick)
{
    return 0;
}
void hub_update_slots(void)
{
}

void handle_search_response_notification(nc_t *nc, const char *channel,
        nc_search_response_t *notification, void *user_data)
{
    fail_unless(nc);
    fail_unless(channel);
    fail_unless(notification);
    fail_unless(notification->response);
    search_response_t *response = notification->response;
    fail_unless(response->hub);
    fail_unless(response->nick);
    fail_unless(response->filename);

    if(response->id > 0)
    {
        got_response++;
    }
}

int main(void)
{
    search_listener_t *sl = search_listener_new(0); /* create a passive search listener */
    fail_unless(sl);

    /* register for search response notifications */
    nc_add_search_response_observer(nc_default(), handle_search_response_notification, NULL);

    search_request_t *request = search_listener_create_search_request("ample zip", 0,
            SHARE_SIZE_NONE, SHARE_TYPE_ANY, 1);
    fail_unless(request);

    fail_unless(request->real_size == 0);
    fail_unless(request->search_size == 0);
    fail_unless(request->type == SHARE_TYPE_ANY);
    fail_unless(request->size_restriction == SHARE_SIZE_NONE);
    fail_unless(request->id == 1);

    got_response = 0;

    /* un-parseable search response */
    search_listener_handle_response(sl, "example");
    fail_unless(got_response == 0);

    /* must add a hub so the search_listener can find it when parsing the
     * search response */
    hub_t *hub1 = hub_new();
    hub1->address = strdup("127.13.2.230:2345");
    hub_list_add(hub1);

    hub_t *hub2 = hub_new();
    hub2->address = strdup("127.0.0.1:6666");
    hub_list_add(hub2);

    hub_t *hub3 = hub_new();
    hub3->address = strdup("1.2.3.4:4111");
    hub_list_add(hub3);

    const char *tth = "FAKEDTTHTOPROTECTTHEINNOCENTADSEGCVBFRX";
    char *search_response_string;
    asprintf(&search_response_string, "$SR [Tele2]foo c:\\example file[test].zip68458636 3/3TTH:%s (127.13.2.230:2345)|", tth);

    /* search request not yet registered, should fail */
    got_response = 0;
    search_listener_handle_response(sl, search_response_string);
    fail_unless(got_response == 0);

    /* add the search request, should succeed */
    search_listener_add_request(sl, request);
    got_response = 0;
    search_listener_handle_response(sl, search_response_string);
    fail_unless(got_response == 1);

    char *srs;
    asprintf(&srs, "TTH:%s", tth);
    request = search_listener_create_search_request(srs, 0, SHARE_SIZE_NONE, SHARE_TYPE_ANY, 2);
    free(srs);
    fail_unless(request);
    search_listener_add_request(sl, request);

    got_response = 0;
    search_listener_handle_response(sl, search_response_string);
    fail_unless(got_response == 1);

    search_response_t *resp = sl_parse_response("gazonk spclient\\hublist_test.c2539 3/3TTH:QBVGSER2GIH34DOZ4WDWU5QOCZIS7RLHAA5NAPI (127.0.0.1:6666)|");
    fail_unless(resp);
    sl_response_free(resp);

    /* Test nicknames beginning with S or R. */
    resp = sl_parse_response("St.Anger test2539 3/3TTH:ASDFSER2GIH34DOZ4WDWU5QOCZIS7RLHAA5NAPI (127.0.0.1:6666)|");
    fail_unless(resp);
    fail_unless(resp->nick);
    fail_unless(strcmp(resp->nick, "St.Anger") == 0);
    sl_response_free(resp);

    resp = sl_parse_response("$SR [FOO]bar files\\directory\\filename 3/3[BAR]Silly Hub™#04 (1.2.3.4:4111)|");
    fail_unless(resp);
    fail_unless(resp->openslots == 3);
    fail_unless(resp->totalslots == 3);
    fail_unless(resp->nick);
    fail_unless(strcmp(resp->nick, "[FOO]bar") == 0);
    fail_unless(resp->filename);
    fail_unless(strcmp(resp->filename, "files\\directory\\filename") == 0);
    fail_unless(resp->tth == NULL);
    fail_unless(resp->type == SHARE_TYPE_DIRECTORY);
    fail_unless(resp->size == 0ULL);
    sl_response_free(resp);

    /* a search response for a non-connected hub should be dropped */
    resp = sl_parse_response("$SR [FOO]bar files\\directory\\filename 3/3[BAR]Silly Hub™#04 (1.1.1.1:1111)|");
    fail_unless(resp == NULL);

    resp = sl_parse_response("$SR fsdewck-O -=mTv=-\\hip hop\\foo ft. bar - funky baz.mp36979308 1/3TTH:6YLV6XTDAZAIGHQTZ7MBULCBNCY7W3EITECFWEQ (1.2.3.4:4111)|9.19.174)|C5U4A (11.9.19.174)|.19.174)|SSanFan666");
    fail_unless(resp);
    fail_unless(resp->tth);
    fail_unless(strcmp(resp->tth, "6YLV6XTDAZAIGHQTZ7MBULCBNCY7W3EITECFWEQ") == 0);
    sl_response_free(resp);

    /* this should fail, 215 octal = 0x8D, which is not valid Windows-1252 encoding */
    const char *response_in_windows_1251_encoding = "$SR [TR]zi\231 turkce mp3\\hande yener\\hande yener - ac\215 veriyor.mp3\0055187712 0/3\005TTH:4AR7IPI5GHCXDNLZ7UDNJJ254WHHJKISOPMLZ5Y (1.2.3.4:4111)";
    resp = sl_parse_response(response_in_windows_1251_encoding);
    fail_unless(resp == NULL);

    /* this should succeed, 215 octal = 0x8D, which is valid Windows-1251 encoding */
    free(hub3->encoding);
    hub3->encoding = strdup("WINDOWS-1251");
    resp = sl_parse_response(response_in_windows_1251_encoding);
    fail_unless(resp);
    fail_unless(strcmp(resp->nick, "[TR]zi\xe2\x84\xa2") == 0);
    fail_unless(strcmp(resp->filename, "turkce mp3\\hande yener\\hande yener - ac\xd0\x8c veriyor.mp3") == 0);

    request = search_listener_create_search_request(tth, 0, SHARE_SIZE_NONE, SHARE_TYPE_TTH, 42);
    fail_unless(request);
    fail_unless(request->tth);
    fail_unless(strcmp(request->tth, tth) == 0);
    fail_unless(request->words == NULL);

    return 0;
}

