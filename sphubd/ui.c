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
#include <sys/stat.h>
#include <dirent.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "ui.h"
#include "io.h"
#include "log.h"
#include "sphubd.h"
#include "client.h"
#include "queue.h"
#include "queue_match.h"
#include "hub.h"
#include "globals.h"
#include "sphashd_client.h"
#include "extra_slots.h"
#include "rx.h"
#include "nmdc.h"
#include "xstr.h"
#include "dstring.h"
#include "extip.h"

static void ui_send_hub_state(hub_t *hub, void *user_data)
{
    ui_t *ui = user_data;

    ui_send_hub_add(ui, hub->address, hub->hubname, hub->me->nick,
            hub->me->description, hub->encoding);

    DEBUG("sending nick list on hub '%s' to file descriptor %d",
            hub->address, hub->fd);
    int i;
    for(i = 0; i < HUB_USER_NHASH; i++)
    {
        user_t *user;
        LIST_FOREACH(user, &hub->users[i], link)
        {
            ui_send_user_login(ui, hub->address, user->nick,
                    user->description, user->tag, user->speed, user->email,
                    user->shared_size, user->is_operator, user->extra_slots);
        }
    }

    DEBUG("sending user-commands");
    hub_user_command_t *uc;
    TAILQ_FOREACH(uc, &hub->user_commands_head, link)
    {
        ui_send_user_command(ui, hub->address,
                uc->type, uc->context, uc->description, uc->command);
    }

    DEBUG("sending hub messages");
    hub_message_t *hubmsg;
    TAILQ_FOREACH(hubmsg, &hub->messages_head, msg_link)
    {
        ui_send_public_message(ui, hub->address, hubmsg->nick, hubmsg->message);
    }

    cc_send_ongoing_transfers(ui);
}

static void ui_send_stored_filelists_state(ui_t *ui)
{
    DIR *fsdir;
    struct dirent *dp;
    time_t now = time(0);
    int limit = 100;

    fsdir = opendir(global_working_directory);
    if (fsdir == 0) {
        WARNING("%s: %s", global_working_directory, strerror(errno));
        return;
    }

    dstring_t *nicks = dstring_new(NULL);
    while((dp = readdir(fsdir)) != NULL)
    {
        const char *filename = dp->d_name;
        int type = is_filelist(filename);

        if(type == FILELIST_NONE)
		continue;

	static const void *rx_xml = NULL;
	static const void *rx_dclst = NULL;
	const void *rx = NULL;

	/* While we're at it, delete old filelists.
	 */
	struct stat stbuf;
	char *path;
	int num_returned_bytes = asprintf(&path, "%s/%s", global_working_directory, filename);
	if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
	if(stat(path, &stbuf) == 0)
	{
	    /* expire filelist after 24 hours */
	    if(stbuf.st_mtime + 24*3600 < now)
	    {
		DEBUG("removing expired filelist [%]", path);
		if(unlink(path) != 0)
		    WARNING(" %s: %s", path, strerror(errno));
		free(path);
		continue;
	    }
	}
	else
	    WARNING("%s: %s", path, strerror(errno));

	/* there's no use keeping uncompressed filelists around */
	if(!str_has_suffix(filename, ".bz2") && !str_has_suffix(filename, ".DcLst"))
	{
	    DEBUG("removing uncompressed filelist [%s]", path);
	    if(unlink(path) != 0)
		WARNING(" %s: %s", path, strerror(errno));
	    free(path);
	    continue;
	}
	free(path);

	/* limit number of stored filelist sent to ui */
	if(--limit < 0)
	    continue;

	if(type == FILELIST_XML && str_has_suffix(filename, ".bz2"))
	{
	    if(rx_xml == NULL)
	    {
		rx_xml = rx_compile("files\\.xml\\.(.+)\\.bz2");
		assert(rx_xml);
	    }
	    rx = rx_xml;
	}
	else if(type == FILELIST_DCLST && str_has_suffix(filename, ".DcLst"))
	{
	    if(rx_dclst == NULL)
	    {
		rx_dclst = rx_compile("MyList\\.(.+)\\.DcLst");
		assert(rx_dclst);
	    }
	    rx = rx_dclst;
	}

	if(rx)
	{
	    rx_subs_t *subs = rx_search_precompiled(filename, rx);
	    if(subs && subs->nsubs == 1)
	    {
		if(nicks->length)
		{
		    dstring_append(nicks, " ");
		}
		dstring_append(nicks, subs->subs[0]);
	    }

	    rx_free_subs(subs);
	}
    }

    closedir(fsdir);

    if(nicks->length)
    {
        ui_send_stored_filelists(ui, nicks->string);
    }

    dstring_free(nicks, 1);
}

static void ui_send_state(ui_t *ui)
{
    /* Send initial state (queue, hubs and users) to newly connected ui.
     */
    queue_send_to_ui();

    DEBUG("sending hub list to file descriptor %d", ui->fd);
    hub_foreach(ui_send_hub_state, ui);

    ui_send_share_stats_for_root(ui, NULL);
    ui_send_stored_filelists_state(ui);
}

static void ui_in_event(struct bufferevent *bufev, void *data)
{
    ui_t *ui = data;

    while(1)
    {
        char *cmd = io_evbuffer_readline(EVBUFFER_INPUT(bufev));
        if(cmd == NULL)
        {
            break;
        }
        print_command(cmd, "<- (fd %d)", ui->fd);
        ui_dispatch_command(cmd, "$", 1, ui);
        free(cmd);
    }
}

static void ui_out_event(struct bufferevent *bufev, void *data)
{
}

static void ui_err_event(struct bufferevent *bufev, short why, void *data)
{
    ui_t *ui = data;

    WARNING("why = 0x%02X", why);
    ui_close_connection(ui);
}

void ui_close_connection(ui_t *ui)
{
    DEBUG("closing down ui %p", ui);

    if(ui->bufev)
    {
        bufferevent_free(ui->bufev);
    }
    if(ui->fd != -1)
    {
        close(ui->fd);
    }

    ui_remove(ui);
    ui_free(ui);
}

static void ui_close_connection_GFunc(ui_t *ui, void *user_data)
{
    ui_close_connection(ui);
}

void ui_close_all_connections(void)
{
    ui_foreach(ui_close_connection_GFunc, NULL);
}

void ui_send_share_stats_for_root(ui_t *ui, const char *local_root)
{
    share_mountpoint_t *mp;
    LIST_FOREACH(mp, &((share_t *)global_share)->mountpoints, link)
    {
        if(local_root == NULL || strcmp(local_root, mp->local_root) == 0)
        {
            ui_send_share_stats(ui, mp->local_root,
                    mp->stats.size, mp->stats.totsize, mp->stats.dupsize,
                    mp->stats.nfiles, mp->stats.ntotfiles,
                    mp->stats.nduplicates);
        }
    }

    /* send total statistics */
    share_stats_t total_stats;
    share_get_stats(global_share, &total_stats);
    ui_send_share_stats(ui, NULL,
            total_stats.size, total_stats.totsize, total_stats.dupsize,
            total_stats.nfiles, total_stats.ntotfiles, total_stats.nduplicates);
}

static int ui_cb_search_common(ui_t *ui, const char *hub_address,
        const char *search_string,
        uint64_t size, share_size_restriction_t size_restriction,
        share_type_t file_type, int id)
{
    search_request_t *sreq = search_listener_create_search_request(
            search_string, size, size_restriction, file_type, id);
    if(sreq == NULL)
    {
        WARNING("failed to create search request");
        ui_send_status_message(NULL, NULL, "Unable to search for '%s'", search_string);
        return 0;
    }

    DEBUG("executing search command: searching for %s", search_string);

    if(hub_address == NULL || *hub_address == 0)
    {
        DEBUG("adding search id %i", sreq->id);
        search_listener_add_request(global_search_listener, sreq);
        hub_foreach((void (*)(hub_t *, void *))hub_search, (void *)sreq);
    }
    else
    {
        hub_t *hub = hub_find_by_address(hub_address);
        if(hub)
        {
            DEBUG("adding search id %i", sreq->id);
            search_listener_add_request(global_search_listener, sreq);

            hub_search(hub, sreq);
        }
        else
            WARNING("unable to execute search: hub not found");
    }

    return 0;
}

static int ui_cb_search_all(ui_t *ui, const char *search_string, uint64_t size,
        int size_restriction, int file_type, int id)
{
    return ui_cb_search_common(ui, NULL, search_string, size, size_restriction,
            file_type, id);
}

static int ui_cb_search(ui_t *ui, const char *hub_address, const char *search_string,
        uint64_t size, int size_restriction,
        int file_type, int id)
{
    return ui_cb_search_common(ui, hub_address, search_string, size, size_restriction,
            file_type, id);
}

static int ui_cb_connect(ui_t *ui, const char *hub_address, const char *nick,
        const char *email, const char *description, const char *speed,
	int passive, const char *password, const char *encoding)
{
    assert(ui);
    assert(hub_address);
    assert(nick);

    if(global_port == -1 && passive == 0)
    {
        WARNING("refused to connect to hub, set-port not yet issued");
        ui_send_status_message(ui, NULL, "No port set");
        return 0;
    }

    if(global_init_completion < 100)
    {
        WARNING("refused connect at init completion level %i", global_init_completion);
        ui_send_status_message(ui, NULL, "Can't connect yet, backend is initializing...");
        return 0;
    }

    /* The commented out code below allows one to connect to the same hub with
     * different nicknames. However, all commands identify hubs only by the
     * address. So this feature can only be enabled once hubs are identified by
     * both address and nick, or something else unique.
     */

    hub_t *hub = hub_find_by_address(hub_address);
    if(hub && hub->reconnect_attempt == 0 /*&& strcmp(hub->me->nick, nick) == 0*/)
    {
        /* ui_send_status_message(ui, NULL, "Already connected to %s as %s", hub_address, nick); */
        ui_send_status_message(ui, NULL, "Already connected to %s", hub_address);
    }
    else
    {
        hub_connect(hub_address, nick, email, description, speed, passive,
	    password, encoding);
    }

    return 0;
}

static int ui_cb_disconnect(ui_t *ui, const char *hub_address)
{
    INFO("disconnecting from hub %s", hub_address);

    hub_t *hub = hub_find_by_address(hub_address);
    if(hub)
    {
        DEBUG("found hub with name '%s'", hub->hubname);
        hub->expected_disconnect = true;
        hub_close_connection(hub);
    }
    return 0;
}

static int ui_cb_public_message(ui_t *ui, const char *hub_address, const char *message)
{
    if(message && *message)
    {
        hub_t *hub = hub_find_by_address(hub_address);
        if(hub)
        {
            hub_send_message(hub, NULL, message);
        }
    }

    return 0;
}

static int ui_cb_private_message(ui_t *ui, const char *hub_address,
        const char *nick, const char *message)
{
    if(message && *message)
    {
        hub_t *hub = hub_find_by_address(hub_address);
        if(hub)
        {
            hub_send_message(hub, nick, message);
        }
    }

    return 0;
}

static int ui_cb_download_file(ui_t *ui, const char *hub_address,
        const char *nick, const char *source_filename, uint64_t size,
        const char *target_filename, const char *tth)
{
    hub_t *hub = hub_find_by_address(hub_address);
    if(hub == 0)
    {
        WARNING("failed to find hub by address '%s'", hub_address);
        hub = hub_find_by_nick(nick);
    }

    if(hub && strcmp(hub->me->nick, nick) == 0)
    {
        ui_send_status_message(NULL, hub ? hub->address : NULL, "Ignored attempt do download from self");
    }
    else
    {
        queue_add(nick, source_filename, size, target_filename, tth);
    }

    return 0;
}

static int ui_cb_download_directory(ui_t *ui, const char *hub_address,
        const char *nick,
        const char *source_directory, const char *target_directory)
{
    hub_t *hub = hub_find_by_address(hub_address);
    if(hub == 0)
    {
        WARNING("failed to find hub by address '%s'", hub_address);
        hub = hub_find_by_nick(nick);
    }

    if(hub && strcmp(hub->me->nick, nick) == 0)
    {
        ui_send_status_message(NULL, hub ? hub->address : NULL,
                "Ignored attempt do download from self");
    }
    else
    {
        queue_add_directory(nick, source_directory, target_directory);
    }

    return 0;
}

static int ui_cb_download_filelist(ui_t *ui, const char *hub_address,
        const char *nick,
        int force_update, int auto_match)
{
    return_val_if_fail(nick, -1);

    hub_t *hub = hub_find_by_address(hub_address);

    if(hub == 0)
    {
        WARNING("failed to find hub by address '%s'", hub_address);
        hub = hub_find_by_nick(nick);
    }

    if(hub && strcmp(hub->me->nick, nick) == 0)
    {
        if(auto_match)
        {
            ui_send_status_message(ui, hub->address,
                    "Ignored attempt to auto-match filelist with myself");
        }
        else
        {
            share_save(global_share, FILELIST_XML);
            char *xml_filename;
            int num_returned_bytes = asprintf(&xml_filename, "%s/files.xml", global_working_directory);
            if (num_returned_bytes == -1)
                DEBUG("asprintf did not return anything");
            ui_send_filelist_finished(NULL, hub->address, nick, xml_filename);
            free(xml_filename);
        }
    }
    else
    {
        /* First look for an already downloaded filelist
         */
        char *existing_filelist = 0;
        if(!force_update)
        {
            existing_filelist = find_filelist(global_working_directory, nick);
        }

        if(existing_filelist)
        {
            /* FIXME: code duplication from queue_match.c ! */
            if(auto_match)
            {
                if(is_filelist(existing_filelist) == FILELIST_XML)
                {
                    queue_match_filelist(existing_filelist, nick);
                }
                else
                {
                    ui_send_status_message(ui, hub_address,
                            "User %s doesn't support auto-matching filelists");
                }
            }
            else
            {
                ui_send_status_message(ui, hub_address,
                        "Using cached copy of %s's filelist", nick);
                ui_send_filelist_finished(NULL, hub_address,
                        nick, existing_filelist);
            }
            free(existing_filelist);
        }
        else
        {
            /* none found, queue it for download */
            queue_add_filelist(nick, auto_match);
        }
    }

    return 0;
}

static int ui_cb_queue_remove_target(ui_t *ui, const char *target_filename)
{
    cc_cancel_transfer(target_filename);
    queue_remove_target(target_filename);

    return 0;
}

static int ui_cb_queue_remove_directory(ui_t *ui, const char *target_directory)
{
    cc_cancel_directory_transfers(target_directory);
    queue_remove_directory(target_directory);

    return 0;
}

static int ui_cb_queue_remove_filelist(ui_t *ui, const char *nick)
{
    cc_t *cc = cc_find_by_nick(nick);
    if(cc)
    {
        if(cc->direction == CC_DIR_DOWNLOAD && cc->current_queue &&
                cc->current_queue->is_filelist)
        {
            cc_close_connection(cc);
        }
    }
    queue_remove_filelist(nick);

    return 0;
}

static int ui_cb_queue_remove_source(ui_t *ui, const char *target_filename, const char *nick)
{
    cc_cancel_transfer(target_filename);
    queue_remove_source(target_filename, nick);

    return 0;
}

static int ui_cb_queue_remove_nick(ui_t *ui, const char *nick)
{
    cc_t *cc = cc_find_by_nick(nick);
    if(cc)
        cc_close_connection(cc);
    queue_remove_nick(nick);
    return 0;
}

static int ui_cb_cancel_transfer(ui_t *ui, const char *target_filename)
{
    cc_cancel_transfer(target_filename);
    return 0;
}

static int ui_cb_set_transfer_stats_interval(ui_t *ui, unsigned int seconds)
{
    if(seconds > 0)
    {
        cc_set_transfer_stats_interval(seconds);
    }

    return 0;
}

static int ui_cb_rescan_share_interval(ui_t *ui, unsigned int seconds)
{
	set_share_rescan_interval(seconds);
    return 0;
}

static int ui_cb_set_port(ui_t *ui, int port)
{
    if(port <= 0)
    {
        WARNING("invalid port %i, ignored", port);
        ui_send_status_message(ui, NULL, "Invalid port %i", port);
    }
    else
    {
        if(start_client_listener(port) == 0 && start_search_listener(port) == 0)
        {
	    hub_set_passive(false);
            global_port = port;
            ui_send_status_message(NULL, NULL, "Using port %i", port);
        }
    }

    return 0;
}

static int ui_cb_set_ip_address(ui_t *ui, const char *ip_address)
{
    if(strcmp(ip_address, "auto-detect") == 0)
        extip_set_static(NULL);
    else
        extip_set_static(ip_address);

    return 0;
}

static int ui_cb_set_allow_hub_ip_override(ui_t *ui, int allow_hub_override)
{
	extip_set_override(allow_hub_override);
	return 0;
}

static int ui_cb_add_shared_path(ui_t *ui, const char *path)
{
    char *expanded_path = tilde_expand_path(path);
    char *xpath = str_trim_end_inplace(expanded_path, "/");

    int rc = share_add(global_share, xpath);
    if(rc != 0)
    {
        ui_send_status_message(NULL, NULL,
                "Skipping shared path %s (unavailable?)", path);
	if(--global_expected_shared_paths == 0)
	{
	    global_init_completion = 200;
	    ui_send_init_completion(NULL, global_init_completion);
	}
    }

    free(expanded_path);

    return 0;
}

static int ui_cb_remove_shared_path(ui_t *ui, const char *path)
{
    char *expanded_path = tilde_expand_path(path);
    char *xpath = str_trim_end_inplace(expanded_path, "/");

    if(share_remove(global_share, xpath, false) == 0)
    {
        ui_send_status_message(NULL, NULL, "Removed shared path %s...", path);
    }
    free(expanded_path);
    return 0;
}

static int ui_cb_shutdown(ui_t *ui)
{
    shutdown_sphubd();
    /* will not return */
    return 0;
}

static int ui_cb_set_password(ui_t *ui, const char *hub_address, const char *password)
{
    hub_t *hub = hub_find_by_address(hub_address);
    if(hub == 0)
        WARNING("hub not found: '%s'", hub_address);
    else
    {
        hub_set_password(hub, password);
    }
    return 0;
}

static int ui_cb_update_user_info(ui_t *ui, const char *speed, const char *description, const char *email)
{
    hub_update_user_info(speed, description, email);
    hub_set_need_myinfo_update(true);
    return 0;
}

static int ui_cb_set_slots(ui_t *ui, unsigned int slots, unsigned int per_hub_flag)
{
    hub_set_slots(slots, per_hub_flag);
    return 0;
} 

static int ui_cb_set_passive(ui_t *ui, int on)
{
    hub_set_passive(on);
    start_search_listener(on ? 0 : global_port);
    start_client_listener(on ? 0 : global_port);
    return 0;
} 

static int ui_cb_forget_search(ui_t *ui, int search_id)
{
    sl_forget_search(global_search_listener, search_id);
    return 0;
} 

static int ui_cb_log_level(ui_t *ui, const char *level)
{
    sp_log_set_level(level);
    return 0;
}

static int ui_cb_raw_command(ui_t *ui, const char *hub_address, const char *command)
{
    hub_t *hub = hub_find_by_address(hub_address);
    if(hub == 0)
    {
        WARNING("unknown hub [%s]", hub_address);
    }
    else
    {
        char *command_unescaped = nmdc_unescape(command);
        if(command_unescaped)
        {
            str_trim_end_inplace(command_unescaped, NULL);
            if(str_has_suffix(command_unescaped, "|") == 0)
            {
                WARNING("raw command lacks terminating '|', ignoring");
            }
            else
            {
                hub_send_command(hub, "%s", command_unescaped);
            }
        }
        free(command_unescaped);
    }
    return 0;
}

static int ui_cb_set_priority(ui_t *ui, const char *target_filename, unsigned int priority)
{
    queue_set_priority(target_filename, priority);
    return 0;
}

static int ui_cb_set_follow_redirects(ui_t *ui, unsigned int flag)
{
    global_follow_redirects = flag;
    return 0;
}

static int ui_cb_grant_slot(ui_t *ui, const char *nick)
{
    extra_slots_grant(nick, 1);
    return 0;
}

static int ui_cb_pause_hashing(ui_t *ui)
{
    hs_pause();
    return 0;
}

static int ui_cb_resume_hashing(ui_t *ui)
{
    hs_resume();
    return 0;
}

static int ui_cb_set_auto_search(ui_t *ui, int enabled)
{
    queue_schedule_auto_search_sources(enabled);
    return 0;
}

static int ui_cb_set_hash_prio(ui_t *ui, unsigned int prio)
{
    hs_set_prio(prio);
    return 0;
}

static int ui_cb_set_download_directory(ui_t *ui, const char *download_directory)
{
    if(download_directory)
    {
        free(global_download_directory);
        global_download_directory = tilde_expand_path(download_directory);
    }

    return 0;
}

static int ui_cb_set_incomplete_directory(ui_t *ui, const char *incomplete_directory)
{
    if(incomplete_directory)
    {
        free(global_incomplete_directory);
        global_incomplete_directory = tilde_expand_path(incomplete_directory);
    }
    
    return 0;
}

static int ui_cb_expect_shared_paths(ui_t *ui, int expected_shared_paths)
{
	DEBUG("expecting %i shared paths, init level = %i",
		expected_shared_paths, global_init_completion);
	global_expected_shared_paths = expected_shared_paths;

	if(global_expected_shared_paths == 0 && global_init_completion >= 100)
	{
		DEBUG("no shared paths to wait for");
		global_init_completion = 200;
		ui_send_init_completion(ui, global_init_completion);
	}

	return 0;
}

void ui_send_state_event(int fd, short condition, void *data)
{
    ui_t *ui = data;
    return_if_fail(ui);

    if(global_init_completion < 100)
    {
	/* we're not done with startup yet, try again in a while */
	DEBUG("delaying sending ui state until init complete");
	struct timeval tv = {.tv_sec = 0, .tv_usec = 500000};
	evtimer_add(&ui->send_state_event, &tv);
    }
    else
	ui_send_state(ui);
}

void ui_accept_connection(int fd, short condition, void *data)
{
    int afd = io_accept_connection(fd);
    if(afd == -1)
        return;

    io_set_blocking(afd, 0);

    ui_t *ui = ui_init();
    ui->fd = afd;

    /* setup callbacks */
    ui->cb_search = ui_cb_search;
    ui->cb_search_all = ui_cb_search_all;
    ui->cb_connect = ui_cb_connect;
    ui->cb_disconnect = ui_cb_disconnect;
    ui->cb_public_message = ui_cb_public_message;
    ui->cb_private_message = ui_cb_private_message;
    ui->cb_download_file = ui_cb_download_file;
    ui->cb_download_filelist = ui_cb_download_filelist;
    ui->cb_download_directory = ui_cb_download_directory;
    ui->cb_queue_remove_target = ui_cb_queue_remove_target;
    ui->cb_queue_remove_directory = ui_cb_queue_remove_directory;
    ui->cb_queue_remove_filelist = ui_cb_queue_remove_filelist;
    ui->cb_queue_remove_source = ui_cb_queue_remove_source;
    ui->cb_queue_remove_nick = ui_cb_queue_remove_nick;
    ui->cb_cancel_transfer = ui_cb_cancel_transfer;
    ui->cb_transfer_stats_interval = ui_cb_set_transfer_stats_interval;
    ui->cb_rescan_share_interval = ui_cb_rescan_share_interval;
    ui->cb_set_port = ui_cb_set_port;
    ui->cb_set_ip_address = ui_cb_set_ip_address;
	ui->cb_set_allow_hub_ip_override = ui_cb_set_allow_hub_ip_override;
    ui->cb_add_shared_path = ui_cb_add_shared_path;
    ui->cb_remove_shared_path = ui_cb_remove_shared_path;
    ui->cb_shutdown = ui_cb_shutdown;
    ui->cb_set_password = ui_cb_set_password;
    ui->cb_update_user_info = ui_cb_update_user_info;
    ui->cb_set_slots = ui_cb_set_slots;
    ui->cb_set_passive = ui_cb_set_passive;
    ui->cb_forget_search = ui_cb_forget_search;
    ui->cb_log_level = ui_cb_log_level;
    ui->cb_raw_command = ui_cb_raw_command;
    ui->cb_set_priority = ui_cb_set_priority;
    ui->cb_set_follow_redirects = ui_cb_set_follow_redirects;
    ui->cb_grant_slot = ui_cb_grant_slot;
    ui->cb_pause_hashing = ui_cb_pause_hashing;
    ui->cb_resume_hashing = ui_cb_resume_hashing;
    ui->cb_set_auto_search = ui_cb_set_auto_search;
    ui->cb_set_hash_prio = ui_cb_set_hash_prio;
    ui->cb_set_download_directory = ui_cb_set_download_directory;
    ui->cb_set_incomplete_directory = ui_cb_set_incomplete_directory;
    ui->cb_expect_shared_paths = ui_cb_expect_shared_paths;

    /* add the channel to the list of connected uis.  */
    DEBUG("adding new ui on file descriptor %d", afd);

    /* add the channel to the event loop */
    ui->bufev = bufferevent_new(ui->fd,
            ui_in_event, ui_out_event, ui_err_event, ui);
    bufferevent_enable(ui->bufev, EV_READ | EV_WRITE);

    ui_add(ui);
    ui_send_server_version(ui, VERSION);
    ui_send_init_completion(ui, global_init_completion);
    ui_send_port(ui, global_port);

    evtimer_set(&ui->send_state_event, ui_send_state_event, ui);
    struct timeval tv = {.tv_sec = 0, .tv_usec = 0};
    evtimer_add(&ui->send_state_event, &tv);
}

