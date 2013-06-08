/*
 * Copyright 2004 Martin Hedenfalk <martin@bzero.se>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libtecla.h>

#include "cfg.h"
#include "spclient.h"
#include "util.h"
#include "args.h"

typedef enum {
    cpl_none,
    cpl_command,
    cpl_nick,
    cpl_hublist,
    cpl_filelist,
    cpl_filelist_dir,
    cpl_filelists,
    cpl_transfer,
    cpl_local_filename
} cpl_t;

typedef struct sp_user sp_user_t;
struct sp_user
{
    LIST_ENTRY(sp_user) link;

    char *nick;
    char *speed;
    char *description;
    char *tag;
    char *email;
    unsigned long long size;
    int is_operator;
};

typedef struct sr sr_t;
struct sr
{
    LIST_ENTRY(sr) link;

    unsigned int id;
    char *hub_address;
    char *nick;
    char *filename;
    int filetype;
    int openslots;
    int totalslots;
    unsigned long long size;
    char *tth;
};

typedef struct sp_filelist sp_filelist_t;
struct sp_filelist
{
    LIST_ENTRY(sp_filelist) link;

    char *nick;
    char *hubaddress;
    fl_dir_t *root;
};

typedef struct sphub sphub_t;
struct sphub
{
    LIST_ENTRY(sphub) link;

    char *name;
    char *address;
    char *nick;
    LIST_HEAD(, sp_user) users;
    LIST_HEAD(, sr) search_results;
};

/* context identifiers in the CLI */
typedef enum ctx
{
    CTX_ALL, /* used in the cmd_t table for commands valid in all contexts */
    CTX_MAIN,
    CTX_HUB,
    CTX_FILELIST,
    CTX_HUBLIST,
    CTX_QUEUE,
} ctx_t;

typedef struct ui_cmd ui_cmd_t;
struct ui_cmd
{
    ctx_t context;
    char *name;
    int minargs;
    int (*func)(sp_t *sp, arg_t *args);
    cpl_t cpl_type;
    char *description;
};

typedef struct queue_source_item queue_source_item_t;
struct queue_source_item
{
    LIST_ENTRY(queue_source_item) link;

    char *remote_filename;
    char *nick;
};

typedef struct queue_source_list queue_source_list_t;
LIST_HEAD(queue_source_list, queue_source_item);

typedef struct queue_item queue_item_t;
struct queue_item
{
    LIST_ENTRY(queue_item) link;

    char *local_filename;
    unsigned long long size;
    char *tth;
    queue_source_list_t sources;
};

typedef struct transfer transfer_t;
struct transfer
{
    LIST_ENTRY(transfer) link;

    char *local_filename;
};

typedef struct shared_path shared_path_t;
struct shared_path
{
    LIST_ENTRY(shared_path) link;

    char *path;
    unsigned long long size;
    unsigned int nfiles;
    unsigned int ntotfiles;
};

typedef struct sp_filelist_head sp_filelist_head_t;
LIST_HEAD(sp_filelist_head, sp_filelist);

typedef struct sp_hublist_head sp_hublist_head_t;
LIST_HEAD(sp_hublist_head, sphub);

typedef struct sp_shared_path_list_head sp_shared_path_list_head_t;
LIST_HEAD(sp_shared_path_list_head, shared_path);

typedef struct sp_transfer_list_head sp_transfer_list_head_t;
LIST_HEAD(sp_transfer_list_head, transfer);

extern cfg_t *cfg;
extern char *working_directory;
extern ctx_t context;
extern int debug;
extern sphub_t *current_hub;
extern fl_dir_t *filelist_cdir;
extern sp_filelist_head_t filelists;
extern sp_filelist_t *current_filelist;
extern sp_hublist_head_t hubs;
extern unsigned long long total_share_size;
extern sp_shared_path_list_head_t shared_paths;
extern int passive_mode;
extern sp_transfer_list_head_t transfers;

sp_filelist_t *fl_lookup_nick(const char *nick);

void cmsg(const char *color, const char *fmt, ...);
void msg(const char *fmt, ...);

/* cmd.c
 */
ui_cmd_t *get_cmd(const char *name);
void cmd_init(void);
void cmd_fini(void);
void cmd_run_loop(void *user_data);
void cmd_add_watch(int fd, int (*cmd_callback)(void *gl, void *data, int fd, int event), void *user_data);
void cmd_normal_io(void);
void cmd_raw_io(void);
void cmd_display_text(const char *str);

/* ctx-main.c
 */
int func_set_hash_prio(sp_t *sp, arg_t *args);
int func_debug(sp_t *sp, arg_t *args);
int func_exit(sp_t *sp, arg_t *args);
int func_connect(sp_t *sp, arg_t *args);
int func_hublist(sp_t *sp, arg_t *args);
int func_hub(sp_t *sp, arg_t *args);
int func_ls(sp_t *sp, arg_t *args);
int func_cancel_transfer(sp_t *sp, arg_t *args);
int func_set_port(sp_t *sp, arg_t *args);
int func_add_shared_path(sp_t *sp, arg_t *args);
int func_remove_shared_path(sp_t *sp, arg_t *args);
int func_help(sp_t *sp, arg_t *args);
int func_info(sp_t *sp, arg_t *args);
int func_set_passive(sp_t *sp, arg_t *args);

/* ctx-hub.c
 */
char *filelists_completion_function(const char *text, int state);
void sort_filelist(sp_filelist_t *fl);
int func_hub_exit(sp_t *sp, arg_t *args);
int func_hub_disconnect(sp_t *sp, arg_t *args);
int func_hub_search(sp_t *sp, arg_t *args);
int func_hub_msg(sp_t *sp, arg_t *args);
int func_hub_hmsg(sp_t *sp, arg_t *args);
int func_hub_browse(sp_t *sp, arg_t *args);
int func_hub_search_results(sp_t *sp, arg_t *args);
int func_hub_sget(sp_t *sp, arg_t *args);
int func_hub_lfilelists(sp_t *sp, arg_t *args);
int func_hub_rfilelist(sp_t *sp, arg_t *args);
int func_hub_list_users(sp_t *sp, arg_t *args);
int func_hub_queue(sp_t *sp, arg_t *args);
int func_hub_set_password(sp_t *sp, arg_t *args);

/* ctx-filelist.c
 */
int cmd_complete_filelist(WordCompletion *cpl, const char *line, int word_start, int word_end);
int cmd_complete_filelist_directories(WordCompletion *cpl, const char *line, int word_start, int word_end);
int func_filelist_ls(sp_t *sp, arg_t *args);
int func_filelist_lsdirs(sp_t *sp, arg_t *args);
int func_filelist_cd(sp_t *sp, arg_t *args);
int func_filelist_get(sp_t *sp, arg_t *args);
int func_filelist_get_directory(sp_t *sp, arg_t *args);
int func_filelist_exit(sp_t *sp, arg_t *args);
int func_filelist_search(sp_t *sp, arg_t *args);

/* ctx-hublist.c
 */
int cmd_complete_hublist(WordCompletion *cpl, const char *line, int word_start, int word_end);
int func_hublist_refresh(sp_t *sp, arg_t *args);
int func_hublist_ls(sp_t *sp, arg_t *args);
int func_hublist_filter(sp_t *sp, arg_t *args);
int func_hublist_exit(sp_t *sp, arg_t *args);

/* ctx-queue.c
 */
typedef struct queue_list queue_list_t;
LIST_HEAD(queue_list, queue_item);

extern queue_list_t queue_head;

int func_queue_refresh(sp_t *sp, arg_t *args);
int func_queue_ls(sp_t *sp, arg_t *args);
int func_queue_remove(sp_t *sp, arg_t *args);
int func_queue_exit(sp_t *sp, arg_t *args);
int func_queue_remove_nick(sp_t *sp, arg_t *args);
int func_queue_remove_source(sp_t *sp, arg_t *args);
int func_queue_remove_filelist(sp_t *sp, arg_t *args);

sp_filelist_t *sp_read_filelist(const char *filename);

