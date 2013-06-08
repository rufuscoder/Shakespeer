/*
 * Copyright 2004-2006 Martin Hedenfalk <martin@bzero.se>
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
#include <sys/time.h>
#include <event.h>

#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "queue.h"
#include "log.h"

struct trigger_entry
{
    LIST_ENTRY(trigger_entry) link;
    char *nick;
    time_t when;
};

LIST_HEAD(, trigger_entry) trigger_list_head =
    LIST_HEAD_INITIALIZER(trigger_list_head);

extern struct queue_store *q_store;

static int queue_connect_interval = 60;

void queue_connect_set_interval(int seconds)
{
    queue_connect_interval = seconds;
}

static struct trigger_entry *recently_triggered(const char *nick)
{
    time_t now = time(0);

    /* check if this nick has been triggered before */
    struct trigger_entry *entry;
    LIST_FOREACH(entry, &trigger_list_head, link)
    {
        if(strcmp(entry->nick, nick) == 0)
        {
            break;
        }
    }

    if(entry == NULL || now - entry->when >= queue_connect_interval)
    {
        /* at least {queue_connect_interval} seconds since we tried to
         * start this transfer, try again
         */
        return NULL;
    }

    return entry;
}

static void call_connect_callback(queue_connect_callback_t connect_callback,
        const char *nick, void *user_data, struct trigger_entry *entry)
{
    return_if_fail(connect_callback);
    return_if_fail(nick);

    if(connect_callback(nick, user_data) == 0)
    {
        /* the callback has verified and actually attempted a
         * connection */

        if(entry == NULL)
        {
            entry = calloc(1, sizeof(struct trigger_entry));
            entry->nick = strdup(nick);
            LIST_INSERT_HEAD(&trigger_list_head, entry, link);
        }
        entry->when = time(0);
    }
    else
    {
        if(entry)
        {
            LIST_REMOVE(entry, link);
            free(entry->nick);
            free(entry);
        }
    }
}

void queue_trigger_connect_filelists(queue_connect_callback_t connect_callback,
        void *user_data)
{
    /* Loop through all (distinct) sources and look for a download (filelist)
     * that needs to be started.
     */
    struct queue_filelist *qf;
    TAILQ_FOREACH(qf, &q_store->filelists, link)
    {
        struct trigger_entry *entry = recently_triggered(qf->nick);
        if(entry)
        {
            continue;
        }

        if((qf->flags & QUEUE_TARGET_ACTIVE) == QUEUE_TARGET_ACTIVE)
        {
            /* this target is already active */
            continue;
        }

        if(qf->priority == 0)
        {
            /* this target is paused */
            continue;
        }

        call_connect_callback(connect_callback, qf->nick, user_data, entry);
    }
}

void queue_trigger_connect_targets(queue_connect_callback_t connect_callback,
        void *user_data)
{
    /* Loop through all (distinct) sources and look for a download (target)
     * that needs to be started.
     */
    struct queue_source *qs;
    TAILQ_FOREACH(qs, &q_store->sources, link)
    {
        struct trigger_entry *entry = recently_triggered(qs->nick);
        if(entry)
        {
            /* skip all targets with this nick */
            continue;
        }

        queue_target_t *qt = queue_lookup_target(qs->target_filename);
        if(qt == NULL)
        {
            WARNING("Target [%s] not available!?", qs->target_filename);
            WARNING("Removing source [%s] without target", qs->source_filename);
	    queue_remove_source(qs->target_filename, qs->nick);
            continue;
        }

        if((qt->flags && QUEUE_TARGET_ACTIVE) == QUEUE_TARGET_ACTIVE)
        {
            /* this target is already active (by another nick) */
            continue;
        }

        if(qt->priority == 0)
        {
            /* this target is paused */
            continue;
        }

        call_connect_callback(connect_callback, qs->nick, user_data, entry);
    }
}

/* This function is called every x seconds to trigger connections.
 * The connection is not immediate, and not guaranteed to succeed.
 */
void queue_trigger_connect(queue_connect_callback_t connect_callback,
        void *user_data)
{
    queue_trigger_connect_filelists(connect_callback, user_data);
    queue_trigger_connect_targets(connect_callback, user_data);
}

static void queue_trigger_connect_event_func(int fd, short why, void *data)
{
    /* Loop through all download requests to see if we should attempt to start
     * any.
     */
    queue_connect_callback_t callback_function = data;
    queue_trigger_connect(callback_function, NULL);

    /* re-schedule the event */
    queue_connect_schedule_trigger(callback_function);
}

void queue_connect_schedule_trigger(queue_connect_callback_t callback_function)
{
    static struct event queue_trigger_event;

    if(event_initialized(&queue_trigger_event))
    {
        evtimer_del(&queue_trigger_event);
    }
    else
    {
        evtimer_set(&queue_trigger_event,
                queue_trigger_connect_event_func,
                callback_function);
    }

    struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
    evtimer_add(&queue_trigger_event, &tv);
}

#ifdef TEST

#include "unit_test.h"
#include "globals.h"
#include "xstr.h"

int triggers = 0;
static int connect_trigger_callback(const char *nick, void *user_data)
{
    DEBUG("got connect trigger for nick [%s]", nick);
    ++triggers;
    fail_unless(user_data == &triggers);

    return 0;
}

void test_setup(void)
{
    global_working_directory = "/tmp/sp-queue_connect-test.d";
    system("/bin/rm -rf /tmp/sp-queue_connect-test.d");
    system("mkdir /tmp/sp-queue_connect-test.d");
    queue_init();
}

void test_teardown(void)
{
    queue_close();
    system("/bin/rm -rf /tmp/sp-queue_connect-test.d");
}

void test_trigger_target(void)
{
    test_setup();

    queue_add("apan", "source_filename", 12345, "target-filename", NULL);
    triggers = 0;
    queue_trigger_connect(connect_trigger_callback, &triggers);
    fail_unless(triggers == 1);

    test_teardown();
    puts("PASS: queue_connect: targets");
}

void test_trigger_filelist(void)
{
    test_setup();

    fail_unless(queue_add_filelist("bananen", 0) == 0);
    triggers = 0;
    queue_trigger_connect(connect_trigger_callback, &triggers);
    fail_unless(triggers == 1);

    test_teardown();
    puts("PASS: queue_connect: filelists");
}

int main(void)
{
    sp_log_set_level("debug");

    test_trigger_target();
    test_trigger_filelist();

    return 0;
}

#endif

