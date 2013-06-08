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

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hublist.h"
#include "args.h"
#include "bz2.h"
#include "rx.h"
#include "nmdc.h"
#include "encoding.h"
#include "xml.h"
#include "xstr.h"
#include "util.h"
#include "uhttp.h"

#include "log.h"

/* from country_map.c */
const char *country_map_lookup(const char *code);

const char *hl_lookup_country(const char *country_code)
{
    const char *longname = country_map_lookup(country_code);
    if(longname == NULL)
        return country_code;
    return longname;
}

static hublist_hub_t *hl_parse_entry(arg_t *args)
{
    assert(args);

    if(args->argc < 4)
        return NULL;

    hublist_hub_t *h = calloc(1, sizeof(hublist_hub_t));

    h->name = strdup(args->argv[0]);
    if(h->name == NULL)
    {
        free(h);
        return NULL;
    }

    rx_subs_t *subs = rx_search(args->argv[2],
            "<R: *([0-9]+)%,S:[^,]+,C:([a-zA-Z0-9-]+)> (.*)");

    if(subs && subs->nsubs == 3)
    {
        h->reliability = strtoul(subs->subs[0], NULL, 10);
        h->country = strdup(hl_lookup_country(subs->subs[1]));
        h->description = strdup(subs->subs[2]);
        rx_free_subs(subs);
    }
    else
    {
        /* INFO("failed description: [%s]", args->argv[2]); */
        h->description = strdup(args->argv[2]);
    }

    if(h->description == NULL)
    {
        free(h->name);
        free(h->country);
        free(h);
        return NULL;
    }

    h->address = strdup(args->argv[1]);

    h->current_users = strtoul(args->argv[3], NULL, 10);

    return h;
}

static void hl_hub_free(hublist_hub_t *h)
{
    if(h)
    {
        free(h->name);
        free(h->address);
        free(h->description);
        free(h->country);
        free(h);
    }
}

void hl_free(hublist_t *hlist)
{
    if(hlist)
    {
        hublist_hub_t *h, *next;
        for(h = LIST_FIRST(hlist); h; h = next)
        {
            next = LIST_NEXT(h, link);
            LIST_REMOVE(h, link);
            hl_hub_free(h);
        }
        free(hlist);
    }
}

static void hl_xml_parse_start_element(void *data,
        const char *el, const char **attr)
{
    hublist_t *hlist = data;

    if(strcasecmp(el, "Hub") != 0)
    {
        return;
    }

    hublist_hub_t *h = calloc(1, sizeof(hublist_hub_t));

    int i;
    for(i = 0; attr && attr[i]; i += 2)
    {
        if(strcmp(attr[i], "Address") == 0)
        {
            h->address = strdup(attr[i+1]);
        }
        else if(strcmp(attr[i], "Name") == 0)
        {
            h->name = nmdc_unescape(attr[i+1]);
        }
        else if(strcmp(attr[i], "Description") == 0)
        {
            h->description = nmdc_unescape(attr[i+1]);
        }
        else if(strcmp(attr[i], "Country") == 0)
        {
            h->country = strdup(attr[i+1]);
        }
        else if(strcmp(attr[i], "Minshare") == 0)
        {
            h->min_share = strtoull(attr[i+1], NULL, 10);
        }
        else if(strcmp(attr[i], "Maxhubs") == 0)
        {
            h->max_hubs = strtoul(attr[i+1], NULL, 10);
        }
        else if(strcmp(attr[i], "Maxusers") == 0)
        {
            h->max_users = strtoul(attr[i+1], NULL, 10);
        }
        else if(strcmp(attr[i], "Reliability") == 0)
        {
            h->reliability = strtoul(attr[i+1], NULL, 10);
        }
        else if(strcmp(attr[i], "Users") == 0)
        {
            h->current_users = strtoul(attr[i+1], NULL, 10);
        }
        else if(strcmp(attr[i], "Minslots") == 0)
        {
            h->min_slots = strtoul(attr[i+1], NULL, 10);
        }
    }

    if(h->address == NULL)
    {
        hl_hub_free(h);
    }
    else
    {
        LIST_INSERT_HEAD(hlist, h, link);
    }
}

static hublist_t *hl_parse_file_xml(const char *filename, xerr_t **err)
{
    assert(filename);

    FILE *fp = fopen(filename, "r");
    return_val_if_fail(fp, NULL);

    hublist_t *hlist = calloc(1, sizeof(hublist_t));
    LIST_INIT(hlist);

    xml_ctx_t *ctx = xml_init_fp(fp, hl_xml_parse_start_element, NULL, hlist);

    while(xml_parse_chunk(ctx, err) == 0)
    {
        /* do nothing */ ;
    }
    
    xml_ctx_free(ctx);

    fclose(fp);

#if 0
    /* Try to get around encoding violations in the hublist
     *
     * This will break when the hublist is in another encoding than windows-1252.
     * 
     * The definition for windows-1252 encoding is available at
     * http://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1252.TXT
     */
    char *contents_utf8 = str_convert_to_lossy_utf8(contents, "WINDOWS-1252");
    free(contents);
    if(contents_utf8 == NULL)
    {
        xerr_set(err, -1, "Really messed up encoding");
        return NULL;
    }
#endif

    return hlist;
}

static hublist_t *hl_parse_file_config(const char *filename, xerr_t **err)
{
    hublist_t *hlist;
    char buf[8192];

    hlist = calloc(1, sizeof(hublist_t));
    LIST_INIT(hlist);

    FILE *fp = fopen(filename, "r");
    if(fp == NULL)
    {
        xerr_set(err, -1, "%s", strerror(errno));
        return NULL;
    }

    while(1)
    {
        arg_t *args = NULL;

        if(fgets(buf, 8192, fp) == 0)
            break;
        buf[8191] = 0;
        char *buf_utf8 = str_legacy_to_utf8_lossy(buf, "WINDOWS-1252");
        if(buf_utf8)
        {
            args = arg_create(buf_utf8, "|", 0);
            free(buf_utf8);
        }
        hublist_hub_t *h = 0;
        if(args)
            h = hl_parse_entry(args);

        if(h)
            LIST_INSERT_HEAD(hlist, h, link);
        arg_free(args);
    }

    return hlist;
}

hublist_t *hl_parse_file(const char *filename, xerr_t **err)
{
    hublist_t *hlist = 0;

    assert(filename);

    char *xfilename = strdup(filename);
    if(str_has_suffix(xfilename, ".bz2"))
    {
        char *e = strrchr(xfilename, '.');
        assert(e);
        *e = 0;

        if(bz2_decode(filename, xfilename, err) != 0)
        {
            free(xfilename);
            return NULL;
        }
    }

    if(str_has_suffix(xfilename, ".xml"))
        hlist = hl_parse_file_xml(xfilename, err);
    else
        hlist = hl_parse_file_config(xfilename, err);
    free(xfilename);

    return hlist;
}

hublist_t *hl_parse_url(const char *address, const char *workdir, xerr_t **err)
{
    uhttp_t *uhttp;
    hublist_t *result = 0;

    uhttp = uhttp_init();
    if (uhttp_open_url(uhttp, address) == 0) {
        char *slash;

        slash = strrchr(address, '/');
        if (slash == 0) {
            xerr_set(err, -1, "invalid address: %s", address);
            return NULL;
        }

        uhttp_get(uhttp);
        int rc = uhttp_read_response_headers(uhttp);
        if (rc == 200) {
            char *ofname;
            int num_returned_bytes = asprintf(&ofname, "%s/%s", workdir, slash + 1);
            if (num_returned_bytes == -1)
                DEBUG("asprintf did not return anything");
            if (uhttp_read_response_file(uhttp, ofname) == 0)
                result = hl_parse_file(ofname, err);
            free(ofname);
        }
        else
            xerr_set(err, -1, "failed to fetch hublist file");

        uhttp_cleanup(uhttp);
    }
    else
        xerr_set(err, -1, "failed to fetch hublist file");

    assert(err || result);

    return result;
}

char *hl_get_current(void)
{
    char *working_directory = get_working_directory();
    char *hublist_filename;
    int num_returned_bytes;
    num_returned_bytes = asprintf(&hublist_filename, "%s/PublicHubList.xml.bz2", working_directory);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
    if (access(hublist_filename, F_OK) != 0) {
        free(hublist_filename);
        num_returned_bytes = asprintf(&hublist_filename, "%s/PublicHubList.config.bz2", working_directory);
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
        if (access(hublist_filename, F_OK) != 0) {
            free(hublist_filename);
            hublist_filename = NULL;
        }
    }
    free(working_directory);

    return hublist_filename;
}

