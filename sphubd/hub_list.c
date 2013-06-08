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

#include <stdlib.h>
#include <string.h>

#include "hub.h"
#include "encoding.h"
#include "log.h"
#include "xstr.h"
#include "notifications.h"
#include "extip.h"

static int myinfo_need_update = false;
static LIST_HEAD(, hub) hub_list_head;

static void hub_handle_external_ip_notification(nc_t *nc, const char *channel,
	nc_external_ip_detected_t *info,
	void *user_data)
{
	hub_t *hub;
	LIST_FOREACH(hub, &hub_list_head, next)
	{
		user_set_ip(hub->me, extip_get(hub->fd, hub->hubip));
		INFO("External IP for hub [%s] is %s", hub->address, hub->me->ip);
	}
}

void hub_list_init(void)
{
	LIST_INIT(&hub_list_head);

	nc_add_external_ip_detected_observer(nc_default(),
		hub_handle_external_ip_notification, NULL);
}

unsigned hub_user_hash(const char *nick)
{
    const char *p = nick;
    unsigned h = *p;

    if(h)
    {
        for(p += 1; *p; p++)
        {
            h = (h << 5) - h + *p;
        }
    }

    return h % HUB_USER_NHASH;
}

hub_t *hub_new(void)
{
    hub_t *hub = calloc(1, sizeof(hub_t));

    hub->hubname = strdup("unknown");
    TAILQ_INIT(&hub->messages_head);
    TAILQ_INIT(&hub->user_commands_head);
    hub->encoding = strdup("WINDOWS-1252");

    int i;
    for(i = 0; i < HUB_USER_NHASH; i++)
    {
        LIST_INIT(&hub->users[i]);
    }

    return hub;
}

void hub_free(hub_t *hub)
{
    DEBUG("hub_free(%p)", hub);
    if(hub)
    {
        /* free all users */
        int i;
        for(i = 0; i < HUB_USER_NHASH; i++)
        {
            user_t *user, *next;
            for(user = LIST_FIRST(&hub->users[i]); user; user = next)
            {
                next = LIST_NEXT(user, link);
                LIST_REMOVE(user, link);
                user_free(user);
            }
        }
        
        user_free(hub->me);
        free(hub->hubname);
        free(hub->hubip);
        free(hub->password);
        free(hub->address);
        free(hub->myinfo_string);
        hub_message_free_all(hub);
        hub_user_command_free_all(hub);
        free(hub->encoding);
        free(hub);
    }
}

void hub_list_add(hub_t *hub)
{
    LIST_INSERT_HEAD(&hub_list_head, hub, next);
    hub_update_slots();
}

void hub_list_remove(hub_t *hub)
{
    LIST_REMOVE(hub, next);
    hub_update_slots();
}

static hub_t *hub_find_by_host_port(const char *hostname, int check_port)
{
    return_val_if_fail(hostname, NULL);

    hub_t *hub;
    LIST_FOREACH(hub, &hub_list_head, next)
    {
        if(strcasecmp(hub->address, hostname) == 0)
        {
            return hub;
        }

        int n = strcspn(hostname, ":");
        if(hub->hubip && strncasecmp(hub->hubip, hostname, n) == 0 &&
                (check_port == 0 ||
                 (n >= strlen(hostname) /* no port */ ||
                  hub->port == strtoul(hostname + n + 1, NULL, 10))))
        {
            return hub;
        }
    }

    return NULL;
}

hub_t *hub_find_by_host(const char *hostname)
{
    return_val_if_fail(hostname, NULL);
    return hub_find_by_host_port(hostname, 0);
}

hub_t *hub_find_by_address(const char *hostname)
{
    return_val_if_fail(hostname, NULL);
    return hub_find_by_host_port(hostname, 1);
}

hub_t *hub_find_by_fd(int fd)
{
    hub_t *hub;
    LIST_FOREACH(hub, &hub_list_head, next)
    {
        if(hub->fd == fd)
        {
            return hub;
        }
    }

    return NULL;
}

user_t *hub_lookup_user(hub_t *hub, const char *nick)
{
    user_t *user;
    LIST_FOREACH(user, &hub->users[hub_user_hash(nick)], link)
    {
        if(strcmp(user->nick, nick) == 0)
        {
            return user;
        }
    }

    return NULL;
}

hub_t *hub_find_by_nick(const char *nick)
{
    hub_t *hub;
    LIST_FOREACH(hub, &hub_list_head, next)
    {
        if(hub_lookup_user(hub, nick) != NULL)
        {
            return hub;
        }
    }

    return NULL;
}

hub_t *hub_find_encoding_by_nick(const char *nick, char **nick_utf8_ptr)
{
    hub_t *hub;
    LIST_FOREACH(hub, &hub_list_head, next)
    {
        char *nick_utf8 = str_convert_to_unescaped_utf8(nick, hub->encoding);
        if(nick_utf8)
        {
            if(hub_lookup_user(hub, nick_utf8) != NULL)
            {
                *nick_utf8_ptr = nick_utf8;
                return hub;
            }
            free(nick_utf8);
        }
    }
    return NULL;
}

void hub_foreach(void (*func)(hub_t *hub, void *user_data), void *user_data)
{
    hub_t *hub, *next;
    for(hub = LIST_FIRST(&hub_list_head); hub; hub = next)
    {
        next = LIST_NEXT(hub, next);
        func(hub, user_data);
    }
}

void hub_set_need_myinfo_update(bool flag)
{
    myinfo_need_update = flag;
}

bool hub_need_myinfo_update(void)
{
    return myinfo_need_update;
}

static hub_message_t *hub_message_new(const char *nick, const char *message)
{
    hub_message_t *hubmsg = calloc(1, sizeof(hub_message_t));
    hubmsg->nick = xstrdup(nick);
    hubmsg->message = xstrdup(message);
    return hubmsg;
}

static void hub_message_free(hub_message_t *hubmsg)
{
    return_if_fail(hubmsg);
    free(hubmsg->nick);
    free(hubmsg->message);
    free(hubmsg);
}

void hub_message_free_all(hub_t *hub)
{
    return_if_fail(hub);
    while(hub->num_messages > 0)
    {
        hub_message_pop(hub);
    }
}

void hub_message_pop(hub_t *hub)
{
    hub_message_t *hubmsg = TAILQ_LAST(&hub->messages_head, hub_messages_list);
    if(hubmsg)
    {
        TAILQ_REMOVE(&hub->messages_head, hubmsg, msg_link);
        hub_message_free(hubmsg);
        --hub->num_messages;
    }
}

void hub_message_push(hub_t *hub, const char *nick, const char *message_escaped)
{
    return_if_fail(hub);
    return_if_fail(message_escaped);

    hub_message_t *hubmsg = hub_message_new(nick, message_escaped);
    TAILQ_INSERT_TAIL(&hub->messages_head, hubmsg, msg_link);
    if(++hub->num_messages > 100)
    {
        hub_message_pop(hub);
    }
}

static hub_user_command_t *hub_user_command_new(int type, int context,
        const char *description, const char *command)
{
    hub_user_command_t *hubuc = calloc(1, sizeof(hub_user_command_t));
    hubuc->type = type;
    hubuc->context = context;
    hubuc->description = xstrdup(description);
    hubuc->command = xstrdup(command);
    return hubuc;
}

static void hub_user_command_free(hub_user_command_t *hubuc)
{
    return_if_fail(hubuc);
    free(hubuc->description);
    free(hubuc->command);
    free(hubuc);
}

void hub_user_command_free_all(hub_t *hub)
{
    return_if_fail(hub);
    while(hub->num_user_commands > 0)
    {
        hub_user_command_pop(hub);
    }
}

void hub_user_command_pop(hub_t *hub)
{
    hub_user_command_t *uc = TAILQ_LAST(&hub->user_commands_head,
            hub_user_commands_list);
    if(uc)
    {
        TAILQ_REMOVE(&hub->user_commands_head, uc,  link);
        hub_user_command_free(uc);
        --hub->num_user_commands;
    }
}

void hub_user_command_push(hub_t *hub, int type, int context,
        const char *description, const char *command)
{
    return_if_fail(hub);

    hub_user_command_t *uc = hub_user_command_new(type, context,
            description, command);
    TAILQ_INSERT_TAIL(&hub->user_commands_head, uc,  link);
    ++hub->num_user_commands;
}

/* returns 1 if we're connected to any hub */
int hub_is_connected(void)
{
    return LIST_FIRST(&hub_list_head) != NULL;
}

typedef struct
{
    int normal;
    int registered;
    int operator;
} hub_count_t;

static void hub_count_GFunc(hub_t *hub, void *user_data)
{
    hub_count_t *c = user_data;

    if(hub->me && hub->me->is_operator)
        c->operator++;
    else if(hub->is_registered)
        c->registered++;
    else
        c->normal++;
}

int hub_count_normal(void)
{
    hub_count_t c = {0, 0, 0};
    hub_foreach(hub_count_GFunc, &c);
    return c.normal;
}

int hub_count_registered(void)
{
    hub_count_t c = {0, 0, 0};
    hub_foreach(hub_count_GFunc, &c);
    return c.registered;
}

int hub_count_operator(void)
{
    hub_count_t c = {0, 0, 0};
    hub_foreach(hub_count_GFunc, &c);
    return c.operator;
}

