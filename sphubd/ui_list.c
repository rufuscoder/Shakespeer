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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/time.h>
#include <event.h>

#include "ui.h"
#include "log.h"
#include "share.h"
#include "notifications.h"
#include "hub.h"
#include "globals.h"

LIST_HEAD(, ui) ui_list_head;

static void handle_hashing_complete_notification(nc_t *nc,
        const char *channel,
        nc_hashing_complete_t *hashing_complete_data, void *user_data)
{
    ui_send_status_message(NULL, NULL, "finished hashing all files");
}

void ui_share_stats_update_event(int fd, short why, void *user_data)
{
    ui_send_share_stats_for_root(NULL, NULL);
}

void ui_schedule_share_stats_update(void)
{
    static struct event ev;

    if(!event_initialized(&ev))
    {
        evtimer_set(&ev, ui_share_stats_update_event, NULL);
    }

    if(!event_pending(&ev, EV_TIMEOUT, NULL))
    {
        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
        event_add(&ev, &tv);
    }
}

static void handle_tth_available_notification(nc_t *nc,
        const char *channel,
        nc_tth_available_t *data, void *user_data)
{
    share_file_t *file = data->file;

    if(data->tth == NULL)
    {
	ui_send_status_message(NULL, NULL, "hashing failed for %s%s",
	    file->mp->local_root, file->partial_path);
    }
    else
    {
	const char *filename = strrchr(file->partial_path, '/');
	if(filename++ == NULL)
	    filename = file->partial_path;

	ui_send_status_message(NULL, NULL, "finished hashing %s (%.2lf MiB/s)",
		filename, data->mibs_per_sec);

	hub_set_need_myinfo_update(true);
	ui_schedule_share_stats_update();
    }
}

static void handle_share_scan_finished_notification(nc_t *nc,
        const char *channel,
        nc_share_scan_finished_t *notification, void *user_data)
{
    ui_send_status_message(NULL, NULL, "Finished scanning %s",
            notification->path);

    if(global_init_completion < 200)
    {
	static int nshares = 0;
	if(++nshares >= global_expected_shared_paths)
	{
	    global_init_completion = 200;
	    ui_send_init_completion(NULL, global_init_completion);
	}
    }
}

static void handle_share_file_added_notification(nc_t *nc,
        const char *channel,
        nc_share_file_added_t *data, void *user_data)
{
    ui_schedule_share_stats_update();
}


static void handle_share_duplicate_found_notification(nc_t *nc,
        const char *channel,
        nc_share_duplicate_found_t *data, void *user_data)
{
    ui_send_share_duplicate_found(NULL, data->path);
}

static void handle_filelist_added_notification(nc_t *nc,
        const char *channel,
        nc_filelist_added_t *data, void *user_data)
{
    ui_send_queue_add_filelist(NULL, data->nick, data->priority);
}

static void handle_filelist_removed_notification(nc_t *nc,
        const char *channel,
        nc_filelist_removed_t *data, void *user_data)
{
    ui_send_queue_remove_filelist(NULL, data->nick);
}

static void handle_queue_source_removed_notification(nc_t *nc,
        const char *channel,
        nc_queue_source_removed_t *data, void *user_data)
{
    ui_send_queue_remove_source(NULL, data->target_filename, data->nick);
}

static void handle_queue_target_removed_notification(nc_t *nc,
        const char *channel,
        nc_queue_target_removed_t *data, void *user_data)
{
    ui_send_queue_remove_target(NULL, data->target_filename);
}

static void handle_filelist_finished_notification(nc_t *nc,
        const char *channel,
        nc_filelist_finished_t *notification, void *user_data)
{
    if(notification->auto_matched == 0)
    {
        ui_send_filelist_finished(NULL, notification->hub_address,
                notification->nick, notification->filename);
    }
}

static void handle_queue_source_added_notification(nc_t *nc,
        const char *channel,
        nc_queue_source_added_t *data, void *user_data)
{
    ui_send_queue_add_source(NULL,
            data->target_filename, data->nick, data->source_filename);
}

static void handle_queue_target_added_notification(nc_t *nc,
        const char *channel,
        nc_queue_target_added_t *data, void *user_data)
{
    ui_send_queue_add_target(NULL, data->target_filename, data->size,
            data->tth, data->priority);
}

static void handle_queue_directory_added_notification(nc_t *nc,
        const char *channel,
        nc_queue_directory_added_t *data, void *user_data)
{
    ui_send_queue_add_directory(NULL, data->target_directory, data->nick);
}

static void handle_queue_directory_removed_notification(nc_t *nc,
        const char *channel,
        nc_queue_directory_removed_t *data, void *user_data)
{
    ui_send_queue_remove_directory(NULL, data->target_directory);
}

static void handle_queue_priority_changed_notification(nc_t *nc,
        const char *channel,
        nc_queue_priority_changed_t *data, void *user_data)
{
    ui_send_set_priority(NULL, data->target_filename, data->priority);
}

static void handle_did_remove_share_notification( nc_t *nc,
        const char *channel,
        nc_did_remove_share_t *notification, void *user_data)
{
    if(!notification->is_rescan)
    {
        ui_send_share_stats_for_root(NULL, notification->local_root);
        hub_set_need_myinfo_update(true);
    }
}

static void handle_extra_slot_granted_notification(nc_t *nc,
    const char *channel,
    nc_extra_slot_granted_t *notification, void *user_data)
{
    return_if_fail(notification);

    hub_t *hub = hub_find_by_nick(notification->nick);
    if(hub)
    {
        user_t *user = hub_lookup_user(hub, notification->nick);
        if(user)
        {
            user->extra_slots = notification->extra_slots;
            ui_send_user_update(NULL, hub->address, notification->nick,
		    user->description,
                    user->tag, user->speed, user->email, user->shared_size,
                    user->is_operator, user->extra_slots);
        }
    }
}

void ui_list_init(void)
{
    LIST_INIT(&ui_list_head);

    nc_add_tth_available_observer(nc_default(),
            handle_tth_available_notification, NULL);
    nc_add_hashing_complete_observer(nc_default(),
            handle_hashing_complete_notification, NULL);
    nc_add_filelist_added_observer(nc_default(),
            handle_filelist_added_notification, NULL);
    nc_add_filelist_removed_observer(nc_default(),
            handle_filelist_removed_notification, NULL);
    nc_add_queue_source_removed_observer(nc_default(),
            handle_queue_source_removed_notification, NULL);
    nc_add_queue_target_removed_observer(nc_default(),
            handle_queue_target_removed_notification, NULL);
    nc_add_queue_source_added_observer(nc_default(),
            handle_queue_source_added_notification, NULL);
    nc_add_queue_target_added_observer(nc_default(),
            handle_queue_target_added_notification, NULL);
    nc_add_queue_directory_added_observer(nc_default(),
            handle_queue_directory_added_notification, NULL);
    nc_add_queue_directory_removed_observer(nc_default(),
            handle_queue_directory_removed_notification, NULL);
    nc_add_queue_priority_changed_observer(nc_default(),
            handle_queue_priority_changed_notification, NULL);
    nc_add_filelist_finished_observer(nc_default(),
            handle_filelist_finished_notification, NULL);
    nc_add_share_scan_finished_observer(nc_default(),
            handle_share_scan_finished_notification, NULL);
    nc_add_share_file_added_observer(nc_default(),
            handle_share_file_added_notification, NULL);
	nc_add_share_duplicate_found_observer(nc_default(),
			handle_share_duplicate_found_notification, NULL);
    nc_add_did_remove_share_observer(nc_default(),
            handle_did_remove_share_notification, NULL);
    nc_add_extra_slot_granted_observer(nc_default(),
	    handle_extra_slot_granted_notification, NULL);
}

void ui_add(ui_t *ui)
{
    if(ui)
    {
        LIST_INSERT_HEAD(&ui_list_head, ui, next);
    }
}

void ui_free(ui_t *ui)
{
    if(ui)
    {
        free(ui);
    }
}

void ui_remove(ui_t *ui)
{
    assert(ui);
    LIST_REMOVE(ui, next);
}

void ui_foreach(void (*func)(ui_t *, void *), void *user_data)
{
    ui_t *ui, *next;
    for(ui = LIST_FIRST(&ui_list_head); ui; ui = next)
    {
        next = LIST_NEXT(ui, next);
        func(ui, user_data);
    }
}

int ui_send_string(ui_t *ui, const char *string)
{
    print_command(string, "-> (fd %i)", ui->fd);
    return bufferevent_write(ui->bufev, (void *)string, strlen(string));
}

static void ui_broadcast_command_GFunc(ui_t *ui, void *user_data)
{
    const char *command = user_data;
    ui_send_string(ui, command);
}

static void ui_vsend_command(ui_t *ui, const char *fmt, va_list ap)
{
    char *command = 0;
    int num_returned_bytes = vasprintf(&command, fmt, ap);
    if (num_returned_bytes == -1)
        DEBUG("vasprintf did not return anything");
    /* XXX: memory leak if this fails */
    /* return_if_fail(g_utf8_validate(command, -1, NULL)); */
    if(ui)
    {
        ui_send_string(ui, command);
    }
    else
    {
        ui_foreach(ui_broadcast_command_GFunc, command);
    }

    free(command);
}

int ui_send_command(ui_t *ui, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ui_vsend_command(ui, fmt, ap);
    va_end(ap);
    return 0;
}

