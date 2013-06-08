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

#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "globals.h"
#include "client.h"
#include "ui.h"
#include "io.h"
#include "queue.h"
#include "nmdc.h"
#include "encoding.h"
#include "log.h"
#include "xstr.h"

static LIST_HEAD(, cc) cc_list_head;

void cc_list_init(void)
{
    LIST_INIT(&cc_list_head);
}

cc_t *cc_new(int fd, hub_t *hub)
{
    cc_t *cc = calloc(1, sizeof(cc_t));

    cc->hub = hub;
    cc->fd = fd;
    cc->last_activity = time(0);
    cc->local_fd = -1;

    return cc;
}

void cc_free(cc_t *cc)
{
    DEBUG("cc = %p", cc);
    if(cc)
    {
        if(cc->fd != -1)
        {
            DEBUG("closing file descriptor %d", cc->fd);
            if(close(cc->fd) != 0)
            {
                INFO("close(): %s", strerror(errno));
            }
        }

        if(cc->slot_state == SLOT_EXTRA)
        {
            hub_free_upload_slot(cc->hub, cc->nick, cc->slot_state);
        }

        if(cc->state == CC_STATE_BUSY || cc->state == CC_STATE_REQUEST)
        {
            if(cc->direction == CC_DIR_UPLOAD && cc->local_filename)
            {
                if(cc->slot_state != SLOT_EXTRA)
                {
                    hub_free_upload_slot(cc->hub, cc->nick, cc->slot_state);
                }
                if(cc->state == CC_STATE_BUSY)
                {
                    ui_send_transfer_aborted(NULL, cc->local_filename);
                }
            }
            else if(cc->direction && cc->current_queue)
            {
		queue_set_active(cc->current_queue, 0);
                if(cc->state == CC_STATE_BUSY)
                {
                    ui_send_transfer_aborted(NULL,
                            cc->current_queue->target_filename);
                }
                queue_free(cc->current_queue);
                cc->current_queue = NULL;
            }
        }

        if(cc->current_queue)
        {
            WARNING("cc->current_queue still allocated (state inconsistency?)");
            queue_free(cc->current_queue);
        }

        free(cc->local_filename);
        free(cc->nick);
        free(cc);
    }
}

void cc_cancel_transfer(const char *local_filename)
{
    cc_t *cc = cc_find_by_local_filename(local_filename);
    if(cc)
    {
        cc_close_connection(cc);
    }
    else
    {
        DEBUG("didn't find any connection for '%s'", local_filename);
    }
}

void cc_cancel_directory_transfers(const char *target_directory)
{
    while(*target_directory == '/')
        ++target_directory;

    DEBUG("cancelling transfers for directory [%s]", target_directory);

    cc_t *cc = NULL;
    do
    {
        cc = cc_find_by_target_directory(target_directory);
        if(cc)
        {
            DEBUG("cancelling directory transfer with nick [%s]", cc->nick);
            if(cc->direction == CC_DIR_DOWNLOAD)
            {
                return_if_fail(cc->current_queue);
                /* FIXME: why do we pause the download? */
                queue_set_priority(cc->current_queue->target_filename, 0);
            }
            cc_close_connection(cc);
        }
    } while(cc);
}

int cc_send_string(cc_t *cc, const char *string)
{
    print_command(string, "-> (fd %i)", cc->fd);
    cc->last_activity = time(0);
    return bufferevent_write(cc->bufev, (void *)string, strlen(string));
}

static int cc_send_command_internal(cc_t *cc, int convert_from_utf8,
        const char *fmt, va_list ap)
{
    return_val_if_fail(cc, -1);
    return_val_if_fail(fmt, -1);

    int rc = -1;
    char *command = 0;
    int num_returned_bytes = vasprintf(&command, fmt, ap);
    if (num_returned_bytes == -1)
        DEBUG("vasprintf did not return anything");
    if (convert_from_utf8) {
        return_val_if_fail(cc->hub, -1);
        char *command_encoded = str_utf8_to_escaped_legacy(command, cc->hub->encoding);
        rc = cc_send_string(cc, command_encoded);
        free(command_encoded);
    }
    else
        rc = cc_send_string(cc, command);

    free(command);

    return rc;
}

int cc_send_command(cc_t *cc, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    int rc = cc_send_command_internal(cc, 1, fmt, ap);
    va_end(ap);

    return rc;
}

int cc_send_command_as_is(cc_t *cc, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    int rc = cc_send_command_internal(cc, 0, fmt, ap);
    va_end(ap);

    return rc;
}

void cc_close_connection(cc_t *cc)
{
    DEBUG("closing down cc on fd %i", cc->fd);

    if(cc->bufev)
        bufferevent_free(cc->bufev);

    if(cc->fd != -1)
    {
        close(cc->fd);
        cc->fd = -1;
    }

    if(event_initialized(&cc->handshake_timer_event))
        event_del(&cc->handshake_timer_event);

    if(cc->local_fd != -1)
        close(cc->local_fd);

    INFO("removing client connection with nick [%s]",
            cc->nick ? cc->nick : "unknown");

    if(cc->nick && cc->state >= CC_STATE_READY)
    {
        ui_send_connection_closed(NULL, cc->nick, cc->direction);
    }

    LIST_REMOVE(cc, next);

    cc_free(cc);
}

void cc_close_all_connections(void)
{
    cc_t *cc;
    for(cc = LIST_FIRST(&cc_list_head); cc != NULL;)
    {
        cc_t *next = LIST_NEXT(cc, next);
        cc_close_connection(cc);
        cc = next;
    }
}

void cc_close_all_on_hub(hub_t *hub)
{
    cc_t *cc;
    for(cc = LIST_FIRST(&cc_list_head); cc != NULL;)
    {
        cc_t *next = LIST_NEXT(cc, next);
        if(cc->hub == hub)
        {
            cc_close_connection(cc);
        }
        cc = next;
    }
}

void cc_in_event(struct bufferevent *bufev, void *data)
{
    cc_t *cc = data;
    return_if_fail(cc);

    cc->last_activity = time(0);

    if(cc->state == CC_STATE_BUSY && cc->direction == CC_DIR_DOWNLOAD)
    {
        cc_download_read(cc);
        return;
    }

    while(1)
    {
        char *cmd = io_evbuffer_readline(EVBUFFER_INPUT(bufev));
        if(cmd == NULL)
        {
            break;
        }
        print_command(cmd, "<- (fd %d)", cc->fd);
        if(strcmp(cmd, "ping") == 0)
        {
            DEBUG("received ping, sending pong");
            cc_send_command_as_is(cc, "pong|");
            free(cmd);
            return;
        }

        int rc = client_execute_command(cc->fd, data, cmd);
        free(cmd);
        if(rc < 0)
        {
            WARNING("command [%s] returned -1, closing connection on fd %i",
                    cmd, cc->fd);
            cc_close_connection(cc);
            break;
        }

        if(fcntl(cc->fd, F_GETFL, 0) < 0 && errno == EBADF)
        {
            WARNING("fd %i closed, breaking loop", cc->fd);
            break;
        }
        
        if(rc != 0)
        {
            break;
        }
    }
}

/* The out event is called when the output buffer is drained. This means
 * we need to read more data from the file.
 */
void cc_out_event(struct bufferevent *bufev, void *data)
{
    cc_t *cc = data;
    return_if_fail(cc);

    if(cc->state == CC_STATE_BUSY && cc->direction == CC_DIR_UPLOAD)
    {
        cc->last_transfer_activity = time(0);

        if(cc->bytes_done >= cc->bytes_to_transfer)
        {
            cc_finish_upload(cc);
        }
        else
        {
            static char buf[8192];
            size_t nbytes = sizeof(buf);
            if(cc->bytes_done + nbytes > cc->bytes_to_transfer)
            {
		/* this is the last chunk */
                nbytes = cc->bytes_to_transfer - cc->bytes_done;
            }

            ssize_t bytes_read = cc_upload_read(cc, buf, nbytes);
            if(bytes_read == -1)
            {
                WARNING("read failed: %s", strerror(errno));
                cc_close_connection(cc);
            }
            else
            {
                bufferevent_write(bufev, buf, bytes_read);
                cc->bytes_done += bytes_read;
            }
        }
    }
}

static void cc_err_event(struct bufferevent *bufev, short why, void *data)
{
    cc_t *cc = data;

    if((why & EVBUFFER_EOF) == EVBUFFER_EOF)
	INFO("end-of-file on connection %i", cc->fd);
    else
    {
	INFO("error condition 0x%02X on connection %i (errno = %i/%s)",
	    why, cc->fd, errno, strerror(errno));
    }

    cc_close_connection(cc);
}

static void cc_expire_handshake_timer_event_func(int fd, short condition, void *data)
{
    cc_t *cc = data;

    return_if_fail(cc);
    WARNING("client handshake timeout exceeded on fd %i (nick %s)", cc->fd, cc->nick ? cc->nick : "unknown");
    cc_close_connection(cc);
}

/* Add a socket for a client connection to the main event loop.
 *
 * INCOMING_CONNECTION is true if the client connection was initiated by
 * another peer (ie, as a response to a $ConnectToMe). It is the connecting
 * peer that sends the first requests.
 */
static void cc_add_channel(int fd, bool incoming_connection, hub_t *hub,
        struct sockaddr_in *peer_addr)
{
    /* Create a new client connection struct */
    cc_t *cc = cc_new(fd, hub);
    cc->incoming_connection = incoming_connection;

    io_set_blocking(fd, 0);

    DEBUG("adding file descriptor %d to client connections", fd);
    cc->bufev = bufferevent_new(fd, cc_in_event, cc_out_event, cc_err_event, cc);
    bufferevent_enable(cc->bufev, EV_READ);

    LIST_INSERT_HEAD(&cc_list_head, cc, next);

    if(peer_addr)
    {
        memcpy(&cc->addr, peer_addr, sizeof(struct sockaddr_in));
    }

    if(cc->incoming_connection == false)
    {
        return_if_fail(hub);
        cc_send_command(cc, "$MyNick %s|", hub->me->nick);
        char *lock_pk = nmdc_makelock_pk(global_id_lock, global_id_version);
        cc_send_command(cc, "$Lock %s|", lock_pk);
        free(lock_pk);
    }

    /* add a timer to close the connection if handshake takes too long time to
     * complete */
    evtimer_set(&cc->handshake_timer_event,
            cc_expire_handshake_timer_event_func, cc);
    struct timeval tv = {.tv_sec = 90, .tv_usec = 0};
    evtimer_add(&cc->handshake_timer_event, &tv);
}

cc_t *cc_find_by_nick_and_direction(const char *nick, cc_direction_t direction)
{
    return_val_if_fail(nick, NULL);

    cc_t *cc;
    LIST_FOREACH(cc, &cc_list_head, next)
    {
        if(cc->nick && strcmp(nick, cc->nick) == 0 && cc->direction == direction)
        {
            return cc;
        }
    }

    return NULL;
}

cc_t *cc_find_by_nick(const char *nick)
{
    return cc_find_by_nick_and_direction(nick, CC_DIR_DOWNLOAD);
}

cc_t *cc_find_by_local_filename(const char *local_filename)
{
    cc_t *cc;
    LIST_FOREACH(cc, &cc_list_head, next)
    {
        if((cc->local_filename && /* upload ? */
            strcmp(local_filename, cc->local_filename) == 0) ||
           (cc->current_queue && /* download ? */
            strcmp(local_filename, cc->current_queue->target_filename) == 0))
        {
            return cc;
        }
    }

    return NULL;
}

cc_t *cc_find_by_target_directory(const char *target_directory)
{
    char *x;
    if(str_has_suffix(target_directory, "/"))
        x = strdup(target_directory);
    else {
        int num_returned_bytes = asprintf(&x, "%s/", target_directory);
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
    }

    DEBUG("looking for transfers with prefix [%s]", x);

    cc_t *cc = NULL;
    LIST_FOREACH(cc, &cc_list_head, next)
    {
        if((cc->local_filename && /* upload ? */
            str_has_prefix(cc->local_filename, x)) ||
           (cc->current_queue && /* download ? */
            str_has_prefix(cc->current_queue->target_filename, x)))
        {
            break;
        }
    }

    free(x);

    return cc;
}

/* This is called every x seconds from sphubd to start downloads. Also
 * disconnects inactive connections.
 */
void cc_trigger_download(void)
{
    time_t now = time(0);

    cc_t *cc;
    for(cc = LIST_FIRST(&cc_list_head); cc != NULL;)
    {
        cc_t *next = LIST_NEXT(cc, next);

        if(cc->direction == CC_DIR_DOWNLOAD && cc->state == CC_STATE_READY)
        {
            /* Found an idle client connection in download mode */
            cc_request_download(cc);
        }

        /* disconnect from clients after inactivity */
        if(cc->state == CC_STATE_READY && now - cc->last_activity > 180)
        {
            INFO("disconnecting from nick %s after 180 seconds of inactivity",
                    cc->nick);
            cc_close_connection(cc);
        }

        cc = next;
    }
}

static void cc_send_transfer_stats(int fd, short condition, void *data)
{
    time_t now = time(0);

    cc_t *cc, *next;
    for(cc = LIST_FIRST(&cc_list_head); cc != NULL; cc = next)
    {
        next = LIST_NEXT(cc, next);

	/* skip idle connections */
        if(cc->state != CC_STATE_BUSY)
	    continue;

	/* close stalled transfers */
	unsigned idle_time = now - cc->last_transfer_activity;
	if(idle_time > CC_IDLE_TIMEOUT)
	{
	    ui_send_status_message(NULL, cc->hub->address,
		    "Aborting transfer with nick '%s'"
		    " after %u seconds idle time",
		    cc->nick, CC_IDLE_TIMEOUT);
	    cc_close_connection(cc);
	    continue;
	}

	unsigned duration = now - cc->transfer_start_time;
	unsigned bytes_per_sec = cc->bytes_done / (duration ? duration : 1);

	const char *target = NULL;
	if(cc->direction == CC_DIR_DOWNLOAD && cc->current_queue)
	{
	    target = cc->current_queue->target_filename;
	}
	else if(cc->direction == CC_DIR_UPLOAD)
	{
	    target = cc->local_filename;
	}

	if(target)
	{
	    ui_send_transfer_stats(NULL, target, cc->bytes_done + cc->offset,
		    cc->filesize, bytes_per_sec);
	}

#if 0
	/* send stats for directories */
	if(cc->direction == CC_DIR_DOWNLOAD && cc->current_queue && cc->current_queue->is_directory)
	{
	    /* Not implemented yet. */
	}
#endif
    }

    /* re-schedule event */
    cc_set_transfer_stats_interval(-1);
}

void cc_set_transfer_stats_interval(int interval)
{
    static struct event ev;
    static int last_interval = 1;

    if(event_initialized(&ev))
    {
        evtimer_del(&ev);
    }
    else
    {
        evtimer_set(&ev, cc_send_transfer_stats, NULL);
    }

    if(interval != -1)
    {
        DEBUG("starting transfer-stats sender with interval %i seconds", interval);
        last_interval = interval;
    }
    else
    {
        interval = last_interval;
    }

    struct timeval tv = {.tv_sec = interval, .tv_usec = 0};
    evtimer_add(&ev, &tv);
}

void cc_send_ongoing_transfers(ui_t *ui)
{
    cc_t *cc;
    LIST_FOREACH(cc, &cc_list_head, next)
    {
        if(cc->state == CC_STATE_BUSY)
        {
            if(cc->direction == CC_DIR_DOWNLOAD && cc->current_queue)
            {
                ui_send_download_starting(ui, cc->hub->address, cc->nick,
                        cc->current_queue->source_filename,
                        cc->current_queue->target_filename,
                        cc->current_queue->size);
            }
            else if(cc->direction == CC_DIR_UPLOAD)
            {
                ui_send_upload_starting(ui, cc->hub->address, cc->nick,
                        cc->local_filename, cc->filesize);
            }
        }
    }
}

void cc_accept_connection(int fd, short condition, void *data)
{
    int afd = io_accept_connection(fd);
    return_if_fail(afd != -1);

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int rc = getpeername(afd, (struct sockaddr *)&addr, &addrlen);
    if(rc != 0)
    {
        WARNING("getpeername: %s (ignored)", strerror(errno));
    }

    cc_add_channel(afd, true, NULL, &addr);
}

static void cc_connect_event(int fd, int error, void *user_data)
{
    hub_t *hub = user_data;

    if(error == 0)
    {
        cc_add_channel(fd, false, hub, NULL);
    }
    else
    {
        WARNING("connection failed: %s", strerror(error));
    }
}

int cc_connect(const char *address, hub_t *hub)
{
    int rc = 0;
    xerr_t *err = 0;
    struct sockaddr_in *cc_addr = io_lookup(address, &err);
    if(cc_addr == NULL)
    {
        WARNING("failed to lookup '%s': %s", address, xerr_msg(err));
        xerr_free(err);
        return -1;
    }

    if(io_connect_async(cc_addr, cc_connect_event, hub, &err) != 0)
    {
        WARNING("failed to connect to client '%s': %s",
                address, xerr_msg(err));
        xerr_free(err);
        rc = -1;
    }
    free(cc_addr);

    return rc;
}

