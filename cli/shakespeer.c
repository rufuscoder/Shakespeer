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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <event.h>

#include <stdio.h>
#include <assert.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>

#include "shakespeer.h"
#include "log.h"
#include "xstr.h"
#include "rx.h"

ctx_t context = CTX_MAIN;

sphub_t *current_hub = 0;

int debug = 0;
cfg_t *cfg = 0;

sp_filelist_head_t filelists = LIST_HEAD_INITIALIZER();
sp_hublist_head_t hubs = LIST_HEAD_INITIALIZER();
sp_shared_path_list_head_t shared_paths = LIST_HEAD_INITIALIZER();
sp_transfer_list_head_t transfers = LIST_HEAD_INITIALIZER();

fl_dir_t *filelist_cdir = 0;
sp_filelist_t *current_filelist = 0;
char *working_directory = 0;
unsigned long long total_share_size = 0;
const char *debug_level = "message";
int passive_mode = 0;

/* FIXME: move to separate source files */

sp_filelist_t *sp_read_filelist(const char *filename)
{
    sp_filelist_t *fl = 0;

    fl_dir_t *root = fl_parse(filename, NULL);
    if(root)
    {
        fl = calloc(1, sizeof(sp_filelist_t));

        rx_subs_t *subs = rx_search(filename, "files.xml.(.+)");
        if(subs && subs->nsubs == 1)
            fl->nick = strdup(subs->subs[0]);
        else
            fl->nick = strdup("dunno");
        rx_free_subs(subs);

        fl->hubaddress = strdup("dunno");
        fl->root = root;
    }

    return fl;
}

sp_filelist_t *fl_lookup_nick(const char *nick)
{
    sp_filelist_t *fl;
    LIST_FOREACH(fl, &filelists, link)
    {
        if(strcmp(nick, fl->nick) == 0)
            return fl;
    }

    return NULL;
}

static queue_item_t *queue_lookup_by_target(const char *target_filename)
{
    queue_item_t *qi = NULL;
    LIST_FOREACH(qi, &queue_head, link)
    {
        if(strcmp(qi->local_filename, target_filename) == 0)
            break;
    }

    return qi;
}

static queue_source_item_t *queue_source_lookup_by_nick(queue_item_t *qi,
        const char *nick)
{
    queue_source_item_t *qs = NULL;
    LIST_FOREACH(qs, &qi->sources, link)
    {
        if(strcmp(qs->nick, nick) == 0)
            break;
    }

    return qs;
}

static shared_path_t *shared_path_lookup_by_path(const char *path)
{
    shared_path_t *sp = NULL;
    LIST_FOREACH(sp, &shared_paths, link)
    {
        if(strcmp(sp->path, path) == 0)
            break;
    }

    return sp;
}

void add_transfer(const char *local_filename)
{
    transfer_t *tr = calloc(1, sizeof(transfer_t));
    tr->local_filename = xstrdup(local_filename);

    LIST_INSERT_HEAD(&transfers, tr, link);
}

transfer_t *find_transfer_by_target(const char *target_filename)
{
    transfer_t *tr = NULL;
    LIST_FOREACH(tr, &transfers, link)
    {
        if(strcmp(target_filename, tr->local_filename) == 0)
            break;
    }

    return tr;
}

void remove_transfer(const char *local_filename)
{
    transfer_t *tr = find_transfer_by_target(local_filename);
    if(tr)
    {
        LIST_REMOVE(tr, link);

        free(tr->local_filename);
        free(tr);
    }
}

sphub_t *find_hub_by_host(const char *hostname)
{
    sphub_t *hub;
    
    LIST_FOREACH(hub, &hubs, link)
    {
        if(strcmp(hub->address, hostname) == 0 || strcmp(hub->name, hostname) == 0)
            return hub;
    }

    msg("hub address %s not found", hostname);
    return 0;
}

static sp_user_t *find_user_by_nick(sphub_t *hub, const char *nick)
{
    sp_user_t *user;
    LIST_FOREACH(user, &hub->users, link)
    {
        if(strcmp(nick, user->nick) == 0)
            return user;
    }

    return NULL;
}

static void vcmsg(const char *color, const char *fmt, va_list ap)
{
    char *str;
    vasprintf(&str, fmt, ap);

    if(color)
    {
        const char *end_color = cfg_getstr(cfg, "normal-color");
        const char *start_color = cfg_getstr(cfg, color);
        if(start_color)
        {
            char *cstr;
            asprintf(&cstr, "%s%s%s", start_color, str, end_color);
            cmd_display_text(cstr);
            free(cstr);
        }
        else
            color = NULL;
    }

    if(color == NULL)
        cmd_display_text(str);

    free(str);
}

void cmsg(const char *color, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vcmsg(color, fmt, ap);
    va_end(ap);
}

void msg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vcmsg(NULL, fmt, ap);
    va_end(ap);
}

static int spcb_logout(sp_t *sp, const char *_hub, const char *nick)
{
    sphub_t *hub = find_hub_by_host(_hub);
    if(hub)
    {
        if(debug)
            msg("user %s logged out", nick);

        sp_user_t *user = find_user_by_nick(hub, nick);
        if(user)
        {
            LIST_REMOVE(user, link);

            /* FIXME: make a function of this */
            free(user->nick);
            free(user->speed);
            free(user->description);
            free(user->tag);
            free(user->email);
            free(user);
        }
        else
            msg("user %s logged out but I didn't know he was logged in!?", nick);
    }
    return 0;
}

static int spcb_login(sp_t *sp, const char *_hub, const char *nick,
        const char *description, const char *tag, const char *speed,
        const char *email, unsigned long long share_size, int is_operator,
        unsigned int extra_slots)
{
    sphub_t *hub = find_hub_by_host(_hub);
    if(hub)
    {
        sp_user_t *user = find_user_by_nick(hub, nick);
        if(user == NULL)
        {
            /* msg("user %s logged in, sharing %s, speed %s",
                    nick, str_size_human(share_size), speed);    */
            user = calloc(1, sizeof(sp_user_t));
            user->nick = xstrdup(nick);
            LIST_INSERT_HEAD(&hub->users, user, link);
        }
        user->size = share_size;
        free(user->speed);
        user->speed = xstrdup(speed);
        free(user->description);
        user->description = xstrdup(description);
        free(user->email);
        user->email = xstrdup(email);
        free(user->tag);
        user->tag = xstrdup(tag);
        user->is_operator = is_operator;
    }
    return 0;
}

static int spcb_public_message(sp_t *sp, const char *hub, const char *nick, const char *message)
{
    cmsg("info-color", "<%s>: %s", nick, message);
    return 0;
}

static int spcb_private_message(sp_t *sp, const char *hub, const char *my_nick,
       const char *remote_nick, const char *display_nick, const char *message)
{
    cmsg("info2-color", "<%s>: %s", display_nick, message);
    return 0;
}

static int spcb_filelist_finished(sp_t *sp, const char *hub, const char *nick, const char *filename)
{
    msg("filelist for %s ready in %s", nick, filename);

    sp_filelist_t *old_fl = fl_lookup_nick(nick);
    if(old_fl)
    {
        LIST_REMOVE(old_fl, link);

        free(old_fl->nick);
        free(old_fl->hubaddress);
        /* FIXME: free old_fl->root, create new func sp_filelist_free() */
        free(old_fl);
    }

    fl_dir_t *root = fl_parse(filename, NULL);
    if(root)
    {
        sp_filelist_t *fl = calloc(1, sizeof(sp_filelist_t));
        fl->root = root;
        fl->nick = xstrdup(nick);
        fl->hubaddress = xstrdup(hub);

        LIST_INSERT_HEAD(&filelists, fl, link);

        filelist_cdir = fl->root;
        current_filelist = fl;
        sort_filelist(fl);
        context = CTX_FILELIST;
    }
    else
        msg("failed to parse filelist from %s", filename);
    remove_transfer(filename);
    return 0;
}

static int spcb_search_response(sp_t *sp, int id, const char *hub_address,
        const char *nick, const char *filename, int filetype,
        unsigned long long size, int openslots, int totalslots, const char *tth,
        const char *speed)
{
    sphub_t *hub = find_hub_by_host(hub_address);
    if(hub)
    {
        sr_t *sr = calloc(1, sizeof(sr_t));
        sr->id = id; 
        sr->hub_address = xstrdup(hub_address);
        sr->nick = xstrdup(nick);
        sr->filename = xstrdup(filename);
        sr->filetype = filetype;
        sr->size = size;
        sr->openslots = openslots;
        sr->totalslots = totalslots;
        sr->tth = xstrdup(tth);

        LIST_INSERT_HEAD(&hub->search_results, sr, link);
    }
    return 0;
}

static int spcb_status_message(sp_t *sp, const char *hub, const char *message)
{
    cmsg("status-color", "%s", message);
    return 0;
}

static int spcb_hubname_changed(sp_t *sp, const char *hub, const char *new_name)
{
    msg("new hubname: %s", new_name);
    if(current_hub)
    {
        free(current_hub->name);
        current_hub->name = xstrdup(new_name);
    }
    return 0;
}

static int spcb_download_starting(sp_t *sp, const char *hub, const char *nick,
        const char *remote_filename, const char *local_filename, unsigned long long filesize)
{
    cmsg("info-color", "starting download of %s (%s) from %s",
            remote_filename, str_size_human(filesize), nick);
    add_transfer(local_filename);
    return 0;
}

static int spcb_upload_starting(sp_t *sp, const char *hub, const char *nick, const char *local_filename, unsigned long long filesize)
{
    cmsg("info2-color", "starting upload of %s (%s) to %s",
            local_filename, str_size_human(filesize), nick);
    add_transfer(local_filename);
    return 0;
}

static int spcb_download_finished(sp_t *sp, const char *local_filename)
{
    cmsg("status-color", "download of %s finished", local_filename);
    remove_transfer(local_filename);
    return 0;
}

static int spcb_upload_finished(sp_t *sp, const char *local_filename)
{
    cmsg("status-color", "upload of %s finished", local_filename);
    remove_transfer(local_filename);
    return 0;
}

static int spcb_transfer_aborted(sp_t *sp, const char *local_filename)
{
    remove_transfer(local_filename);
    return 0;
}

static int spcb_source_add(sp_t *sp, const char *local_filename, const char *nick,
        const char *remote_filename)
{
    queue_item_t *qi = queue_lookup_by_target(local_filename);
    if(qi)
    {
        queue_source_item_t *qs = calloc(1, sizeof(queue_source_item_t));
        qs->nick = xstrdup(nick);
        qs->remote_filename = xstrdup(remote_filename);

        LIST_INSERT_HEAD(&qi->sources, qs, link);
    }
    return 0;
}

static int spcb_queue_add_target(sp_t *sp, const char *local_filename,
        unsigned long long size, const char *tth,
        unsigned int priority)
{
    queue_item_t *qi = calloc(1, sizeof(queue_item_t));
    qi->local_filename = xstrdup(local_filename);
    qi->size = size;
    qi->tth = xstrdup(tth);

    LIST_INSERT_HEAD(&queue_head, qi, link);

    return 0;
}

static int spcb_queue_add_filelist(sp_t *sp, const char *nick, unsigned int priority)
{
    queue_item_t *qi = calloc(1, sizeof(queue_item_t));

    asprintf(&qi->local_filename, "<filelist-%s>", nick);

    LIST_INSERT_HEAD(&queue_head, qi, link);

    spcb_source_add(sp, qi->local_filename, nick, qi->local_filename);

    return 0;
}

static void queue_source_free(queue_source_item_t *qs)
{
    free(qs->remote_filename);
    free(qs->nick);
    free(qs);
}

static int spcb_source_remove(sp_t *sp, const char *local_filename, const char *nick)
{
    queue_item_t *qi = queue_lookup_by_target(local_filename);
    if(qi)
    {
        queue_source_item_t *qs = queue_source_lookup_by_nick(qi, nick);
        if(qs)
        {
            g_message("removing source for local filename '%s', nick %s",
                    local_filename, nick);
            LIST_REMOVE(qs, link);
            queue_source_free(qs);
        }
    }
    return 0;
}

static int spcb_queue_remove(sp_t *sp, const char *local_filename)
{
    queue_item_t *qi = queue_lookup_by_target(local_filename);
    if(qi)
    {
        g_message("removing queue item for local filename '%s'", local_filename);
        LIST_REMOVE(qi, link);

        free(qi->local_filename);
        free(qi->tth);

        /* g_list_foreach(qi->sources, free_queue_source, NULL); */ /* FIXME! */

        free(qi);
    }
    return 0;
}

static int spcb_queue_remove_filelist(sp_t *sp, const char *nick)
{
    char *target_filename;
    asprintf(&target_filename, "<filelist-%s>", nick);

    queue_item_t *qi = queue_lookup_by_target(target_filename);
    if(qi)
    {
        g_message("removing queue item for local filename '%s'", target_filename);
        LIST_REMOVE(qi, link);

        free(qi->local_filename);
        free(qi->tth);
        /* g_list_foreach(qi->sources, free_queue_source, NULL); */ /* FIXME! */
        free(qi);
    }
    return 0;
}

static int spcb_hub_redirect(sp_t *sp, const char *hub, const char *new_address)
{
    msg("Redirected to %s", new_address);
    sphub_t *sphub = find_hub_by_host(hub);
    if(sphub)
    {
        free(sphub->address);
        sphub->address = xstrdup(new_address);
    }
    return 0;
}

static int spcb_transfer_stats(sp_t *sp, const char *local_filename,
        unsigned long long offset, unsigned long long filesize,
        unsigned bytes_per_sec)
{
    const char *filename = strrchr(local_filename, '/');
    if(filename++ == NULL)
        filename = local_filename;

    msg("%s is %.1f%% complete (%s of %s): %s/s", filename,
            100 * ((float)offset / filesize),
            str_size_human(offset),
            str_size_human(filesize),
            str_size_human(bytes_per_sec));

    return 0;
}

static int spcb_hub_add(sp_t *sp, const char *address, const char *hubname,
        const char *nick, const char *description, const char *encoding)
{
    current_hub = calloc(1, sizeof(sphub_t));
    current_hub->nick = xstrdup(nick);
    current_hub->address = xstrdup(address);
    current_hub->name = xstrdup(hubname);

    LIST_INSERT_HEAD(&hubs, current_hub, link);

    context = CTX_HUB;
    msg("added hub %s@%s", current_hub->nick, current_hub->address);

    return 0;
}

static int spcb_port(sp_t *sp, int port)
{
    int cfg_port = cfg_getint(cfg, "port");
    int passive = cfg_getbool(cfg, "passive");
    if(passive)
    {
        sp_send_set_passive(sp, 1);
    }
    else if(port != cfg_port)
    {
        /* set the search and download port */
        sp_send_set_passive(sp, 0);
        sp_send_set_port(sp, cfg_port);
    }
    return 0;
}

static int spcb_connection_closed(sp_t *sp, const char *nick, int direction)
{
    cmsg("warning-color", "connection closed with %s", nick);
    return 0;
}

static int spcb_share_stats(sp_t *sp,
        const char *path,
        unsigned long long size, unsigned long long totsize,
        unsigned long long dupsize,
        unsigned nfiles, unsigned ntotfiles, unsigned nduplicates)
{
    total_share_size = size;

    if(path && path[0])
    {
        shared_path_t *spath = shared_path_lookup_by_path(path);
        if(spath == NULL)
        {
            spath = calloc(1, sizeof(shared_path_t));
            spath->path = strdup(path);

            LIST_INSERT_HEAD(&shared_paths, spath, link);
        }

        spath->size = size;
        spath->nfiles = nfiles;
        spath->ntotfiles = ntotfiles;
    }

    return 0;
}

static int spcb_server_version(sp_t *sp, const char *version)
{
    msg("server version is %s", version);
    if(strcmp(version, VERSION) != 0)
    {
        msg("WARNING: version mismatch! You should restart sphubd.");
    }
    return 0;
}

void sighandler(int sig)
{
    switch(sig)
    {
        case SIGINT:
            cmd_fini();
            exit(0);
            break;
        case SIGTSTP:
            cmd_normal_io();
            signal(SIGTSTP, SIG_DFL);
            raise(sig);
            break;
        case SIGCONT:
            cmd_raw_io();
            break;
    }
}

static int sp_cmd_callback(void *gl, void *data, int fd, int event)
{
    sp_t *sp = data;
    assert(sp);
    int rc = sp_in_event(fd, EV_READ, sp);
    return rc == 0 ? 1 : 0;
}

int sp_send_string(sp_t *sp, const char *string)
{
    if(sp->output == NULL)
    {
        sp->output = evbuffer_new();
    }
    evbuffer_add(sp->output, (void *)string, strlen(string));
    evbuffer_write(sp->output, sp->fd);
    return 0;
}

int main(int argc, char **argv)
{
    char *remote_sphubd_host = NULL;
    
    cfg = parse_config();
    if(cfg == NULL)
    {
        return 1;
    }
    passive_mode = cfg_getbool(cfg, "passive");
    debug_level = cfg_getstr(cfg, "log-level");

    int c;
    while((c = getopt(argc, argv, "w:d:h:")) != EOF)
    {
        switch(c)
        {
            case 'w':
                working_directory = verify_working_directory(optarg);
                break;
            case 'd':
                debug_level = optarg;
                break;
            case 'h':
                remote_sphubd_host = optarg;
                break;
            case '?':
            default:
                return 3;
        }
    }

    argc -= optind;
    argv += optind;

    if(working_directory == NULL)
    {
        working_directory = verify_working_directory(cfg_getstr(cfg,
                    "working-directory"));
    }
    sp_log_init(working_directory, "shakespeer");
    sp_log_set_level(debug_level);

    /* connect to the sphubd server... */
    sp_t *sp = sp_create(NULL);

    /* setup the callback functions */
    sp->cb_user_logout = spcb_logout;
    sp->cb_user_login = spcb_login;
    sp->cb_user_update = spcb_login;
    sp->cb_public_message = spcb_public_message;
    sp->cb_private_message = spcb_private_message;
    sp->cb_search_response = spcb_search_response;
    sp->cb_filelist_finished = spcb_filelist_finished;
    sp->cb_hubname = spcb_hubname_changed;
    sp->cb_status_message = spcb_status_message;
    sp->cb_download_starting = spcb_download_starting;
    sp->cb_upload_starting = spcb_upload_starting;
    sp->cb_download_finished = spcb_download_finished;
    sp->cb_upload_finished = spcb_upload_finished;
    sp->cb_transfer_aborted = spcb_transfer_aborted;
    sp->cb_queue_add_target = spcb_queue_add_target;
    sp->cb_queue_add_filelist = spcb_queue_add_filelist;
    sp->cb_queue_add_source = spcb_source_add;
    sp->cb_queue_remove_target = spcb_queue_remove;
    sp->cb_queue_remove_filelist = spcb_queue_remove_filelist;
    sp->cb_queue_remove_source = spcb_source_remove;
    sp->cb_hub_redirect = spcb_hub_redirect;
    sp->cb_transfer_stats = spcb_transfer_stats;
    sp->cb_hub_add = spcb_hub_add;
    sp->cb_port = spcb_port;
    sp->cb_connection_closed = spcb_connection_closed;
    sp->cb_share_stats = spcb_share_stats;
    sp->cb_server_version = spcb_server_version;

    if(remote_sphubd_host)
    {
        if(sp_connect_remote(sp, remote_sphubd_host) != 0)
        {
            printf("Unable to connect to remote sphubd, exiting\n");
            return 8;
        }
    }
    else if(sp_connect(sp, working_directory,
                  cfg_getstr(cfg, "sphubd-executable-path")) != 0)
    {
        printf("There was an error when connecting to the sphubd program\n");
        printf("The configured path is '%s'\n", cfg_getstr(cfg, "sphubd-executable-path"));
        printf("To change this, specify \"sphubd-executable-path = '/path/sphubd'\" in the\n"
                "configuration file ~/.shakespeer.conf\n");
        return 7;
    }

    /* add any shared paths from config file */
    int i;
    for(i = 0; i < cfg_size(cfg, "shared-paths"); i++)
    {
        sp_send_add_shared_path(sp, cfg_getnstr(cfg, "shared-paths", i));
    }

    /* set the update interval for transfer stats */
    sp_send_transfer_stats_interval(sp, 10);

    signal(SIGTSTP, sighandler);
    signal(SIGCONT, sighandler);
    signal(SIGINT, sighandler);
    signal(SIGPIPE, SIG_IGN);

    cmd_init();
    cmd_add_watch(sp->fd, sp_cmd_callback, sp);
    cmd_run_loop(sp);

    /* will never be reached */

    return 0;
}

