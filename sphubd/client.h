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

#ifndef _client_h_
#define _client_h_

#include <sys/types.h>
#include <netinet/in.h>
#include <stdbool.h>

#include "sys_queue.h"
#include "hub.h"
#include "queue.h"
#include "io.h"
#include "ui.h"
#include "xerr.h"

/* idle timeout in seconds before a transfer is aborted due to inactivity */
#define CC_IDLE_TIMEOUT 5*60

enum cc_direction {
    CC_DIR_UNKNOWN,
    CC_DIR_DOWNLOAD = 1,
    CC_DIR_UPLOAD
};
typedef enum cc_direction cc_direction_t;

/* note: order is important, check client.c for integer comparisons of state */
enum cc_state
{
    CC_STATE_MYNICK,
    CC_STATE_LOCK,
    CC_STATE_DIRECTION,
    CC_STATE_KEY,
    CC_STATE_READY,
    CC_STATE_REQUEST,
    CC_STATE_BUSY
};
typedef enum cc_state cc_state_t;

typedef struct cc cc_t;
struct cc
{
    LIST_ENTRY(cc) next;
    
    int fd;
    struct bufferevent *bufev;
    struct sockaddr_in addr;

    /* close connections if handshake takes too long */
    struct event handshake_timer_event;

    cc_state_t state;
    hub_t *hub;
    char *nick;
    cc_direction_t direction;
    unsigned short challenge;
    bool incoming_connection;
    slot_state_t slot_state;

    bool extended_protocol;
    bool has_xmlbzlist;
    bool has_adcget;
    bool has_tthl;
    bool has_tthf;

    char upload_buf[4096];
    int upload_buf_offset;
    int upload_buf_size;
    int local_fd;
    uint64_t filesize; /* total file size */ /* FIXME: also in current_queue->size */
    uint64_t offset; /* FIXME: also in current_queue->offset */
    uint64_t bytes_to_transfer;
    uint64_t bytes_done;
    time_t last_activity;
    time_t transfer_start_time;
    time_t last_transfer_activity;
    queue_t *current_queue;
    char *local_filename;
    int fetch_leaves;

    void *leafdata;
    unsigned leafdata_len;
    unsigned leafdata_index;
};

cc_t *cc_new(int fd, hub_t *hub);
void cc_free(cc_t *cc);

int cc_request_download(cc_t *cc);
void cc_cancel_transfer(const char *local_filename);
void cc_cancel_directory_transfers(const char *target_directory);
cc_t *cc_find_by_fd(int fd);
cc_t *cc_find_by_nick_and_direction(const char *nick, cc_direction_t direction);
cc_t *cc_find_by_nick(const char *nick);
cc_t *cc_find_by_local_filename(const char *local_filename);
cc_t *cc_find_by_target_directory(const char *target_directory);
void cc_trigger_download(void);
void cc_set_transfer_stats_interval(int interval);
void cc_close_connection(cc_t *cc);
void cc_close_all_connections(void);
void cc_close_all_on_hub(hub_t *hub);
void cc_send_ongoing_transfers(ui_t *ui);
void cc_accept_connection(int fd, short condition, void *data);
int cc_connect(const char *address, hub_t *hub);

int cc_send_string(cc_t *cc, const char *string);
int cc_send_command(cc_t *cc, const char *fmt, ...);
int cc_send_command_as_is(cc_t *cc, const char *fmt, ...);
void cc_in_event(struct bufferevent *bufev, void *data);
void cc_out_event(struct bufferevent *bufev, void *data);

void cc_list_init(void);

/* client_cmd.c
 */
int client_execute_command(int fd, void *data, char *cmdstr);

/* client_download.c
 */
void cc_download_read(cc_t *cc);
int cc_start_download(cc_t *cc);
void cc_fl_match_queue(const char *filelist_path, const char *nick);

/* client_upload.c
 */
int cc_upload_prepare(cc_t *cc, const char *filename,
        uint64_t offset, uint64_t bytes_to_transfer, xerr_t **err);
int cc_start_upload(cc_t *cc);
void cc_finish_upload(cc_t *cc);
ssize_t cc_upload_read(cc_t *cc, void *buf, size_t nbytes);

#endif

