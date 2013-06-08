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

#ifndef _notification_center_h_
#define _notification_center_h_

#include "sys_queue.h"

typedef struct notification_center nc_t;
typedef struct nc_observer nc_observer_t;
typedef void (*nc_callback_t)(nc_t *nc, const char *channel,
        void *data, void *user_data);

struct nc_observer
{
    char *channel;
    nc_callback_t callback;
    void *user_data;
    LIST_ENTRY(nc_observer) next;
};

struct notification_center
{
    LIST_HEAD(, nc_observer) observers;
};

nc_t *nc_new(void);
nc_t *nc_default(void);

void nc_add_observer(nc_t *nc, const char *channel,
        nc_callback_t callback, void *user_data);
void nc_remove_observer(nc_t *nc, const char *channel,
        nc_callback_t callback);
void nc_send_notification(nc_t *nc, const char *channel, void *data);

#endif

