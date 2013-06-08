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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "dstring.h"
#include "xstr.h"
#include "log.h"
#include "rx.h"
#include "nfkc.h"
#include "encoding.h"
#include "globals.h"
#include "share.h"

static int file_matches_search(share_file_t *f, const share_search_t *search)
{
    switch(search->size_restriction)
    {
        case SHARE_SIZE_MIN:
            if(f->size < search->size)
                return 0;
            break;
        case SHARE_SIZE_MAX:
            if(f->size > search->size)
                return 0;
            break;
        case SHARE_SIZE_EQUAL:
            if(f->size != search->size)
                return 0;
            break;
        case SHARE_SIZE_NONE:
            break;
    }

    if(search->type != SHARE_TYPE_ANY && f->type != search->type)
        return 0;

    char *filename = strrchr(f->partial_path, '/');
    if(filename++ == NULL)
	filename = f->partial_path;

    int i;
    for(i = 0; i < search->words->argc; i++)
    {
        /* FIXME: what about UTF-8?  */
        if(strcasestr(filename, search->words->argv[i]) == NULL)
        {
            return 0;
        }
    }

    return 1;
}

int share_search(share_t *share, const share_search_t *search,
        search_match_func_t func, void *user_data)
{
    int limit = 10; /* limit number of search responses */

    if(search->tth)
    {
        /* If we're searching for a TTH, just look it up in the database. */

        struct tth_entry *tthd = tth_store_lookup(global_tth_store, search->tth);
        if(tthd == NULL)
        {
            /* not found */
            return 0;
        }
        DEBUG("found inode %"PRIu64, tthd->active_inode);

        share_file_t *f = share_lookup_file_by_inode(share, tthd->active_inode);
        if(f)
        {
            func(search, f, search->tth, user_data);
        }

        return 0;
    }
    else
    {
        /* Check bloom filter first, if there is one
         */
        if(share->bloom)
        {
            int i;
            for(i = 0; i < search->words->argc; i++)
            {
                if(bloom_check_filename(share->bloom,
                            search->words->argv[i]) != 0)
                {
                    DEBUG("[%s] failed bloom check, skipping search",
                            search->words->argv[i]);
                    return 0;
                }
            }
        }

        if(search->type == SHARE_TYPE_DIRECTORY)
        {
            /* FIXME: implement search for directory type */
        }
        else
        {
            share_file_t *f;
            RB_FOREACH(f, file_tree, &share->files)
            {
                if(file_matches_search(f, search))
                {
		    struct tth_inode *ti = tth_store_lookup_inode(global_tth_store, f->inode);
                    int rc = func(search, f, ti ? ti->tth : NULL, user_data);
                    if(--limit == 0)
                        break;

                    if(rc == -1)
                    {
                        /* search match callback failed, don't try again */
                        break;
                    }
                }
            }

#if 0
            if(search->type == SHARE_TYPE_ANY &&
                    search->size_restriction == SHARE_SIZE_NONE)
            {
                /* include directories in search */
                dstring_append_format(sql,
                        " UNION SELECT 0, vd.path, vd.name, %d, 0"
                        " FROM virtual_directory vd"
                        " WHERE", SHARE_TYPE_DIRECTORY);
                share_append_sql_name(sql, "vd.name", search->words);
            }

#endif
        }
    }

    return 0;
}

share_search_t *share_search_parse_nmdc(const char *command,
        const char *encoding)
{
    const char *original_command = command;
    share_search_t *s = calloc(1, sizeof(share_search_t));

    size_t n = strcspn(command, ":");
    if(command[n] != ':')
        goto error;

    s->host = xstrndup(command, n);
    command += n + 1;

    n = strcspn(command, " ");
    if(command[n] != ' ')
        goto error;

    char *port_or_user = xstrndup(command, n);
    command += n + 1;

    s->passive = (strcasecmp(s->host, "Hub") == 0 ? true : false);
    if(s->passive)
    {
        s->nick = port_or_user;
    }
    else
    {
        s->port = strtoul(port_or_user, NULL, 10);
        if(s->port <= 0)
        {
            INFO("Invalid port number '%s' in search request (ignored)",
                    port_or_user);
            free(port_or_user);
            goto error;
        }
    }

    int size_restricted = (*command == 'T');
    command++;
    if(*command++ != '?')
        goto error;

    int is_minimum_size = (*command == 'F');
    command++;
    if(*command++ != '?')
        goto error;

    n = strcspn(command, "?");
    if(command[n] != '?')
        goto error;

    if(size_restricted)
    {
        s->size_restriction =
            (is_minimum_size ? SHARE_SIZE_MIN : SHARE_SIZE_MAX);

        char *size_str = xstrndup(command, n);
        s->size = strtoull(size_str, NULL, 10);
        free(size_str);
    }
    else
    {
        s->size_restriction = SHARE_SIZE_NONE;
    }

    command += n + 1;
    n = strcspn(command, "?");
    if(command[n] != '?')
        goto error;

    char *type_str = xstrndup(command, n);
    s->type = strtoul(type_str, NULL, 10);
    free(type_str);

    command += n + 1;
    if(*command == 0)
        goto error;

    if(str_has_prefix(command, "TTH:"))
    {
        s->tth = strdup(command + 4);
        s->words = NULL;
    }
    else
    {
        char *search_string_utf8 = str_convert_to_unescaped_utf8(command,
                encoding);
        if(search_string_utf8 == NULL)
        {
            INFO("Invalid search string encoding.");
            goto error;
        }

        /* normalize the string (handles different decomposition) */
        char *normalized_utf8_string = g_utf8_normalize(search_string_utf8,
                -1, G_NORMALIZE_DEFAULT_COMPOSE);
        free(search_string_utf8);

        /* make independent of case */
        char *casefold_utf8_string = g_utf8_casefold(normalized_utf8_string, -1);
        free(normalized_utf8_string);

        s->tth = NULL;
        s->words = arg_create(casefold_utf8_string, "$", 0);
        free(casefold_utf8_string);
    }

    return s;

error:
    free(s->tth);
    free(s->host);
    free(s->nick);
    free(s);

    DEBUG("Invalid search request: [%s]", original_command);

    return NULL;
}

void share_search_free(share_search_t *s)
{
    if(s)
    {
        free(s->host);
        free(s->nick);
        free(s->tth);
        arg_free(s->words);
        free(s);
    }
}

#ifdef TEST

#include "globals.h"
#include "unit_test.h"

#include "ui.h"
int ui_send_status_message(ui_t *ui, const char *hub_address, const char *message, ...)
{
    return 0;
}

int main(void)
{
    global_working_directory = "/tmp";
    sp_log_set_level("debug");
    unlink("/tmp/tth.db");

    share_t *share = share_new();
    fail_unless(share);

    share_search_t *s = share_search_parse_nmdc("192.168.1.189:412 F?T?0?9?TTH:QSYBVKR6IAIEF6R4RG7DGBXWEP3PQBTBEBV2IPY", "WINDOWS-1252");
    fail_unless(s);
    fail_unless(s->host);
    fail_unless(strcmp(s->host, "192.168.1.189") == 0);
    fail_unless(s->port == 412);
    fail_unless(s->nick == NULL);
    fail_unless(s->tth);
    fail_unless(strcmp(s->tth, "QSYBVKR6IAIEF6R4RG7DGBXWEP3PQBTBEBV2IPY") == 0);
    fail_unless(s->words == NULL);
    fail_unless(s->type == SHARE_TYPE_TTH);
    fail_unless(s->passive == false);
    share_search_free(s);

    s = share_search_parse_nmdc("1.2.3.4:5922 F?T?0?1?", "WINDOWS-1252");
    fail_unless(s == NULL);

    return 0;
}

#endif

