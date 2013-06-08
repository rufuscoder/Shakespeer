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

#ifndef _hub_h_
#define _hub_h_

#include "sys_queue.h"
#include <sys/types.h>
#include <sys/time.h>
#include <event.h>
#include <stdbool.h>
#include <stdint.h>

#include "user.h"

#define HUB_USER_NHASH 509

/* user-command types */
#define UC_TYPE_SEPARATOR 0
#define UC_TYPE_RAW 1
#define UC_TYPE_RAW_NICK_LIMITED 2
#define UC_TYPE_CLEAR 255

/* user-command contexts */
#define UC_CONTEXT_HUB 1
#define UC_CONTEXT_USER 2
#define UC_CONTEXT_USER_FILE 4
#define UC_CONTEXT_FILELIST 8

typedef struct hub_connect_data hub_connect_data_t;
struct hub_connect_data
{
    char *address;
    char *nick;
    char *email;
    char *description;
    char *speed;
    char *resolved_ip;
    int port;
    bool passive;
    char *password;
    char *encoding;
};

typedef struct hub_message hub_message_t;
struct hub_message
{
    TAILQ_ENTRY(hub_message) msg_link;
    char *nick;
    char *message;
};

typedef struct hub_user_command hub_user_command_t;
struct hub_user_command
{
    TAILQ_ENTRY(hub_user_command) link;
    int type;
    int context;
    char *description;
    char *command;
};

typedef struct hub hub_t;
struct hub
{
    LIST_ENTRY(hub) next;
    
    int fd;
    struct event idle_timeout_event;
    struct bufferevent *bufev;

    bool expected_disconnect;
    struct event reconnect_event;
    int reconnect_attempt;

    /* We define a kick as a disconnect where a reconnect succeeds
     * on the first try. We give up reconnecting after 3 repeated
     * kicks within 1 minute (ie, stop hammering).
     */
    int kick_counter;
    time_t kick_time; /* time of first disconnect */

    user_t *me;
    char *password;
    char *hubname;
    char *hubip;
    int port; /* remote port of hub */
    char *address; /* original address passed from ui */
    bool has_userip;
    bool has_nogetinfo;

    bool extended_protocol;
    bool logged_in;
    bool is_registered;
    bool got_lock;

    LIST_HEAD(, user) users[HUB_USER_NHASH];

    char *myinfo_string;
    int sent_user_commands;
    int sent_default_user_commands;
    TAILQ_HEAD(hub_messages_list, hub_message) messages_head;
    TAILQ_HEAD(hub_user_commands_list, hub_user_command) user_commands_head;
    int num_messages;
    int num_user_commands;
    char *encoding;
};

typedef enum {SLOT_NONE, SLOT_FREE, SLOT_EXTRA, SLOT_NORMAL} slot_state_t;

#include "search_listener.h"

/* hub_cmd.c
 */
void hub_attach_io_channel(hub_t *hub, int fd);

/* hub_list.c
 */
void hub_list_init(void);
hub_t *hub_new(void);
unsigned hub_user_hash(const char *nick);
user_t *hub_lookup_user(hub_t *hub, const char *nick);
void hub_free(hub_t *hub);
void hub_list_add(hub_t *hub);
void hub_list_remove(hub_t *hub);
void hub_foreach(void (*func)(hub_t *hub, void *user_data), void *user_data);
hub_t *hub_find_by_host(const char *hostname);
hub_t *hub_find_by_address(const char *hostname);
hub_t *hub_find_by_fd(int fd);
hub_t *hub_find_by_nick(const char *nick);
hub_t *hub_find_encoding_by_nick(const char *nick, char **nick_utf8_ptr);
void hub_update_myinfo(void);
void hub_set_need_myinfo_update(bool flag);
bool hub_need_myinfo_update(void);
int hub_send_command(hub_t *hub, const char *fmt, ...);
int hub_is_connected(void);
void hub_close_all_connections(void);

void hub_message_free_all(hub_t *hub);
void hub_message_pop(hub_t *hub);
void hub_message_push(hub_t *hub, const char *nick, const char *message_escaped);

void hub_user_command_free_all(hub_t *hub);
void hub_user_command_pop(hub_t *hub);
void hub_user_command_push(hub_t *hub, int type, int context, const char *description, const char *command);

/* hub_slots
 */
void hub_free_upload_slot(hub_t *hub, const char *nick,
	slot_state_t slot_state);
slot_state_t hub_request_upload_slot(hub_t *hub, const char *nick,
	const char *filename, uint64_t size);
void hub_set_slots(int slots, bool per_hub);
void hub_update_slots(void);
int hub_slots_free(void);
int hub_slots_total(void);

/* hub.c
 */
int hub_send_string(hub_t *hub, const char *string);
int hub_send_message(hub_t *hub, const char *nick, const char *message);
void hub_search(hub_t *hub, search_request_t *request);
void hub_close_connection(hub_t *hub);
bool hub_user_is_passive(hub_t *hub, const char *nick);
int hub_connect(const char *hubname, const char *nick, const char *email,
        const char *description, const char *speed, bool passive, const char *password,
        const char *encoding);
int user_is_logged_in_on_hub(const char *nick, hub_t *hub);
void hub_send_password(hub_t *hub);
void hub_send_myinfo(hub_t *hub);
void hub_all_send_myinfo(void);
int hub_count_normal(void);
int hub_count_registered(void);
int hub_count_operator(void);
void hub_set_password(hub_t *hub, const char *password);
void hub_update_user_info(const char *speed, const char *description, const char *email);
void hub_start_myinfo_updater(void);
// void hub_all_set_ip_address(const char *ip_address);
void hub_set_passive(bool on);
void hub_set_idle_timeout(hub_t *hub);
void hub_send_nmdc_default_user_commands(hub_t *hub);
void hub_set_encoding(hub_t *hub, const char *encoding);

#endif

