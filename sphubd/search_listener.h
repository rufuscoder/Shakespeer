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

#ifndef _search_listener_h_
#define _search_listener_h_

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "sys_queue.h"
#include <sys/types.h>
#include <sys/time.h>
#include <event.h>

#include "args.h"
#include "share.h"

typedef struct search_request search_request_t;
struct search_request
{
    TAILQ_ENTRY(search_request) link;
    
    share_size_restriction_t size_restriction;
    uint64_t search_size; /* the size we're searching for (sent to hub) */
    uint64_t real_size;   /* the real size we're looking for (to
                                       emulate exact search) */
    int minsize;
    share_type_t type;
    arg_t *words;        /* always in utf-8 composed form */
    int id;
    char *tth;
};

#include "hub.h"
typedef struct search_response search_response_t;
struct search_response
{
    int id;
    char *nick;
    char *filename;
    int type;
    uint64_t size;
    int openslots;
    int totalslots;
    char *tth;
    hub_t *hub;
};

typedef struct search_listener search_listener_t;

struct search_listener
{
    struct event in_event;
    int fd;
    TAILQ_HEAD(search_request_list, search_request) search_request_head;
};

search_listener_t *search_listener_new(int port);
void search_listener_close(search_listener_t *sl);
void search_listener_add_request(search_listener_t *sl, search_request_t *request);
search_request_t *search_listener_create_search_request(const char *words,
        uint64_t size,
        share_size_restriction_t size_restriction,
        share_type_t type,
        int id);
int search_listener_handle_response(search_listener_t *sl, const char *buf);
void sl_response_free(search_response_t *resp);
search_response_t *sl_parse_response(const char *buf);
void sl_forget_search(search_listener_t *sl, int search_id);
void sl_request_free(search_request_t *sreq);

#endif

