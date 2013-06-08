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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "nfkc.h"
#include "encoding.h"
#include "io.h"
#include "nmdc.h"
#include "notifications.h"
#include "queue.h"
#include "rx.h"
#include "search_listener.h"
#include "ui.h"
#include "log.h"
#include "xstr.h"

int search_listener_handle_response(search_listener_t *sl, const char *buf);

static void sl_in_event(int fd, short condition, void *data)
{
    static char buf[1500];
    struct sockaddr_in fromaddr;
    unsigned int fromlen = sizeof(fromaddr);
    ssize_t bytes_read = recvfrom(fd, buf, sizeof(buf), 0,
            (struct sockaddr *)&fromaddr, &fromlen);
    if(bytes_read == -1)
    {
        WARNING("recvfrom: %s", strerror(errno));
        return;
    }

    if(bytes_read)
    {
        if(bytes_read == 4 && strncmp(buf, "ping", 4) == 0)
        {
            DEBUG("received ping, sending pong");
            if(sendto(fd, "pong", 4, 0,
                        (const struct sockaddr *)&fromaddr, fromlen) == -1)
            {
                WARNING("sendto(pong): %s", strerror(errno));
            }
        }
        else
        {
            buf[bytes_read] = 0;
            search_listener_handle_response(data, buf);
        }
    }
}

void search_listener_close(search_listener_t *sl)
{
    if(sl)
    {
        if(event_initialized(&sl->in_event))
        {
            event_del(&sl->in_event);
        }

        if(sl->fd != -1)
        {
            close(sl->fd);
        }
        free(sl);
    }
}

static search_listener_t *search_listener_create(void)
{
    search_listener_t *sl = calloc(1, sizeof(search_listener_t));
    TAILQ_INIT(&sl->search_request_head);

    return sl;
}

search_listener_t *search_listener_new(int port)
{
    search_listener_t *sl = search_listener_create();
    if(port <= 0)
    {
        return sl;
    }

    int fd;
    struct sockaddr_in addr;

    fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(fd == -1)
    {
        WARNING("socket(): %s", strerror(errno));
        free(sl);
        return NULL;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY; /* bind to all addresses */

    /* enable local address reuse */
    int on = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
    {
        WARNING("unable to enable local address reuse (ignored)");
    }

    INFO("Binding UDP port %i for search responses", port);
    if(bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) != 0)
    {
        WARNING("Unable to bind UDP port %i: %s", port, strerror(errno));
        close(fd);
        free(sl);
        return NULL;
    }

    sl->fd = fd;

    if(io_set_blocking(sl->fd, 0) != 0)
    {
        close(fd);
        free(sl);
        return NULL;
    }

    DEBUG("adding search listener on file descriptor %i", fd);

    event_set(&sl->in_event, fd, EV_READ|EV_PERSIST, sl_in_event, sl);
    event_add(&sl->in_event, NULL);

    return sl;
}

int search_result_matches_request(const char *filename, uint64_t size, const char *tth,
        search_request_t *sreq)
{
    char *xfilename;
    int i;

    return_val_if_fail(sreq, 0);

    if(sreq->tth)
    {
        if(tth && strcmp(tth, sreq->tth) == 0)
            return 1;
        return 0;
    }

    return_val_if_fail(filename, 0);

    /* ensure composed form. */
    char *filename_utf8_composed = g_utf8_normalize(filename, -1, G_NORMALIZE_DEFAULT_COMPOSE);
    xfilename = g_utf8_casefold(filename_utf8_composed, -1);
    free(filename_utf8_composed);

    for(i = 0; i < sreq->words->argc; i++)
    {
        if(strstr(xfilename, sreq->words->argv[i]) == 0)
            break;
    }
    free(xfilename);

    if(i == sreq->words->argc)
    {
        /* we got a match on the filename, now check the size
         */
        switch(sreq->size_restriction)
        {
            case SHARE_SIZE_MIN:
                if(size < sreq->real_size)
                    return 0;
                break;
            case SHARE_SIZE_MAX:
                if(size > sreq->real_size)
                    return 0;
                break;
            case SHARE_SIZE_EQUAL:
                if(size != sreq->real_size)
                    return 0;
                break;
            case SHARE_SIZE_NONE:
                break;
        }
        return 1;
    }

    return 0;
}

search_response_t *sl_parse_response(const char *buf)
{
    return_val_if_fail(buf, NULL);

    DEBUG("parsing [%s]", buf);

    if(str_has_prefix(buf, "$SR "))
        buf += 4;
    while(*buf == ' ')
        ++buf;

    char *delim = strchr(buf, '|');
    char *xbuf = 0;
    if(delim)
        xbuf = xstrndup(buf, delim - buf);
    else
        xbuf = xstrdup(buf);

    int i, x5 = 0;
    int is_directory;

    /* count number of 0x05's in the search response in order to decide if it's
     * a directory or file response.
     */
    for(i = 0; xbuf[i]; i++)
    {
        if(xbuf[i] == 0x05)
            x5++;
    }
    is_directory = (x5 == 1 ? 1 : 0);

    const void *rx = NULL;
    if(is_directory)
    {
        static const void *rx_dir = NULL;
        if(rx_dir == NULL)
        {
            rx_dir = rx_compile("([^ ]+) (.+) ([0-9]+)/([0-9]+)\x05(.*)\\(([-0-9a-zA-Z_.:]+)\\)\\|?");
        }
        rx = rx_dir;
    }
    else
    {
        static const void *rx_file = NULL;
        if(rx_file == NULL)
        {
            rx_file = rx_compile("([^ ]+) ([^\x05]+)\x05([0-9]+) ([0-9]+)/([0-9]+)\x05(.*)\\(([-0-9a-zA-Z_.:]+)\\)\\|?");
        }
        rx = rx_file;
    }
    return_val_if_fail(rx, NULL);

    search_response_t *resp = NULL;
    rx_subs_t *subs = rx_search_precompiled(xbuf, rx);
    if(subs && \
            ((subs->nsubs == 6 && is_directory) || \
             (subs->nsubs == 7 && !is_directory)))
    {
        /* regexp matched */

        char *nick = subs->subs[0];
        char *filename = subs->subs[1];

        if(nick && filename)
        {
            /* Need to find the associated hub in order to determine what
             * encoding to use.
             */
            char *hub_address = subs->subs[6 - is_directory];
            char *nick_utf8 = 0;
            hub_t *hub = hub_find_by_address(hub_address);
            if(hub == 0)
            {
                hub = hub_find_encoding_by_nick(nick, &nick_utf8);
                if(hub == NULL)
                {
                    WARNING("unknown hub address '%s'"
                            " in search response (skipping)", hub_address);
                    rx_free_subs(subs);
                    free(xbuf);
                    return NULL;
                }
            }
            else
            {
                nick_utf8 = str_convert_to_unescaped_utf8(nick, hub->encoding);
            }

            if(nick_utf8 == NULL)
            {
                WARNING("no valid nick in search response (skipping)");
                rx_free_subs(subs);
                free(xbuf);
                return NULL;
            }
            
            resp = calloc(1, sizeof(search_response_t));

            resp->nick = nick_utf8;
            resp->filename = str_legacy_to_utf8(filename, hub->encoding);

            if(resp->filename == NULL)
            {
                free(resp);
                free(nick_utf8);
                rx_free_subs(subs);
                free(xbuf);
                return NULL;
            }
            
            resp->hub = hub;
            resp->openslots = strtoull(subs->subs[3 - is_directory], 0, 10);
            resp->totalslots = strtoull(subs->subs[4 - is_directory], 0, 10);

            resp->type = SHARE_TYPE_DIRECTORY;
            resp->size = 0;
            if(!is_directory)
            {
                resp->type = share_filetype(filename);
                resp->size = strtoull(subs->subs[2], 0, 10);
            }

            if(strncmp(subs->subs[5 - is_directory], "TTH:", 4) == 0)
            {
                resp->tth = str_trim_end_inplace(xstrdup(subs->subs[5 - is_directory] + 4), NULL);
            }

            if(resp->tth && !valid_tth(resp->tth))
            {
                WARNING("invalid TTH in search response [%s]", xbuf);
                sl_response_free(resp);
                resp = NULL;
            }
        }
        rx_free_subs(subs);
    }
    else
    {
        INFO("regexp failed on: [%s]", xbuf);
    }
    free(xbuf);

    return resp;
}

void sl_response_free(search_response_t *resp)
{
    if(resp)
    {
        free(resp->filename);
        free(resp->nick);
        free(resp->tth);
        free(resp);
    }
}

int search_listener_handle_response(search_listener_t *sl, const char *buf)
{
    return_val_if_fail(sl, -1);
    return_val_if_fail(buf, -1);

    search_response_t *resp = sl_parse_response(buf);
    if(resp == NULL)
    {
        INFO("invalid search response (ignored)");
        return 1;
    }

    /* Go through the list of search requests and find a matching search
     * ID. The list is traversed backwards, so if there are multiple
     * matches, the last search is used. */
    int search_id = 0;
    struct search_request *sreq;
    TAILQ_FOREACH_REVERSE(sreq, &sl->search_request_head,
            search_request_list, link)
    {
        if(search_result_matches_request(resp->filename, resp->size,
                    resp->tth, sreq))
        {
            DEBUG("Found search ID %i", sreq->id);
            search_id = sreq->id;
            break;
        }
    }

    resp->id = search_id;

    /* notify any observers of the search response */
    nc_send_search_response_notification(nc_default(), resp);

    sl_response_free(resp);

    return 1;
}

/* <words> assumed to be in UTF-8 */
search_request_t *search_listener_create_search_request(const char *words,
        uint64_t size,
        share_size_restriction_t size_restriction,
        share_type_t type, int id)
{
    return_val_if_fail(words, NULL);

    search_request_t *sreq = calloc(1, sizeof(search_request_t));

    char *words_unescaped = nmdc_unescape(words);

    char *tth = NULL;
    if(str_has_prefix(words_unescaped, "TTH:"))
        tth = words_unescaped + 4;
    else if(type == SHARE_TYPE_TTH)
        tth = words_unescaped;

    if(tth)
    {
        sreq->tth = xstrdup(tth);
        free(words_unescaped);
        if(!valid_tth(sreq->tth))
        {
            DEBUG("invalid TTH [%s], skipping search", sreq->tth);
            free(sreq->tth);
            free(sreq);
            return NULL;
        }
        sreq->type = SHARE_TYPE_TTH;
    }
    else
    {
        /* Convert to composed utf-8.
         */
        if(words_unescaped == NULL)
        {
            WARNING("invalid encoding in search words, ignoring");
            free(sreq);
            return NULL;
        }
        char *words_utf8_composed = g_utf8_normalize(words_unescaped, -1,
                G_NORMALIZE_DEFAULT_COMPOSE);
        free(words_unescaped);
        char *p_casefold = g_utf8_casefold(words_utf8_composed, -1);
        free(words_utf8_composed);
        return_val_if_fail(p_casefold, NULL);
        str_replace_set(p_casefold, " ", '$');
        char *p = p_casefold;
        while(p && *p == '$') /* skip any initial $'s */
        {
            ++p;
        }
        if(*p == 0)
        {
            DEBUG("empty search string");
            free(sreq);
            free(p_casefold);
            return NULL;
        }

        sreq->real_size = size;
        sreq->size_restriction = size_restriction;
        sreq->type = type;
        sreq->minsize = (size_restriction == SHARE_SIZE_MIN);
        sreq->search_size = size;

        if(size_restriction == SHARE_SIZE_EQUAL)
        {
            /* if we're searching for an exact size (which isn't supported by the
             * DC protocol, doh!), we simulate this by searching either for a
             * minimum or maximum size, and later filter the incoming search
             * responses. If we search for an exact size of 100 MiB or more, we put
             * a lower bound on that size (minus one so we really include the exact
             * size asked for), otherwise we put an upper bound on the size.
             */
            if(size > 100*1024*1024)
            {
                sreq->minsize = 1;
                sreq->search_size = size - 1;
            }
            else
            {
                sreq->minsize = 0;
                sreq->search_size = size + 1;
            }
        }

        sreq->words = arg_create(p_casefold, "$", 0);
        free(p_casefold);
    }
    sreq->id = id;

    return sreq;
}

void sl_request_free(search_request_t *sreq)
{
    if(sreq)
    {
        arg_free(sreq->words);
        free(sreq->tth);
        free(sreq);
    }
}

void search_listener_add_request(search_listener_t *sl, search_request_t *request)
{
    return_if_fail(sl);
    return_if_fail(request);

    if(request->id == -1)
    {
        /* search id -1 is used internally for auto-searching alternative
         * sources (see queue_auto_search_sources_event_func)
         *
         * Place those requests in the beginning of the list so they are
         * searched last when matching responses, so we don't interfere with
         * manual searches from the UI.
         */
        TAILQ_INSERT_HEAD(&sl->search_request_head, request, link);
    }
    else
    {
        TAILQ_INSERT_TAIL(&sl->search_request_head, request, link);
    }
}

void sl_forget_search(search_listener_t *sl, int search_id)
{
    struct search_request *sreq;

    if(sl == NULL)
        return;

    if(search_id == 0)
    {
        while((sreq = TAILQ_FIRST(&sl->search_request_head)) != NULL)
        {
            DEBUG("forgetting search ID %i", sreq->id);
            TAILQ_REMOVE(&sl->search_request_head, sreq, link);
            sl_request_free(sreq);
        }
    }
    else
    {
        struct search_request *next;
        for(sreq = TAILQ_FIRST(&sl->search_request_head); sreq; sreq = next)
        {
            next = TAILQ_NEXT(sreq, link);
            if(sreq->id == search_id)
            {
                DEBUG("forgetting search ID %i", sreq->id);
                TAILQ_REMOVE(&sl->search_request_head, sreq, link);
                sl_request_free(sreq);
            }
        }   
    }
}

