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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include <evdns.h>

#include "client.h"
#include "log.h"
#include "encoding.h"
#include "globals.h"
#include "hub.h"
#include "nmdc.h"
#include "search_listener.h"
#include "ui.h"
#include "user.h"
#include "sphubd.h"
#include "xstr.h"
#include "extip.h"

static void hub_schedule_reconnect_event(hub_t *hub);

static void hub_send_keep_alive(int fd, short condition, void *data)
{
    hub_t *hub = data;
    return_if_fail(hub);

    DEBUG("sending keep-alive to hub '%s'", hub->address);
    if(hub_send_string(hub, "|") != 0)
    {
        hub_close_connection(hub);
    }

    /* Reschedule the event
     */
    hub_set_idle_timeout(hub);
}

void hub_set_idle_timeout(hub_t *hub)
{
    if(event_initialized(&hub->idle_timeout_event))
    {
        evtimer_del(&hub->idle_timeout_event);
    }
    else
    {
        evtimer_set(&hub->idle_timeout_event, hub_send_keep_alive, hub);
    }

    struct timeval tv = {.tv_sec = 300, .tv_usec = 0};
    evtimer_add(&hub->idle_timeout_event, &tv);
}

static char *hub_make_tag(hub_t *hub)
{
    char *tag = 0;
    int num_returned_bytes = asprintf(&tag, "<%s V:%s,M:%c,H:%d/%d/%d,S:%d>",
            global_id_tag,
            global_id_version,
            hub->me->passive ? 'P' : 'A',
            hub_count_normal(), hub_count_registered(), hub_count_operator(),
            hub_slots_total());
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");

    return tag;
}

int hub_send_string(hub_t *hub, const char *string)
{
    return_val_if_fail(hub, -1);
    return_val_if_fail(string, -1);
    return_val_if_fail(hub->bufev, -1);
    return_val_if_fail(hub->fd != -1, -1);

    print_command(string, "-> (fd %i)", hub->fd);
    return bufferevent_write(hub->bufev, (void *)string, strlen(string));
}

int hub_send_command(hub_t *hub, const char *fmt, ...)
{
    return_val_if_fail(hub, -1);
    return_val_if_fail(fmt, -1);

    va_list ap;
    va_start(ap, fmt);
    char *command = 0;
    int num_returned_bytes = vasprintf(&command, fmt, ap);
    if (num_returned_bytes == -1)
        DEBUG("vasprintf did not return anything");
    char *command_encoded = str_utf8_to_escaped_legacy(command, hub->encoding);
    int rc = hub_send_string(hub, command_encoded);
    free(command_encoded);
    free(command);
    va_end(ap);

    return rc;
}

void
hub_send_password(hub_t *hub)
{
    /* XXX: encoding?
     */
    if(hub_send_command(hub, "$MyPass %s|", hub->password) == 0)
        hub_set_idle_timeout(hub);
}

void hub_send_myinfo(hub_t *hub)
{
    return_if_fail(hub);

    if(!hub->logged_in)
    {
        INFO("ignored sending myinfo as we're not logged in yet");
        return;
    }

    share_stats_t stats;
    share_get_stats(global_share, &stats);

    char *tag = hub_make_tag(hub);
    char *myinfo_string = 0;
    int num_returned_bytes = asprintf(&myinfo_string, "$MyINFO $ALL %s %s%s$ $%s\x01$%s$%"PRIu64"$|",
            hub->me->nick,
            hub->me->description ? hub->me->description : "",
            tag,
            hub->me->speed,
            hub->me->email ? hub->me->email : "",
            stats.size);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
    free(tag);

    if(hub->myinfo_string == NULL ||
            strcmp(hub->myinfo_string, myinfo_string) != 0)
    {
        if(hub_send_command(hub, "%s", myinfo_string) == 0)
        {
            hub_set_idle_timeout(hub);
        }
        free(hub->myinfo_string);
        hub->myinfo_string = myinfo_string;
    }
    else
    {
        free(myinfo_string);
    }
}

static void hub_all_send_myinfo_GFunc(hub_t *hub, void *user_data)
{
    hub_send_myinfo(hub);
}

void hub_all_send_myinfo(void)
{
    hub_foreach(hub_all_send_myinfo_GFunc, NULL);
}

static hub_t *
hub_new_from_hcd(hub_connect_data_t *hcd)
{
    DEBUG("creating new hub with nick %s...", hcd->nick);
    hub_t *hub = hub_new();
    if(hub == NULL)
    {
        WARNING("failed to create hub...");
        ui_send_status_message(NULL, NULL, "failed to create hub...");
        return NULL;
    }
    hub->me = user_new(hcd->nick, NULL, hcd->speed, hcd->description,
	hcd->email, 0ULL, hub);
    hub->me->passive = hcd->passive;
    hub->password = xstrdup(hcd->password);
    hub->address = strdup(hcd->address);
    hub->port = hcd->port;
    free(hub->hubname);
    hub->hubname = strdup(hcd->address);
    hub->hubip = strdup(hcd->resolved_ip);
    if(hcd->encoding)
    {
        hub_set_encoding(hub, hcd->encoding);
    }

    hub_list_add(hub);
    hub_set_need_myinfo_update(true);

    ui_send_hub_add(NULL, hub->address, hub->hubname, hub->me->nick,
	hub->me->description, hub->encoding);

    hub_set_idle_timeout(hub);

    return hub;
}

static void
hcd_free(hub_connect_data_t *hcd)
{
    if(hcd)
    {
        free(hcd->nick);
        free(hcd->email);
        free(hcd->description);
        free(hcd->speed);
        free(hcd->address);
        free(hcd->resolved_ip);
        free(hcd->password);
        free(hcd->encoding);
        free(hcd);
    }
}

static void
hub_connect_event(int fd, int error, void *user_data)
{
    hub_connect_data_t *hcd = user_data;

    if(error == 0)
    {
        ui_send_status_message(NULL, hcd->address,
                "Connected to hub %s", hcd->address);

        hub_t *hub = hub_find_by_address(hcd->address);
        if(hub)
        {
	    /* found a hub with same address, check if it's a reconnection
	     * attempt */
            if(hub->reconnect_attempt > 0)
            {
                /* yes, remove the old one and create a new */

		/* Reset kick counter after 1 minute. */
		time_t now = time(0);
		if(hub->kick_counter && hub->kick_time + 60 < now)
		{
		    hub->kick_counter = 0;
		}

		int kick_counter = 0;
		if(hub->reconnect_attempt == 1)
		{
		    /* We got reconnected on the first attempt, this
		     * might actually be a kick. */
		    kick_counter = hub->kick_counter + 1;
		}

                DEBUG("replacing reconnection hub");
                hub_close_connection(hub);
                hub = hub_new_from_hcd(hcd);

		/* Restore the saved kick_counter. If we got
		 * disconnected because we we're being banned or kicked, we
		 * shouldn't keep hammering on the door. Also set the time
		 * of the first kick.
		 */
		hub->kick_counter = kick_counter;
		if(hub->kick_counter == 1)
		    hub->kick_time = time(0);
            }
            else
            {
                /* no, strange... discard the new hub connection */
                ui_send_status_message(NULL, hcd->address,
                        "Already connected to hub %s", hcd->address);
                close(fd);
                hub = NULL;
            }
        }
        else
        {
            hub = hub_new_from_hcd(hcd);
        }

        if(hub)
        {
            hub_attach_io_channel(hub, fd);
	    user_set_ip(hub->me, extip_get(hub->fd, hub->hubip));
        }
    }
    else
    {
        /* must be an error */
        WARNING("connection failed: %s", strerror(error));
        ui_send_status_message(NULL, hcd->address,
                "Failed to connect to hub %s: %s",
                hcd->address, strerror(error));
        ui_send_connect_failed(NULL, hcd->address);
        close(fd);

        /* look for a reconnecting hub */
        hub_t *hub = hub_find_by_address(hcd->address);
        if(hub && hub->reconnect_attempt > 0)
        {
            DEBUG("found reconnection hub with address [%s]", hub->address);
            hub_schedule_reconnect_event(hub);
        }
    }

    hcd_free(hcd);
}

static void hub_connect_async(hub_connect_data_t *hcd, struct in_addr *addr)
{
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(hcd->port);
    memcpy(&saddr.sin_addr, addr, sizeof(struct in_addr));

    hcd->resolved_ip = strdup( inet_ntoa(saddr.sin_addr) );

    xerr_t *err = 0;
    int rc = io_connect_async(&saddr, hub_connect_event, hcd, &err);
    if(rc != 0)
    {
        WARNING("Failed to connect to hub '%s': %s",
                hcd->address, xerr_msg(err));
        ui_send_status_message(NULL, hcd->address,
                "Failed to connect to hub '%s': %s",
                hcd->address, xerr_msg(err));
        xerr_free(err);
        hcd_free(hcd);
    }
}

static void hub_lookup_event(int result, char type, int count, int ttl,
    void *addresses, void *user_data)
{
    hub_connect_data_t *hcd = user_data;
    return_if_fail(hcd);

    if(result == DNS_ERR_NONE)
    {
	struct in_addr *addrs = addresses;
	return_if_fail(addrs);
	return_if_fail(count >= 1);
        hub_connect_async(hcd, &addrs[0]);
    }
    else
    {
        const char *errmsg = evdns_err_to_string(result);
        WARNING("Failed to lookup '%s': %s", hcd->address, errmsg);
        ui_send_status_message(NULL, hcd->address,
                "Failed to lookup '%s': %s", hcd->address, errmsg);
    }
}

int hub_connect(const char *hubname, const char *nick, const char *email,
        const char *description, const char *speed, bool passive,
        const char *password, const char *encoding)
{
    return_val_if_fail(nick, -1);
    return_val_if_fail(hubname, -1);

    char *host = 0;
    int port = 0;
    if(split_host_port(hubname, &host, &port) != 0)
    {
        return -1;
    }
    else if(port < 0)
    {
        ui_send_status_message(NULL, hubname,
                "Invalid port in hub address: %s", hubname);
        free(host);
        return -1;
    }
    else if(port == 0)
    {
        port = 411; /* default port */
    }

    ui_send_status_message(NULL, hubname, "Connecting to %s...", hubname);

    hub_connect_data_t *hcd = calloc(1, sizeof(hub_connect_data_t));
    hcd->nick = strdup(nick);
    hcd->email = xstrdup(email);
    hcd->description = xstrdup(description);
    hcd->speed = xstrdup(speed);
    hcd->address = strdup(hubname);
    hcd->passive = passive;
    hcd->password = xstrdup(password);
    hcd->encoding = xstrdup(encoding);
    hcd->port = port;

    struct in_addr xaddr;
    if(inet_aton(host, &xaddr))
    {
        /* host already given as an IP address */
        hub_connect_async(hcd, &xaddr);
        free(host);
    }
    else
    {
        int rc = evdns_resolve_ipv4(host, 0, hub_lookup_event, hcd);
        free(host);
        if(rc != DNS_ERR_NONE)
        {
            WARNING("Failed to lookup '%s': %s",
		hubname, evdns_err_to_string(rc));
            ui_send_status_message(NULL, hubname, "Failed to lookup '%s': %s",
                    hubname, evdns_err_to_string(rc));
            hcd_free(hcd);
            return -1;
        }
    }

    return 0;
}

/* Specify nick = NULL to send a public message.
 */
int hub_send_message(hub_t *hub, const char *nick, const char *message)
{
    return_val_if_fail(hub && message, 0);

    int rc = -1;
    if(nick)
    {
        rc = hub_send_command(hub, "$To: %s From: %s $<%s> %s|",
                nick, hub->me->nick, hub->me->nick, message);
    }
    else
    {
        rc = hub_send_command(hub, "<%s> %s|", hub->me->nick, message);
    }

    if(rc == 0)
        hub_set_idle_timeout(hub);

    return 0;
}

void hub_search(hub_t *hub, search_request_t *request)
{
    return_if_fail(hub);
    return_if_fail(request);
    return_if_fail(hub->logged_in);

    char *words_joined = 0;
    int num_returned_bytes;
    if (request->tth) {
        num_returned_bytes = asprintf(&words_joined, "TTH:%s", request->tth);
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
    }
    else {
        if (request->words == NULL) {
            WARNING("no search words?");
            return;
        }
        words_joined = arg_join(request->words, 0, -1, "$");
    }

    char *search_string = 0;
    num_returned_bytes = asprintf(&search_string, "%c?%c?%"PRIu64"?%u?%s",
            (request->size_restriction != SHARE_SIZE_NONE && request->type != SHARE_TYPE_TTH) ? 'T' : 'F',
            ((request->size_restriction == SHARE_SIZE_MIN ||
              (request->size_restriction == SHARE_SIZE_EQUAL && request->minsize)) &&
             request->type != SHARE_TYPE_TTH) ? 'F' : 'T',
            request->search_size, request->type, words_joined);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");

    int rc = -1;
    if (hub->me->passive)
        rc = hub_send_command(hub, "$Search Hub:%s %s|", hub->me->nick, search_string);
    else
        rc = hub_send_command(hub, "$Search %s:%u %s|", hub->me->ip, global_port, search_string);

    if (rc == 0)
        hub_set_idle_timeout(hub);

    free(search_string);
    free(words_joined);
}

void
hub_reconnect_event_func(int fd, short why, void *data)
{
    hub_t *hub = data;

    return_if_fail(hub);

    hub_connect(hub->address, hub->me->nick, hub->me->email,
	hub->me->description, hub->me->speed, hub->me->passive,
	hub->password, hub->encoding);
}

static void
hub_schedule_reconnect_event(hub_t *hub)
{
    return_if_fail(hub);

    if(event_pending(&hub->reconnect_event, EV_TIMEOUT, NULL))
    {
        DEBUG("ignoring manual reconnection");
        return;
    }

    if(hub->reconnect_attempt == 10)
    {
        ui_send_status_message(NULL, hub->address,
                "Giving up on hub %s after 10 reconnect attempts",
                hub->address);
        hub_close_connection(hub);
        return;
    }

    ++hub->reconnect_attempt;
    /* increase the reconnection interval quadratically */
    int interval = hub->reconnect_attempt * hub->reconnect_attempt * 5;

    ui_send_status_message(NULL, hub->address,
            "Reconnecting to %s in %i seconds (attempt %i)",
            hub->address, interval, hub->reconnect_attempt);

    if(event_initialized(&hub->reconnect_event))
    {
        evtimer_del(&hub->reconnect_event);
    }
    else
    {
        evtimer_set(&hub->reconnect_event, hub_reconnect_event_func, hub);
    }

    struct timeval tv = {.tv_sec = interval, .tv_usec = 0};
    evtimer_add(&hub->reconnect_event, &tv);
}

void
hub_close_connection(hub_t *hub)
{
    return_if_fail(hub);

    /* are we currently trying to reconnect to this hub? */
    if(hub->reconnect_attempt == 0)
    {
        /* no */

        DEBUG("closing down hub on fd %d (address %s)", hub->fd, hub->address);
        if(hub->bufev)
        {
            bufferevent_free(hub->bufev);
            hub->bufev = NULL;
        }
        if(hub->fd != -1)
        {
            close(hub->fd);
            hub->fd = -1;
        }
        evtimer_del(&hub->idle_timeout_event);

        if(hub->address)
        {
            ui_send_hub_disconnected(NULL, hub->address);
        }
        ui_send_status_message(NULL, hub->address,
                "Connection closed with hub %s", hub->address);

        cc_close_all_on_hub(hub);

        hub_set_need_myinfo_update(true);

	/* should we attempt to reconnect to the hub? */
        if(hub->expected_disconnect == false && hub->kick_counter < 3)
        {
	    hub_schedule_reconnect_event(hub);
            return;
        }
    }
    else
    {
        evtimer_del(&hub->reconnect_event);
    }

    hub_list_remove(hub);
    hub_free(hub);
}

static void hub_close_connection_foreach_func(hub_t *hub, void *user_data)
{
    hub_close_connection(hub);
}

void hub_close_all_connections(void)
{
    hub_foreach(hub_close_connection_foreach_func, NULL);
}

bool hub_user_is_passive(hub_t *hub, const char *nick)
{
    return_val_if_fail(hub, false);
    return_val_if_fail(nick, false);

    user_t *user = hub_lookup_user(hub, nick);
    if(user)
        return user->passive;
    return false;
}

int user_is_logged_in(const char *nick)
{
    return hub_find_by_nick(nick) == NULL ? 0 : 1;
}

int user_is_logged_in_on_hub(const char *nick, hub_t *hub)
{
    return hub_lookup_user(hub, nick) ? 1 : 0;
}

void
hub_set_password(hub_t *hub, const char *password)
{
    return_if_fail(hub);
    free(hub->password);
    hub->password = NULL;
    if(password)
    {
        hub->password = strdup(password);
        hub_send_password(hub);
    }
}

struct hub_update_user_info_data
{
    const char *speed;
    const char *description;
    const char *email;
};

static void hub_update_user_info_GFunc(hub_t *hub, void *user_data)
{
    return_if_fail(hub);
    return_if_fail(hub->me);

    struct hub_update_user_info_data *udata = user_data;
    return_if_fail(udata);

    user_set_speed(hub->me, udata->speed);
    user_set_description(hub->me, udata->description);
    user_set_email(hub->me, udata->email);
}

void hub_update_user_info(const char *speed, const char *description,
        const char *email)
{
    struct hub_update_user_info_data udata = {
        .speed = speed, .description = description, .email = email};
    hub_foreach(hub_update_user_info_GFunc, &udata);
}

static void myinfo_updater(int fd, short condition, void *data)
{
    static time_t last_myinfo_update = 0;
    time_t now = time(0);

    if(hub_need_myinfo_update() && now - last_myinfo_update > 30)
    {
	if(((share_t *)global_share)->scanning == 0)
	{
	    hub_set_need_myinfo_update(false);
	    hub_all_send_myinfo();
	}
	else
	    WARNING("skipping update, %i scans in progress", ((share_t *)global_share)->scanning);
        last_myinfo_update = time(0);
    }

    /* re-schedule the event */
    hub_start_myinfo_updater();
}

void hub_start_myinfo_updater(void)
{
    static struct event ev;
    evtimer_set(&ev, myinfo_updater, &ev);
    struct timeval tv = {.tv_sec = 4, .tv_usec = 0};
    evtimer_add(&ev, &tv);
}

static void hub_set_passive_GFunc(hub_t *hub, void *user_data)
{
    return_if_fail(hub);
    return_if_fail(hub->me);

    bool *on = user_data;
    return_if_fail(on);

    hub->me->passive = *on;
}

void hub_set_passive(bool on)
{
    hub_foreach(hub_set_passive_GFunc, &on);
    hub_set_need_myinfo_update(true);
}

void hub_send_nmdc_default_user_commands(hub_t *hub)
{
    return_if_fail(hub);
    if(hub->me->is_operator)
    {
        int context = (UC_CONTEXT_USER | UC_CONTEXT_USER_FILE);

        ui_send_user_command(NULL, hub->address, UC_TYPE_SEPARATOR, context, NULL, NULL);
        hub_user_command_push(hub, UC_TYPE_SEPARATOR, context, NULL, NULL);

        char *kick_cmd = nmdc_escape("$To: %[nick] From: %[mynick] $<%[mynick]>"
                " You are being kicked because: %[line:Reason]|"
                "<%[mynick]> %[mynick] is kicking %[nick] because: %[line:Reason]|"
                "$Kick %[nick]|");
        ui_send_user_command(NULL, hub->address, UC_TYPE_RAW_NICK_LIMITED, context, "Kick user", kick_cmd);
        hub_user_command_push(hub, UC_TYPE_RAW_NICK_LIMITED, context, "Kick user", kick_cmd);
        free(kick_cmd);

        char *redirect_cmd = nmdc_escape("$OpForceMove $Who:%[nick]$Where:%[line:Target Server]$Msg:%[line:Message]|");
        ui_send_user_command(NULL, hub->address, UC_TYPE_RAW_NICK_LIMITED, context, "Redirect user", redirect_cmd);
        hub_user_command_push(hub, UC_TYPE_RAW_NICK_LIMITED, context, "Redirect user", redirect_cmd);
        free(redirect_cmd);

        hub->sent_default_user_commands = 1;
    }
}

void hub_set_encoding(hub_t *hub, const char *encoding)
{
    return_if_fail(hub);
    return_if_fail(encoding);

    DEBUG("setting encoding [%s] for hub [%s]", encoding, hub->address);

    free(hub->encoding);
    hub->encoding = strdup(encoding);
}

