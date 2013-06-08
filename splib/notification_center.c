/*
 * Copyright (c) 2006 Martin Hedenfalk <martin@bzero.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "notification_center.h"

nc_t *nc_new(void)
{
    nc_t *nc = calloc(1, sizeof(nc_t));
    return nc;
}

nc_t *nc_default(void)
{
    static nc_t *default_nc = NULL;
    if(default_nc == NULL)
    {
        default_nc = nc_new();
    }
    return default_nc;
}

static nc_observer_t *nc_observer_new(const char *channel,
        nc_callback_t callback, void *user_data)
{
    nc_observer_t *ob = calloc(1, sizeof(nc_observer_t));
    ob->channel = strdup(channel);
    ob->callback = callback;
    ob->user_data = user_data;

    return ob;
}

static void nc_observer_free(nc_observer_t *ob)
{
    if(ob)
    {
        free(ob->channel);
        free(ob);
    }
}

void nc_add_observer(nc_t *nc, const char *channel,
        nc_callback_t callback, void *user_data)
{
    assert(nc);
    assert(channel);
    assert(callback);

    nc_observer_t *ob = nc_observer_new(channel, callback, user_data);
    LIST_INSERT_HEAD(&nc->observers, ob, next);
}

void nc_remove_observer(nc_t *nc, const char *channel, nc_callback_t callback)
{
    assert(nc);
    assert(channel);
    assert(callback);

    nc_observer_t *ob;
    LIST_FOREACH(ob, &nc->observers, next)
    {
        if(ob->callback == callback && strcmp(ob->channel, channel) == 0)
        {
            LIST_REMOVE(ob, next);
            nc_observer_free(ob);
            break;
        }
    }
}

void nc_send_notification(nc_t *nc, const char *channel, void *data)
{
    assert(nc);
    assert(channel);

    nc_observer_t *ob;
    LIST_FOREACH(ob, &nc->observers, next)
    {
        if(strcmp(channel, ob->channel) == 0)
        {
            ob->callback(nc, channel, data, ob->user_data);
        }
    }
}

#ifdef TEST

#include <stdio.h>
#include "unit_test.h"

int sample_callback_called = 0;

void sample_callback(nc_t *nc, const char *channel, void *data, void *user_data)
{
    fail_unless(nc);
    fail_unless(nc == nc_default());
    fail_unless(channel);
    fail_unless(user_data == nc);
    fail_unless(strcmp(channel, "sample channel") == 0);

    fail_unless(data);
    fail_unless(strcmp((const char *)data, "sample data") == 0);
    ++sample_callback_called;
}

int main(int argc, char **argv)
{
    /* create the shared, default notification center */
    nc_t *nc = nc_default();

    /* send a notification without any observers */
    nc_send_notification(nc, "sample channel", "sample data");
    fail_unless(sample_callback_called == 0);

    /* add an observers */
    nc_add_observer(nc, "sample channel", sample_callback, nc);

    /* notify all observers */
    nc_send_notification(nc, "sample channel", "sample data");
    fail_unless(sample_callback_called == 1);

    /* remove the observer */
    nc_remove_observer(nc, "sample channel", sample_callback);
    nc_send_notification(nc, "sample channel", "sample data");
    fail_unless(sample_callback_called == 1);

    return 0;
}

#endif

