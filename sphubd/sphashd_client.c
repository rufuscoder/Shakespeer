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

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "base64.h"
#include "base32.h"
#include "log.h"
#include "io.h"

#include "notifications.h"
#include "sphashd_client.h"
#include "globals.h"

/* Request this many files each time we need to feed the hashing server */
#define HASH_BATCH_SIZE 100

static hs_t *global_hash_server = 0;
static int got_new_files = 1;

int hs_send_string(hs_t *hs, const char *string)
{
    print_command(string, "-> (fd %i)", hs->fd);
    return bufferevent_write(hs->bufev, (void *)string, strlen(string));
}

int hs_send_command(hs_t *hs, const char *fmt, ...)
{
    return_val_if_fail(hs, -1);
    return_val_if_fail(hs->fd != -1, -1);
    return_val_if_fail(fmt, -1);

    va_list ap;
    va_start(ap, fmt);
    char *command = 0;
    int num_returned_bytes = vasprintf(&command, fmt, ap);
    if (num_returned_bytes == -1)
        DEBUG("vasprintf did not return anything");
    int rc = hs_send_string(hs, command);
    free(command);
    va_end(ap);

    return rc;
}

static void hs_finish_file(hs_t *hs, const char *filename,
        const char *hash_base32, const char *leaves_base64,
        double mibs_per_sec)
{
    return_if_fail(hs->unfinished_list);

    share_file_t *file = NULL;
    SLIST_FOREACH(file, hs->unfinished_list, link)
    {
	char *local_path = share_complete_path(file);
	int rc = strcmp(filename, local_path);
	free(local_path);

	if(rc == 0)
            break;
    }

    if(file == NULL)
    {
        WARNING("hashed file not in unfinished list, ignoring");
    }
    else
    {
        SLIST_REMOVE(hs->unfinished_list, file, share_file, link);

	nc_send_tth_available_notification(nc_default(),
	    file, hash_base32, leaves_base64, mibs_per_sec);
    }

    if(SLIST_FIRST(hs->unfinished_list) == NULL && !hs->paused)
	hs_start_hash_feeder();
}

static int hashd_cb_add_hash(hs_t *hs, const char *filename,
        const char *hash_base32, const char *leaves_base64,
        double mibs_per_sec)
{
    return_val_if_fail(hs, -1);
    return_val_if_fail(filename, -1);
    return_val_if_fail(hash_base32, -1);

    hs_finish_file(hs, filename, hash_base32, leaves_base64, mibs_per_sec);

    return 0;
}

static int hashd_cb_fail_hash(hs_t *hs, const char *filename)
{
    return_val_if_fail(hs, -1);
    return_val_if_fail(filename, -1);

    INFO("tth failed for file '%s'", filename);
    hs_finish_file(hs, filename, NULL, NULL, 0.0);

    return 0;
}

static void hs_close_connection(hs_t *hs)
{
    return_if_fail(hs);

    DEBUG("closing down hash server %p", hs);
    if(hs->bufev)
    {
        bufferevent_free(hs->bufev);
    }
    if(hs->fd != -1)
    {
        close(hs->fd);
        hs->fd = -1;
    }
}

static void hs_restart_connection(hs_t *hs)
{
    return_if_fail(hs);

    hs_start();
    if(!hs->paused)
        hs_start_hash_feeder();
}

static void hs_in_event(struct bufferevent *bufev, void *data)
{
    hs_t *hs = data;
    return_if_fail(hs);

    while(1)
    {
        char *cmd = io_evbuffer_readline(EVBUFFER_INPUT(bufev));
        if(cmd == NULL)
        {
            break;
        }
        print_command(cmd, "<- (fd %d)", hs->fd);
        hs_dispatch_command(cmd, "$", 1, hs);
        free(cmd);
    }
}

static void hs_out_event(struct bufferevent *bufev, void *data)
{
}

static void hs_err_event(struct bufferevent *bufev, short why, void *data)
{
    hs_t *hs = data;

    WARNING("why = 0x%02X", why);
    hs_close_connection(hs);
}

/* feed the hashing server with a batch of files
 */
static void hs_feed_server(hs_t *hs)
{
    return_if_fail(hs->unfinished_list == NULL ||
            SLIST_FIRST(hs->unfinished_list) == NULL);

    /* get a batch of files from the share database */
    free(hs->unfinished_list);
    hs->unfinished_list = share_next_unhashed(global_share,
            HASH_BATCH_SIZE);
    if(hs->unfinished_list == NULL)
    {
        if(got_new_files)
        {
            nc_send_hashing_complete_notification(nc_default());
        }
        got_new_files = 0;
    }
    else
    {
        got_new_files = 1;

        /* and send them all to the hashing server */
        int num_files = 0;
        share_file_t *file;
        SLIST_FOREACH(file, hs->unfinished_list, link)
        {
	    char *local_path = share_complete_path(file);
            hs_send_add(hs, local_path);
	    free(local_path);
            ++num_files;
        }
        DEBUG("added %i files", num_files);

        if(num_files < HASH_BATCH_SIZE)
        {
            /* We got less files than we asked for, which means that we've
             * hashed all files. So we wait to add more files until next
             * registering event occurs (should another directory be added).
             */
            hs->finished = true;
        }
    }
}

void hs_start_hash_feeder(void)
{
    return_if_fail(global_hash_server);

    global_hash_server->finished = false;
    global_hash_server->paused = false;

    if(global_hash_server->fd == -1)
    {
        WARNING("lost connection to sphashd");
        hs_restart_connection(global_hash_server);
        return;
    }

    if(global_hash_server->unfinished_list == NULL ||
       SLIST_FIRST(global_hash_server->unfinished_list) == NULL)
    {
        hs_feed_server(global_hash_server);
    }
}

static void hs_handle_share_scan_finished_notification(
        nc_t *nc,
        const char *channel,
        nc_share_scan_finished_t *data,
        void *user_data)
{
	return_if_fail(global_hash_server);

	if(global_hash_server->paused)
	{
		DEBUG("finished scanning [%s], but hash feeder paused",
			data->path);
	}
	else
	{
		DEBUG("finished scanning [%s], starting hash feeder", data->path);
		hs_start_hash_feeder();
	}
}

static void hs_handle_will_remove_share_notification(
        nc_t *nc,
        const char *channel,
        nc_will_remove_share_t *notification,
        void *user_data)
{
	/* If a shared path is being removed, we must stop hashing, since we
	 * keep a list of share_file_t pointers that might be freed.
	 */
	hs_stop();
}

static void hs_handle_did_remove_share_notification(
        nc_t *nc,
        const char *channel,
        nc_did_remove_share_t *notification,
        void *user_data)
{
	return_if_fail(global_hash_server);

	/* Once a shared path has been removed, restart hashing other files.
	 */
	if(global_hash_server->paused)
	{
		DEBUG("share [%s] removed, but hash feeder paused",
			notification->local_root);
	}
	else
	{
		DEBUG("share [%s] removed, starting hash feeder",
			notification->local_root);
		hs_start_hash_feeder();
	}
}

int hs_start(void)
{
    char *sphashd_socket_filename = 0;
    char *sphashd_path = 0;
    int num_returned_bytes;

    num_returned_bytes = asprintf(&sphashd_socket_filename, "%s/sphashd", global_working_directory);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
    num_returned_bytes = asprintf(&sphashd_path, "%s/sphashd", argv0_path);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");

    int fd = io_exec_and_connect_unix(sphashd_socket_filename,
            sphashd_path, global_working_directory);
    free(sphashd_path);
    free(sphashd_socket_filename);
    if(fd == -1)
    {
        WARNING("failed to connect to sphashd");
        return -1;
    }

    io_set_blocking(fd, 0);

    global_hash_server = hs_init();
    global_hash_server->fd = fd;
    global_hash_server->cb_add_hash = hashd_cb_add_hash;
    global_hash_server->cb_fail_hash = hashd_cb_fail_hash;

    DEBUG("adding hashing server on file descriptor %d", fd);
    global_hash_server->bufev = bufferevent_new(fd,
            hs_in_event, hs_out_event, hs_err_event, global_hash_server);
    bufferevent_enable(global_hash_server->bufev, EV_READ | EV_WRITE);

    /* register handler for share scan finished notification */
    nc_add_share_scan_finished_observer(nc_default(),
            hs_handle_share_scan_finished_notification, NULL);

    nc_add_will_remove_share_observer(nc_default(),
            hs_handle_will_remove_share_notification, NULL);
    nc_add_did_remove_share_observer(nc_default(),
            hs_handle_did_remove_share_notification, NULL);

    hs_set_prio(global_hash_prio);

    return 0;
}

int hs_stop(void)
{
    hs_send_abort(global_hash_server);

    if(global_hash_server->unfinished_list)
    {
        while(SLIST_FIRST(global_hash_server->unfinished_list) != NULL)
        {
            SLIST_REMOVE_HEAD(global_hash_server->unfinished_list, link);
        }
    }

    return 0;
}

void hs_pause(void)
{
	INFO("hashing paused");
	hs_stop();
	global_hash_server->paused = true;
}

void hs_resume(void)
{
	INFO("hashing resumed");
	hs_start_hash_feeder();
}

void hs_shutdown(void)
{
    return_if_fail(global_hash_server);
    hs_send_shutdown(global_hash_server);
    hs_close_connection(global_hash_server);

    free(global_hash_server->unfinished_list);
    free(global_hash_server);
    global_hash_server = 0;
}

void hs_set_prio(unsigned int prio)
{
    unsigned int prio_delays[5] = {0, 10000, 50000, 100000, 500000};
    if(prio > 4)
    {
        prio = 4;
    }
    global_hash_prio = prio;
    hs_send_set_delay(global_hash_server, prio_delays[prio]);
}

