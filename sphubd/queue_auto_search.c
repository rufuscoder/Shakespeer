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

#include "sys_queue.h"
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <event.h>

#include "globals.h"
#include "log.h"
#include "queue.h"
#include "search_listener.h"

#define QUEUE_MAX_RECENT_SEARCHES 30

extern struct queue_store *q_store;

/* keep a FIFO queue of the QUEUE_MAX_RECENT_SEARCHES most recent searches */
struct recent_search_entry
{
    char *tth;
    TAILQ_ENTRY(recent_search_entry) link;
};

static TAILQ_HEAD(, recent_search_entry) recent_searches_head =
    TAILQ_HEAD_INITIALIZER(recent_searches_head);
static int num_recent_searches = 0;

#ifndef TEST
static void queue_auto_search_sources_event_func(int fd, short why,
        void *data);
#endif

static bool is_recently_searched(const char *tth)
{
    struct recent_search_entry *e;
    TAILQ_FOREACH(e, &recent_searches_head, link)
    {
        if(strcmp(e->tth, tth) == 0)
        {
            return true;
        }
    }

    return false;
}

static char *queue_auto_search_get_tth(void)
{
    while(num_recent_searches > QUEUE_MAX_RECENT_SEARCHES)
    {
        /* remove the first element */
        struct recent_search_entry *e = TAILQ_FIRST(&recent_searches_head);
        TAILQ_REMOVE(&recent_searches_head, e, link);
        free(e->tth);
        free(e);

        --num_recent_searches;
    }

    int ncandidates = 0;

    /* Select targets for auto-search, ordered by urgency
     */
    queue_target_t *qt_candidate = NULL;
    struct queue_target *qt;
    TAILQ_FOREACH(qt, &q_store->targets, link)
    {
        if(qt->tth[0] == 0) /* TTH required */
            continue;

        if(qt->priority == 0) /* skip paused targets */
            continue;

        if(is_recently_searched(qt->tth))
            continue;

        ncandidates++;

        /* FIXME: check number of sources, skip for nsources >= 5 */
        /* FIXME: fix ordering dependency below */

        if(qt_candidate == NULL ||
           (qt->flags & QUEUE_TARGET_ACTIVE) == 0 ||
           qt->priority > qt_candidate->priority ||
           qt->size > qt_candidate->size)
        {
            qt_candidate = qt;
        }
    }

    char *tth = 0;

    if(qt_candidate)
    {
        tth = strdup(qt_candidate->tth);
        DEBUG("found tth [%s]", tth);
        struct recent_search_entry *e = calloc(1,
	    sizeof(struct recent_search_entry));
        e->tth = tth;
        TAILQ_INSERT_TAIL(&recent_searches_head, e, link);
        ++num_recent_searches;
    }
    else
    {
        DEBUG("found no candidates for auto-search (%i possible)",
                ncandidates);
    }

    return tth;
}

#ifndef TEST

void queue_schedule_auto_search_sources(int enable)
{
    static struct event ev;

    if(event_initialized(&ev))
    {
        evtimer_del(&ev);
    }
    else if(enable)
    {
        evtimer_set(&ev, queue_auto_search_sources_event_func, NULL);
    }

    if(enable)
    {
        struct timeval tv = {.tv_sec = 123, .tv_usec = 0};
        evtimer_add(&ev, &tv);
    }
}

static void queue_auto_search_sources_event_func(int fd, short why, void *data)
{
    /* forget the last auto-search request */
    sl_forget_search(global_search_listener, -1);

    if(!hub_is_connected())
    {
        /* not connected to any hub, skip auto-search for now */
        queue_schedule_auto_search_sources(1);
        return;
    }

    char *tth = queue_auto_search_get_tth();

    if(tth)
    {
        search_request_t *sreq = search_listener_create_search_request(
                tth, 0, SHARE_SIZE_NONE, SHARE_TYPE_TTH, -1);
        if(sreq == NULL)
        {
            WARNING("failed to create search request");
        }
        else
        {
            DEBUG("auto-searching for [%s]", tth);

            search_listener_add_request(global_search_listener, sreq);
            hub_foreach((void (*)(hub_t *, void *))hub_search, (void *)sreq);
        }
    }

    queue_schedule_auto_search_sources(1);
}

void queue_auto_search_init(void)
{
    if(global_auto_search_sources)
    {
        queue_schedule_auto_search_sources(1);
    }
}

#endif

#ifdef TEST

#include "unit_test.h"

void test_setup(void)
{
    global_working_directory = "/tmp/sp-queue_auto_search-test.d";
    system("/bin/rm -rf /tmp/sp-queue_auto_search-test.d");
    system("mkdir /tmp/sp-queue_auto_search-test.d");

    queue_init();

    fail_unless(queue_add("foo", "remote/path/to/file.img", 17471142,
                "file.img", "IP4CTCABTUE6ZHZLFS2OP5W7EMN3LMFS65H7D2Y") == 0);
}

void test_teardown(void)
{
    queue_close();
    system("/bin/rm -rf /tmp/sp-queue_auto_search-test.d");
}

int main(void)
{
    sp_log_set_level("debug");
    test_setup();

    char *tth = queue_auto_search_get_tth();
    fail_unless(tth);
    fail_unless(strcmp(tth, "IP4CTCABTUE6ZHZLFS2OP5W7EMN3LMFS65H7D2Y") == 0);

    tth = queue_auto_search_get_tth();
    fail_unless(tth == NULL);

    test_teardown();
    
    return 0;
}

#endif

