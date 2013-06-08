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
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "nfkc.h"

#include "client.h"
#include "log.h"
#include "cmd_table.h"
#include "encoding.h"
#include "globals.h"
#include "hub.h"
#include "io.h"
#include "nmdc.h"
#include "rx.h"
#include "xstr.h"
#include "extip.h"

typedef struct hub_search_data hub_search_data_t;
struct hub_search_data
{
    hub_t *hub;
    bool passive;
    union
    {
        struct
        {
            int fd;
            struct sockaddr_in addr;
        } active;
        char *nick;
    } dest;
};

static int hub_get_nicklist(hub_t *hub)
{
    return hub_send_string(hub, "$GetNickList|");
}

/* send the matched filename to the peer (client/hub) */
static int hub_search_match_callback(const share_search_t *search,
        share_file_t *file, const char *tth, void *data)
{
    hub_search_data_t *hsd = data;
    hub_t *hub = hsd->hub;
    char *response = 0;
    int num_returned_bytes;

    char *virtual_path = share_local_to_virtual_path(global_share, file);

    DEBUG("sending SR for %s", virtual_path);

    if (file->type == SHARE_TYPE_DIRECTORY) {
        num_returned_bytes = asprintf(&response, "$SR %s %s %u/%u\x05%s%s (%s:%d)%s%s|",
                hub->me->nick,
                virtual_path,
		hub_slots_free(), hub_slots_total(),
                tth ? "TTH:" : "",
                tth ? tth : hub->hubname,
                hub->hubip,
                hub->port,
                hsd->passive ? "\x05" : "",
                hsd->passive ? hsd->dest.nick : "");
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
    }
    else {
        num_returned_bytes = asprintf(&response, "$SR %s %s\x05%"PRIu64" %u/%u\x05%s%s (%s:%d)%s%s|",
                hub->me->nick,
                virtual_path,
                file->size,
		hub_slots_free(), hub_slots_total(),
                tth ? "TTH:" : "",
                tth ? tth : hub->hubname,
                hub->hubip,
                hub->port,
                hsd->passive ? "\x05" : "",
                hsd->passive ? hsd->dest.nick : "");
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
    }

    free(virtual_path);

    if (hsd->passive) {
        /* searching nick is passive, send results to hub */
        if(hub_send_command(hub, "%s", response) == 0)
        {
            hub_set_idle_timeout(hub);
        }
    }
    else
    { /* searching nick is active, send results directly via UDP */
        if(hsd->dest.active.fd == -1)
        {
            hsd->dest.active.fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

            if(hsd->dest.active.fd == -1)
            {
                WARNING("socket(): %s", strerror(errno));
                free(response);
                return -1;
            }
        }

        print_command(response, "-> (UDP:%s:%d)",
                inet_ntoa(hsd->dest.active.addr.sin_addr),
                ntohs(hsd->dest.active.addr.sin_port));

        int rc = -1;
        char *response_encoded = str_utf8_to_escaped_legacy(response,
                hub->encoding);
        if(response_encoded)
        {
            size_t len = strlen(response_encoded);

            rc = sendto(hsd->dest.active.fd, response_encoded, len, 0,
                    (const struct sockaddr *)&hsd->dest.active.addr,
                    sizeof(struct sockaddr_in));
            free(response_encoded);
        }

        if(rc == -1)
        {
            WARNING("unable to send UDP response: %s", strerror(errno));
            free(response);
            return -1;
        }
    }

    free(response);
    return 0;
}

/* return 1 if HOST corresponds to self */
static int hub_search_from_self(hub_t *hub, share_search_t *s)
{
    int rc = 0;

    if(s->passive)
        rc = (g_utf8_collate(s->nick, hub->me->nick) == 0);
    else
        rc = (hub->me->ip != NULL && strcmp(s->host, hub->me->ip) == 0 && global_port == s->port);

    return rc;
}

/* $Quit <nick> */
static int hub_cmd_Quit(void *data, int argc, char **argv)
{
    hub_t *hub = data;
    user_t *user = hub_lookup_user(hub, argv[0]);
    if(user)
    {
        cc_t *cc = cc_find_by_nick(argv[0]);
        if(cc /*&& cc->direction == CC_DIR_UPLOAD*/)
        {
            /* abort current transfer with logged out nick */
            cc_close_connection(cc);
        }
        LIST_REMOVE(user, link);
        user_free(user);
        ui_send_user_logout(NULL, hub->address, argv[0]);
    }
    return 0;
}

/* $Hello <nick> */
static int hub_cmd_Hello(void *data, int argc, char **argv)
{
    hub_t *hub = (hub_t *)data;

    if(strcmp(argv[0], hub->me->nick) == 0)
    {
        if(!hub->logged_in)
        {
            hub_send_string(hub, "$Version 1,0091|");
            hub_get_nicklist(hub);
            hub->logged_in = true;
            hub_send_myinfo(hub);
        }
    }

    return 0;
}

/* $MyINFO $ALL <nick> <interests>$ $<speed>$<email>$<sharesize>$ */
static int hub_cmd_MyINFO(void *opaque_param, int argc, char **argv)
{
    hub_t *hub = (hub_t *)opaque_param;

    user_t *new_user = user_new_from_myinfo(argv[0], hub);

    if(new_user)
    {
        user_t *user = hub_lookup_user(hub, new_user->nick);
        if(user)
        {
            /* need to merge the operator status */
            new_user->is_operator = user->is_operator;

            ui_send_user_update(NULL,
                    hub->address, new_user->nick, new_user->description,
                    new_user->tag, new_user->speed, new_user->email,
                    new_user->shared_size, new_user->is_operator,
                    new_user->extra_slots);
            if(new_user->tag &&
               strstr(new_user->tag, ",M:A,") != 0 && user->passive)
            {
                /* reset passive flag */
                new_user->passive = false;
            }

            /* remove the old user */
            LIST_REMOVE(user, link);
            user_free(user);
        }
        else
        {
            ui_send_user_login(NULL,
                    hub->address, new_user->nick, new_user->description,
                    new_user->tag, new_user->speed, new_user->email,
                    new_user->shared_size, new_user->is_operator,
                    new_user->extra_slots);
        }

        /* insert the new user */
        LIST_INSERT_HEAD(&hub->users[hub_user_hash(new_user->nick)],
                new_user, link);
    }

    return 0;
}

/* $ForceMove <newIP> */
static int hub_cmd_ForceMove(void *opaque_param, int argc, char **argv)
{
    hub_t *hub = (hub_t *)opaque_param;

    if(global_follow_redirects)
    {
        ui_send_hub_redirect(NULL, hub->address, argv[0]);

        char *nick = xstrdup(hub->me->nick);
        char *email = xstrdup(hub->me->email);
        char *description = xstrdup(hub->me->description);
        char *speed = xstrdup(hub->me->speed);
        char *encoding = xstrdup(hub->encoding);
        bool passive = hub->me->passive;
        hub->expected_disconnect = true;
        hub_close_connection(hub);
        hub_connect(argv[0], nick, email, description, speed,
                passive, NULL, encoding);
        free(nick);
        free(email);
        free(description);
        free(speed);
        free(encoding);
    }
    else
    {
        ui_send_status_message(NULL, hub->address,
                "Redirected to hub %s", argv[0]);
        hub->expected_disconnect = true;
        hub_close_connection(hub);
    }

    return -1;
}

/* $ConnectToMe <remotenick> <sender_host:port> */
static int hub_cmd_ConnectToMe(void *opaque_param, int argc, char **argv)
{
    hub_t *hub = (hub_t *)opaque_param;

    cc_connect(argv[1], hub);

    return 0;
}

/* $RevConnectToMe <remotenick> <mynick> */
static int hub_cmd_RevConnectToMe(void *opaque_param, int argc, char **argv)
{
    hub_t *hub = (hub_t *)opaque_param;

    DEBUG("got reverse connection request from %s", argv[0]);

    return_val_if_fail(hub, -1);

    if(strcmp(argv[1], hub->me->nick) != 0)
    {
        WARNING("strange $RevConnectToMe request: I'm not '%s'", argv[1]);
        return 0;
    }

    user_t *user = hub_lookup_user(hub, argv[0]);
    if(user)
    {
        if(!hub->me->passive)
        {
            hub_send_command(hub, "$ConnectToMe %s %s:%u|",
                    argv[0], hub->me->ip, global_port);
        }
        else
        {
            /* Send back a RevConnectToMe to the requesting peer to notify him
             * that we're also passive. Only do this once.
             */
            if(!user->passive)
            {
                DEBUG("bouncing back a RevConnectToMe to requesting user [%s]",
                        argv[0]);
                hub_send_command(hub, "$RevConnectToMe %s %s|",
                        hub->me->nick, argv[0]);
            }
            INFO("ignoring $RevConnectToMe to user %s: we're also passive",
                    argv[0]);
        }

        user->passive = true;
        /* FIXME: update UI of the changed passive/active mode ? */
    }
    else
    {
        INFO("Got RevConnectToMe from non-logged-in user '%s' (ignored)",
                argv[0]);
    }

    return 0;
}

/* $To: <tonick> From: <fromnick> $<<nick>> <message> */
static int hub_cmd_To(void *opaque_param, int argc, char **argv)
{
    hub_t *hub = (hub_t *)opaque_param;

    static const void *rx = NULL;
    if(rx == NULL)
    {
        rx = rx_compile("[^ ]+ From: ([^ ]+) \\$(.*)|$");
        return_val_if_fail(rx, -1);
    }
    
    rx_subs_t *subs = rx_search_precompiled(argv[0], rx);

    if(subs && subs->nsubs == 2)
    {
        char *nick = subs->subs[0];
        char *display_nick = nick;
        char *msg = subs->subs[1];

        static const void *rx2 = NULL;
        if(rx2 == NULL)
        {
            rx2 = rx_compile("^<([^ ]+)> (.*)$");
            return_val_if_fail(rx2, -1);
        }

        rx_subs_t *msgsubs = rx_search_precompiled(subs->subs[1], rx2);
        if(msgsubs && msgsubs->nsubs == 2)
        {
            display_nick = msgsubs->subs[0];
            msg = msgsubs->subs[1];
        }

        char *msg_escaped = nmdc_escape(msg);
        ui_send_private_message(NULL, hub->address, hub->me->nick,
                nick, display_nick, msg_escaped);
        free(msg_escaped);

        rx_free_subs(msgsubs);
    }

    rx_free_subs(subs);

    return 0;
}

/* <<nick>> <message> */
static int hub_cmd_ToAll(void *data, char *args)
{
    hub_t *hub = data;
    char *e;

    e = args+1 + strcspn(args+1, ">");
    *e++ = 0;
    if(*e == ' ') e++;
    /* e += strspn(e, " "); */

#if 0
    char *f = strchr(e, '|');
    if(f)
        *f = 0;
#endif

    char *nick = args + 1;
    char *msg = e;
    if(nick && *nick && msg && *msg)
    {
        char *msg_escaped = nmdc_escape(msg);
        ui_send_public_message(NULL, hub->address, nick, msg_escaped);
        hub_message_push(hub, nick, msg_escaped);
        free(msg_escaped);
    }

    return 0;
}

/* $Search <client_host:port> <searchstr> */
/* searchstr: <sizerestricted>?<isminimumsize>?<size>?<datatype>?<searchpattern> */
static int hub_cmd_Search(void *data, int argc, char **argv)
{
    hub_t *hub = data;

    share_search_t *s = share_search_parse_nmdc(argv[0], hub->encoding);
    if(s == NULL)
    {
        return 0;
    }

    if(hub_search_from_self(hub, s))
    {
        INFO("Ignoring my own search request");
        share_search_free(s);

        return 0;
    }

    hub_search_data_t hsd;
    hsd.hub = hub;
    hsd.passive = s->passive;
    if(hsd.passive)
    {
        hsd.dest.nick = s->nick;
    }
    else
    {
        memset(&hsd.dest.active.addr, 0, sizeof(struct sockaddr_in));
        if(inet_aton(s->host, &hsd.dest.active.addr.sin_addr) == 0)
        {
            WARNING("invalid IPv4 address in search request: '%s'"
                    " (skipping search request)", s->host);
            share_search_free(s);

            return 0;
        }
        hsd.dest.active.addr.sin_port = htons(s->port);
        hsd.dest.active.addr.sin_family = AF_INET;
        hsd.dest.active.fd = -1; /* will be set on first found search result */
    }

    share_search(global_share, s, hub_search_match_callback, &hsd);

    if(!hsd.passive && hsd.dest.active.fd != -1)
    {
        close(hsd.dest.active.fd);
    }

    share_search_free(s);

    return 0;
}

/* $NickList <nick1>$$<nick2>$$...$<nickN>$$ */
static int hub_cmd_NickList(void *data, int argc, char **argv)
{
    hub_t *hub = data;

    if(argc == 0)
        return 0;

    arg_t *nicks = arg_create(argv[0], "$", 0);

    if(nicks && hub->has_nogetinfo == false)
    {
        int i;
        for(i = 0; i < nicks->argc; i++)
        {
            if(nicks->argv[i][0])
            {
                hub_send_command(hub, "$GetINFO %s %s|",
                        nicks->argv[i], hub->me->nick);
            }
        }
    }

    arg_free(nicks);

    return 0;   
}

/* $OpList <nick1>$$<nick2>$$...$<nickN>$$ */
static int hub_cmd_OpList(void *data, int argc, char **argv)
{
    hub_t *hub = data;

    if(argc == 0)
        return 0;
    
    arg_t *ops = arg_create(argv[0], "$$", 0);
    int i;

    for(i = 0; i < ops->argc; i++)
    {
        if(g_utf8_collate(ops->argv[i], hub->me->nick) == 0)
        {
            hub->me->is_operator = true;
            INFO("I am an operator");
            if(!hub->sent_user_commands && !hub->sent_default_user_commands)
                hub_send_nmdc_default_user_commands(hub);
        }
        else
            INFO("User '%s' is an operator", ops->argv[i]);

        user_t *user = hub_lookup_user(hub, ops->argv[i]);
        int update_op_status = 0;
        if(user)
        {
            user->is_operator = true;
            update_op_status = 1;
        }
        else
        {
            user = user_new(ops->argv[i], NULL, NULL, NULL, NULL, 0, hub);
            if(user)
            {
                user->is_operator = true;
                LIST_INSERT_HEAD(&hub->users[hub_user_hash(user->nick)],
                        user, link);
            }
        }

        if(update_op_status)
        {
            ui_send_user_update(NULL,
                    hub->address, user->nick, user->description, user->tag,
                    user->speed, user->email, user->shared_size,
                    user->is_operator, user->extra_slots);
        }
    }
    arg_free(ops);

    return 0;   
}

/* $HubName <hubname> */
static int hub_cmd_HubName(void *data, int argc, char **argv)
{
    hub_t *hub = data;

    return_val_if_fail(argv[0] && argv[0][0], 0);

    INFO("new hubname: %s", argv[0]);
    ui_send_hubname(NULL, hub->address, argv[0]);

    free(hub->hubname);
    hub->hubname = xstrdup(argv[0]);

    return 0;
}

/* $UserIP <nick> <ip> */
/* $UserIP2 <nick1> <ip1>$$<nick2> <ip2>$$...$$<nickN> <ipN> */
static int hub_cmd_UserIP(void *data, int argc, char **argv)
{
    hub_t *hub = data;
    int i;

    arg_t *a = arg_create(argv[0], "$$", 0);
    for(i = 0; i < a->argc; i++)
    {
        arg_t *b = arg_create(a->argv[i], " ", 0);

        if(b && b->argc == 2 && g_utf8_collate(b->argv[0], hub->me->nick) == 0)
        {
            struct in_addr tmp;
            if(inet_aton(b->argv[1], &tmp) == 0)
            {
                WARNING("Public IP address received from hub is invalid (%s),"
                        " ignored", b->argv[1]);
            }
            else
            {
                /* Use this IP address for this hub, overriding any manual
                 * setting or detected external IP. After all, the hub should
                 * know best.
                 */

                INFO("Hub reported my active IP as %s", b->argv[1]);
				if (extip_get_override()) {
					user_set_ip(hub->me, b->argv[1]);
				} else {
					INFO("Ignoring reported IP, using %s", hub->me->ip);
				}
            }
        }

        arg_free(b);
    }
    arg_free(a);

    return 0;
}

static int hub_cmd_SR(void *data, int argc, char **argv)
{
    search_listener_handle_response(global_search_listener, argv[0]);
    return 0;
}

static int hub_cmd_Supports(void *data, int argc, char **argv)
{
    hub_t *hub = data;
    int i;

    for(i = 0; i < argc; i++)
    {
        if(strcmp(argv[i], "NoGetINFO") == 0)
            hub->has_nogetinfo = true;
        else if(strcmp(argv[i], "NoHello") == 0)
            /* whatever */ ;
        else if(strcmp(argv[i], "UserIP") == 0)
            hub->has_userip = true;
        else if(strcmp(argv[i], "UserIP2") == 0)
            hub->has_userip = true;
        else
            INFO("Hub supports unknown feature %s", argv[i]);
    }

#if 0   /* FIXME: shouldn't we enable this? */
    if(!hub->me->passive && hub->has_userip)
    {
        hub_send_command(hub, "$UserIP %s|", hub->me->nick);
    }
#endif

    return 0;
}

static int
hub_cmd_GetPass(void *data, int argc, char **argv)
{
    hub_t *hub = data;

    hub->is_registered = true;

    if(hub->password == NULL)
    {
        /* FIXME: should probably only send this to the ui that initiated the
         * connection */
        ui_send_get_password(NULL, hub->address, hub->me->nick);
    }
    else
        hub_send_password(hub);

    return 0;
}

static int
hub_cmd_LogedIn(void *data, int argc, char **argv)
{
    hub_set_need_myinfo_update(true);
    DEBUG("Ok, seems we're logged in...");
    return 0;
}

static int
hub_cmd_BadPass(void *data, int argc, char **argv)
{
    hub_t *hub = data;

    INFO("Wrong password");
    hub->expected_disconnect = true;
    hub_close_connection(hub);
    return -1;
}

static int hub_cmd_ValidateDenide(void *data, int argc, char **argv)
{
    hub_t *hub = data;

    ui_send_status_message(NULL, hub ? hub->address : NULL,
            "The hub didn't accept the nickname");
    if(hub)
    {
        hub->expected_disconnect = true;
        hub_close_connection(hub);
    }
    return -1;
}

/* $Lock lock Pk=key */
static int hub_cmd_Lock(hub_t *hub, const char *args)
{
    if(hub->got_lock)
    {
        DEBUG("Already got a lock, ignoring");
        return 0;
    }

    if(strncmp(args, "EXTENDEDPROTOCOL", 16) == 0)
        hub->extended_protocol = true;

    if(hub->extended_protocol)
    {
        hub_send_string(hub,
                "$Supports UserCommand NoGetINFO NoHello UserIP2 TTHSearch |");
    }

    char *pk = strstr(args, " Pk=");
    if(pk)
        *pk = 0;

    char *key = nmdc_lock2key(args);
    char *cmd = 0;
    int num_returned_bytes = asprintf(&cmd, "$Key %s|", key);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
    hub_send_string(hub, cmd);
    free(cmd);
    free(key);

    hub->got_lock = true;
    return hub_send_command(hub, "$ValidateNick %s|", hub->me->nick);
}

static int hub_cmd_Usercommand(void *data, int argc, char **argv)
{
    hub_t *hub = data;

    hub->sent_user_commands = 1;

    arg_t *args = arg_create_max(argv[0], " ", 0, 3);
    return_val_if_fail(args && args->argc >= 2, 0);

    int type = strtoul(args->argv[0], 0, 10);
    int context = strtoul(args->argv[1], 0, 10);

    if((context & 0x0F) == 0)
    {
        INFO("unrecognized user-command context %i (ignored)", type);
    }
    else
    {
        if(type == UC_TYPE_SEPARATOR || type == UC_TYPE_CLEAR)
        {
            /* separator or clear sent commands */
            ui_send_user_command(NULL, hub->address, type, context, NULL, NULL);
            hub_user_command_push(hub, type, context, NULL, NULL);
            if(type == UC_TYPE_CLEAR)
            {
                hub_user_command_free_all(hub);
                hub_send_nmdc_default_user_commands(hub);
            }
        }
        else if(type == UC_TYPE_RAW || type == UC_TYPE_RAW_NICK_LIMITED)
        {
            /* raw or raw-nick-limited command */
            arg_t *details = NULL;
            if(args->argc > 2)
            {
                details = arg_create_max(args->argv[2], "$", 0, 2);
            }

            if(details && details->argc == 2)
            {
                char *title = details->argv[0];
                char *command = 0;

                /* add a pipe (encoded) to end of command if none found */
                if(str_has_suffix(details->argv[1], "&#124;"))
                    command = strdup(details->argv[1]);
                else {
                    int num_returned_bytes = asprintf(&command, "%s&#124;", details->argv[1]);
                    if (num_returned_bytes == -1)
                        DEBUG("asprintf did not return anything");
                }

                /* rest of command might need additional escaping */
                char *escaped = nmdc_escape(command);
                free(command);

                ui_send_user_command(NULL, hub->address, type, context,
                        title, escaped);
                hub_user_command_push(hub, type, context,
                        title, escaped);
                free(escaped);
            }
            else
            {
                INFO("malformed user-command [%s] (ignored), argc = %i",
                        argv[0], details ? details->argc : -1);
            }
            arg_free(details);
        }
        else
        {
            INFO("unrecognized user-command type %i (ignored)", type);
        }
    }

    arg_free(args);

    return 0;
}

static cmd_t hub_cmds[] = {
    {"$Hello", hub_cmd_Hello, 1},
    {"$Quit", hub_cmd_Quit, 1},
    {"$MyINFO", hub_cmd_MyINFO, -1},
    {"$HubName", hub_cmd_HubName, -1},
    {"$Supports", hub_cmd_Supports, 1},
    {"$Search", hub_cmd_Search, -1},
    {"$MultiSearch", hub_cmd_Search, -1},
    {"$SR", hub_cmd_SR, -1},
    {"$ConnectToMe", hub_cmd_ConnectToMe, 2},
    {"$RevConnectToMe", hub_cmd_RevConnectToMe, 2},
    {"$To:", hub_cmd_To, -1},
    {"$NickList", hub_cmd_NickList, -1},
    {"$OpList", hub_cmd_OpList, -1},
    {"$GetPass", hub_cmd_GetPass, 0},
    {"$ForceMove", hub_cmd_ForceMove, 1},
    {"$UserIP", hub_cmd_UserIP, -1},
    {"$UserIP2", hub_cmd_UserIP, -1},
    {"$LogedIn", hub_cmd_LogedIn, 0},
    {"$BadPass", hub_cmd_BadPass, 0},
    {"$ValidateDenide", hub_cmd_ValidateDenide, 0},
    {"$UserCommand", hub_cmd_Usercommand, -1},
    {0, 0, -1}
};

int hub_dispatch_command(hub_t *hub, char *cmdstr)
{
    int rc = -2;

    str_trim_end_inplace(cmdstr, NULL);

    if(cmdstr[0] == 0)
        /* ignore */ ;
    else if(strncmp(cmdstr, "$Lock ", 6) == 0)
    {
        rc = hub_cmd_Lock(hub, cmdstr + 6);
    }
    else
    {
        char *cmdstr_utf8 = str_legacy_to_utf8_lossy(cmdstr, hub->encoding);
        if(cmdstr_utf8 == NULL)
        {
            WARNING("command [%s] failed to convert to (lossy) UTF-8", cmdstr);
            return 0;
        }

        char *cmdstr_utf8_unescaped;
        if(str_need_unescape_unicode(cmdstr_utf8))
        {
            cmdstr_utf8_unescaped = str_unescape_unicode(cmdstr_utf8);
            free(cmdstr_utf8);
        }
        else
        {
            cmdstr_utf8_unescaped = cmdstr_utf8;
        }

        if(cmdstr_utf8_unescaped[0] == '<')
        {
            if(strcasestr(cmdstr_utf8_unescaped, "banned") != NULL)
            {
                hub->expected_disconnect = true;
            }
            rc = hub_cmd_ToAll(hub, cmdstr_utf8_unescaped);
        }
        else if(cmdstr_utf8_unescaped[0] != '$')
        {
            char *msg_escaped = nmdc_escape(cmdstr_utf8_unescaped);
            ui_send_public_message(NULL, hub->address, "nobody", msg_escaped);
            hub_message_push(hub, "nobody", msg_escaped);
            free(msg_escaped);
            rc = 0;
        }
        else
        {
            rc = cmd_dispatch(cmdstr_utf8_unescaped, " ", 0, hub_cmds, hub);
        }

        free(cmdstr_utf8_unescaped);
    }

    return rc;
}

static void hub_in_event(struct bufferevent *bufev, void *data)
{
    hub_t *hub = data;

    return_if_fail(hub);

    hub_set_idle_timeout(hub);

    while(1)
    {
        char *cmd = io_evbuffer_readline(EVBUFFER_INPUT(bufev));
        if(cmd == NULL)
        {
            break;
        }
        print_command(cmd, "<- (fd %d)", hub->fd);
        int rc = hub_dispatch_command(hub, cmd);
        free(cmd);
        if(rc != 0)
        {
            break;
        }
    }
}

static void hub_out_event(struct bufferevent *bufev, void *data)
{
}

static void hub_err_event(struct bufferevent *bufev, short why, void *data)
{
    hub_t *hub = data;

    WARNING("why = 0x%02X", why);
    hub_close_connection(hub);
}

void hub_attach_io_channel(hub_t *hub, int fd)
{
    io_set_blocking(fd, 0);

    hub->fd = fd;

    DEBUG("adding hub on fd %d to main loop", fd);
    hub->bufev = bufferevent_new(fd, hub_in_event, hub_out_event, hub_err_event, hub);
    bufferevent_enable(hub->bufev, EV_READ | EV_WRITE);
}

