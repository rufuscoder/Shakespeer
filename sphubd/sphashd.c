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
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>

#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "share.h"
#include "tigertree.h"
#include "io.h"
#include "log.h"
#include "sphashd.h"
#include "base64.h"

#define HASHER_BUFSIZ 4*1024*1024

static char *socket_filename = 0;
static char *working_directory = NULL;
static LIST_HEAD(, hc) client_head = LIST_HEAD_INITIALIZER(client_head);
static unsigned int global_delay = 100000;
static void shutdown_sphashd_event(int fd, short condition, void *data) __attribute (( noreturn ));
static void hc_close_connection(hc_t *hc);

static int hc_send_string(hc_t *hc, const char *string)
{
    print_command(string, "-> (fd %i)", hc->fd);
    return bufferevent_write(hc->bufev, (void *)string, strlen(string));
}

int hc_send_command(hc_t *hc, const char *fmt, ...)
{
    char *cmd = 0;
    int rc;

    va_list ap;
    va_start(ap, fmt);
    int num_returned_bytes = vasprintf(&cmd, fmt, ap);
    if (num_returned_bytes == -1)
        DEBUG("vasprintf did not return anything");
    va_end(ap);

    rc = hc_send_string(hc, cmd);
    free(cmd);

    return rc;
}

static void hc_pop_current(hc_t *hc)
{
    return_if_fail(hc);

    if(hc->current_fd != -1)
    {
        close(hc->current_fd);
    }
    hc->current_fd = -1;

    tt_destroy(&hc->tth);

    if(hc->current_entry)
    {
        /* remove the current entry */
        TAILQ_REMOVE(&hc->hash_queue_head, hc->current_entry, link);
        free(hc->current_entry->filename);
        free(hc->current_entry);
        hc->current_entry = NULL;
    }
}

static void share_hasher(hc_t *hc)
{
    return_if_fail(hc);

    /* huh, why do we support multiple clients... ? */
    static struct timeval start;
    static off_t size;

    if(hc->current_fd == -1)
    {
        hc->current_entry = TAILQ_FIRST(&hc->hash_queue_head);
        if(hc->current_entry == NULL)
        {
            DEBUG("no more unhashed files");
            return;
        }

        DEBUG("starting hashing %s", hc->current_entry->filename);

        gettimeofday(&start, NULL);

        struct stat sb;
        if(stat(hc->current_entry->filename, &sb) != 0 ||
           (hc->current_fd = open(hc->current_entry->filename, O_RDONLY)) == -1)
        {
            WARNING("%s: %s", hc->current_entry->filename, strerror(errno));

            if(hc_send_fail_hash(hc, hc->current_entry->filename) != 0)
            {
                hc_close_connection(hc);
                return;
            }

            hc_pop_current(hc);
            return;
        }

        size = sb.st_size;

        tt_init(&hc->tth, tt_calc_block_size(sb.st_size, 10));
    }

    return_if_fail(hc->current_fd != -1);
    return_if_fail(hc->current_entry);

    static unsigned char buf[HASHER_BUFSIZ];

    ssize_t len = read(hc->current_fd, buf, HASHER_BUFSIZ);
    if(len > 0)
    {
        tt_update(&hc->tth, buf, len);
    }
    else
    {
        if(len < 0)
        {
            WARNING("read(fd = %i): %s", hc->current_fd, strerror(errno));

            /* unreadable?, disable the file */
            if(hc_send_fail_hash(hc, hc->current_entry->filename) != 0)
            {
                hc_close_connection(hc);
                return;
            }
        }
        else /* len == 0 */
        {
            DEBUG("finished hashing %s", hc->current_entry->filename);

            struct timeval end;
            gettimeofday(&end, NULL);
            double e = end.tv_sec + (double)end.tv_usec / 1000000;
            double s = start.tv_sec + (double)start.tv_usec / 1000000;
            double d = e - s;
            double Mps = ((double)size / (1024*1024)) / d;
            DEBUG("Hashing speed: %.1lf MiB/s", Mps);

            tt_digest(&hc->tth, NULL);
            char *hash_base32 = tt_base32(&hc->tth);
            char *leaves_base64 = tt_leafdata_base64(&hc->tth);
            hc_send_add_hash(hc, hc->current_entry->filename,
                    hash_base32, leaves_base64, Mps);
            free(leaves_base64);
            free(hash_base32);
        }

        hc_pop_current(hc);
    }
}

static void shutdown_sphashd_event(int fd, short condition, void *data)
{
    /* close all client connections and exit */
    INFO("shutting down");
    if(socket_filename && unlink(socket_filename) != 0)
    {
        WARNING("failed to unlink socket file '%s': %s", socket_filename, strerror(errno));
    }

    sp_remove_pid(working_directory, "sphashd");
    free(working_directory);

    exit(6);
}

int hc_cb_add(hc_t *hc, const char *filename)
{
    /* check if the filename is already added to the hash queue */
    struct hash_entry *entry;
    TAILQ_FOREACH(entry, &hc->hash_queue_head, link)
    {
        if(strcmp(filename, entry->filename) == 0)
        {
            /* already got this one */
            return 0;
        }
    }

    DEBUG("adding filename [%s]", filename);
    entry = calloc(1, sizeof(struct hash_entry));
    entry->filename = strdup(filename);
    TAILQ_INSERT_TAIL(&hc->hash_queue_head, entry, link);

    return 0;
}

int hc_cb_shutdown(hc_t *hc)
{
    shutdown_sphashd_event(0, EV_SIGNAL, NULL);
    return 0;
}

static void hc_free_hash_queue(hc_t *hc)
{
    struct hash_entry *entry;
    while((entry = TAILQ_FIRST(&hc->hash_queue_head)) != NULL)
    {
        TAILQ_REMOVE(&hc->hash_queue_head, entry, link);
        free(entry->filename);
        free(entry);
    }
}

int hc_cb_abort(hc_t *hc)
{
    hc_pop_current(hc);
    hc_free_hash_queue(hc);

    return 0;
}

int hc_cb_set_delay(hc_t *hc, unsigned int delay)
{
    global_delay = delay;
    return 0;
}

void hc_free(hc_t *hc)
{
    if(hc)
    {
        hc_pop_current(hc);
        hc_free_hash_queue(hc);
        free(hc);
    }
}

static void hc_close_connection(hc_t *hc)
{
    return_if_fail(hc);

    DEBUG("closing down hash client on fd %i", hc->fd);
    if(hc->bufev)
    {
        bufferevent_free(hc->bufev);
    }
    if(hc->fd != 1)
    {
        close(hc->fd);
    }

    hc_free(hc);

    /* shutdown if we loose our client */
    shutdown_sphashd_event(0, EV_SIGNAL, NULL);
}

static void hc_in_event(struct bufferevent *bufev, void *data)
{
    hc_t *hc = data;
    return_if_fail(hc);

    while(1)
    {
        char *cmd = io_evbuffer_readline(EVBUFFER_INPUT(bufev));
        if(cmd == NULL)
        {
            break;
        }
        print_command(cmd, "<- (fd %d)", hc->fd);
        hc_dispatch_command(cmd, "$", 1, hc);
        free(cmd);
    }
}

static void hc_out_event(struct bufferevent *bufev, void *data)
{
}

static void hc_err_event(struct bufferevent *bufev, short why, void *data)
{
    hc_t *hc = data;

    WARNING("why = 0x%02X", why);
    hc_close_connection(hc);
}

/* event handler for socket connections */
void hash_accept_connection(int fd, short condition, void *data)
{
    int afd = io_accept_connection(fd);
    if(afd == -1)
    {
        return;
    }

    io_set_blocking(afd, 0);

    DEBUG("adding hash client on fd %i", afd);

    hc_t *hc = hc_init();
    hc->fd = afd;
    hc->current_fd = -1;
    TAILQ_INIT(&hc->hash_queue_head);

    /* setup callbacks */
    hc->cb_add = hc_cb_add;
    hc->cb_shutdown = hc_cb_shutdown;
    hc->cb_abort = hc_cb_abort;
    hc->cb_set_delay = hc_cb_set_delay;

    /* add the socket to the event loop */
    hc->bufev = bufferevent_new(hc->fd,
            hc_in_event, hc_out_event, hc_err_event, hc);
    bufferevent_enable(hc->bufev, EV_READ | EV_WRITE);

    LIST_INSERT_HEAD(&client_head, hc, link);
}

int main(int argc, char **argv)
{
    const char *debug_level = "message";
    int c;
    while ((c = getopt(argc, argv, "w:d:")) != EOF) {
        switch (c) {
            case 'w':
                working_directory = verify_working_directory(optarg);
                break;
            case 'd':
                debug_level = optarg;
                break;
            case '?':
            default:
                /* skip unknown options */
                break;
        }
    }

    if (working_directory == NULL) {
        working_directory = get_working_directory();
    }

    sp_log_init(working_directory, "sphashd");
    sp_log_set_level(debug_level);

    sp_daemonize();
    sp_write_pid(working_directory, "sphashd");

    event_init();

    /* install signal handlers */
    struct event sigterm_event;
    signal_set(&sigterm_event, SIGTERM, shutdown_sphashd_event, NULL);
    signal_add(&sigterm_event, NULL);

    /* create a socket for sphubd connections */
    int num_returned_bytes = asprintf(&socket_filename, "%s/sphashd", working_directory);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
    int fd = io_bind_unix_socket(socket_filename); 
    if (fd == -1)
        return 1;

    /* add event handler to accept client connections */
    struct event ev;
    event_set(&ev, fd, EV_READ|EV_PERSIST, hash_accept_connection, NULL);
    event_add(&ev, NULL);

    /* set lower priority */
    if (setpriority(PRIO_PROCESS, 0 /* current process */, 10) != 0)
        WARNING("setpriority: %s (ignored)", strerror(errno));

    DEBUG("starting main loop");
    while(1) {
        event_loop(EVLOOP_NONBLOCK);

        /* loop through all client connections and see if we have something to do */
        int block = 1;
        hc_t *hc;
        LIST_FOREACH(hc, &client_head, link) {
            if (hc->current_fd != -1 || TAILQ_FIRST(&hc->hash_queue_head)) {
                share_hasher(hc);
                block = 0;
            }
        }

        if (block) {
            /* nothing to do, block for one event */
            DEBUG("blocking for next event");
            event_loop(EVLOOP_ONCE);
        }
        else if(global_delay)
            usleep(global_delay);
    }
    
    DEBUG("main loop finished");

    return 0;
}

