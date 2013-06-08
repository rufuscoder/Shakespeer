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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <event.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "share.h"
#include "nfkc.h"
#include "log.h"
#include "notifications.h"

#include "globals.h"
#include "ui_send.h"

typedef struct share_scan_directory share_scan_directory_t;
struct share_scan_directory
{
    LIST_ENTRY(share_scan_directory) link;
    char *dirpath;
};

typedef struct share_scan_state share_scan_state_t;
struct share_scan_state
{
    LIST_HEAD(, share_scan_directory) directories;
    share_t *share;
    struct event ev;
    share_mountpoint_t *mp;
};

#define SHARE_STAT_TO_INODE(st) (uint64_t)(((uint64_t)st->st_size << 32) | st->st_ino)

static void share_scan_schedule_event(share_scan_state_t *ctx);

static int share_skip_file(const char *filename)
{
    if(filename[0] == '.')
        return 1;
    if(strchr(filename, '$') != NULL)
        return 1;
    if(strchr(filename, '|') != NULL)
        return 1;
    return 0;
}

static void share_scan_add_file(share_scan_state_t *ctx,
        const char *filepath, struct stat *stbuf)
{
    return_if_fail(filepath);
    return_if_fail(ctx);
    return_if_fail(ctx->mp);
    return_if_fail(ctx->mp->local_root);

    /* is it already hashed? */
    bool already_hashed = false;

    uint64_t inode = SHARE_STAT_TO_INODE(stbuf);
    bool is_duplicate = false;

    /* Check if we're already sharing this inode.
     */
    share_file_t *collision_file =
	share_lookup_file_by_inode(ctx->share, inode);
    if(collision_file)
    {
	char *local_path = share_complete_path(collision_file);
	if(ctx->mp != collision_file->mp)
	{
	    WARNING("%"PRIX64": collision between [%s] and [%s]",
		inode, filepath, local_path);
	}
	else
	{
	    INFO("%"PRIX64": collision between [%s] and [%s]",
		inode, filepath, local_path);

	    if(strcmp(local_path, filepath) == 0)
	    {
		WARNING("re-adding the exact same file?");
		free(local_path);
		return;
	    }
	}
	free(local_path);

	is_duplicate = true;
	goto done; /* just update statistics */
    }

    struct tth_inode *ti = tth_store_lookup_inode(global_tth_store, inode);

    if(ti == NULL)
    {
        /* unhashed */
    }
    else if(ti->mtime != stbuf->st_mtime)
    {
	DEBUG("[%s] has an obsolete inode", filepath);
	DEBUG("removing obsolete inode %"PRIX64" for TTH %s (modified)",
	    inode, ti->tth);
	tth_store_remove_inode(global_tth_store, inode);

	/* don't remove any corresponding TTH:
	 * it could be used by a duplicate */
    }
    else
    {
	struct tth_entry *td = tth_store_lookup(global_tth_store, ti->tth);

	if(td == NULL)
	{
	    /* This is an inode without a TTH, which is useless */
	    DEBUG("removing obsolete inode %"PRIX64" (missing TTH)", inode);
	    tth_store_remove_inode(global_tth_store, inode);
	}
	else if(td->active_inode == 0)
	{
	    /* TTH is not active, claim this TTH for this inode */
	    tth_store_set_active_inode(global_tth_store, ti->tth, inode);
	    already_hashed = true;
	}
	else if(td->active_inode != inode)
	{
	    already_hashed = true;

	    /* DEBUG("duplicate TTH for different inodes"); */
	    /* check if the original is shared */
	    share_file_t *original_file =
		share_lookup_file_by_inode(ctx->share, td->active_inode);
	    if(original_file)
	    {
		/* ok, keep as duplicate */
		INFO("skipping duplicate [%s]", filepath);
		nc_send_share_duplicate_found_notification(nc_default(), filepath);
		is_duplicate = true;
	    }
	    else
	    {
		/* original not shared, switch with duplicate */
		/* (this can only happen if shares has been removed live) */
		tth_store_set_active_inode(global_tth_store, ti->tth, inode);
	    }
	}
	else
	{
	    /* we found the same file again: rehash */
	    already_hashed = true;
	}
    }

done:

    if(is_duplicate)
    {
	/* update mount statistics */
	ctx->mp->stats.nduplicates++;
	ctx->mp->stats.dupsize += stbuf->st_size;
    }
    else
    {
	share_file_t *f = calloc(1, sizeof(share_file_t));
	f->partial_path = strdup(filepath + strlen(ctx->mp->local_root));
	f->mp = ctx->mp;
	f->type = share_filetype(f->partial_path);
	f->size = stbuf->st_size;
	f->inode = SHARE_STAT_TO_INODE(stbuf);

	if(already_hashed)
	{
	    /* Insert it in the tree. */
	    RB_INSERT(file_tree, &ctx->share->files, f);

	    /* update the mount statistics */
	    ctx->mp->stats.nfiles++;
	    ctx->mp->stats.size += f->size;

	    /* add it to the bloom filter */
	    char *filename = strrchr(f->partial_path, '/');
	    if(filename++ == NULL)
		filename = f->partial_path;
	    bloom_add_filename(ctx->share->bloom, filename);
	}
	else
	{
	    /* Insert it in the unhashed tree. */
	    RB_INSERT(file_tree, &ctx->share->unhashed_files, f);
	}

	/* Add the file to the inode hash.
	 * We also add unhashed files so we can quickly lookup collisions
	 * without the need to hash. If we after hashing get a duplicate,
	 * the file must be removed from the inode hash table.
	 */
	share_add_to_inode_table(ctx->share, f);
    }

    /* update the mount statistics */
    ctx->mp->stats.ntotfiles++;
    ctx->mp->stats.totsize += stbuf->st_size;

    nc_send_share_file_added_notification(nc_default());
}

static char *share_scan_absolute_path(const char *dirpath,
        const char *filename)
{
    if (!g_utf8_validate(filename, -1, NULL)) {
        WARNING("Unknown encoding in filename [%s]", filename);
        return NULL;
    }
    
    /* FIXME: if filename is not in utf-8, try to convert from some
     * local encoding (locale) */

#ifdef __APPLE__ /* FIXME: is this correct? */
    /* normalize the string (handles different decomposition) */
    /* Can we be sure this is on a HFS+ partition? Nope... */
    char *utf8_filename = g_utf8_normalize(filename, -1, G_NORMALIZE_DEFAULT);
#else
    char *utf8_filename = strdup(filename);
#endif

    char *filepath;
    int num_returned_bytes = asprintf(&filepath, "%s/%s", dirpath, utf8_filename);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
    free(utf8_filename);

    return filepath;
}

static void share_scan_push_directory(share_scan_state_t *ctx,
        const char *dirpath)
{
    share_scan_directory_t *d = calloc(1, sizeof(share_scan_directory_t));
    d->dirpath = strdup(dirpath);

    LIST_INSERT_HEAD(&ctx->directories, d, link);
}

/* Scans the files in the given directory and adds found files. Pushes any
 * subdirectories found to the ctx->directories stack. */
static void share_scan_context(share_scan_state_t *ctx,
        const char *dirpath)
{
    DIR *fsdir;
    struct dirent *dp;

    if(strcmp(dirpath, global_incomplete_directory) == 0)
    {
	INFO("Refused to share incomplete download directory [%s]",
	    dirpath);

	ui_send_status_message(NULL, NULL,
	    "Refused to share incomplete download directory '%s'",
	    dirpath);

	return;
    }
    /* DEBUG("scanning directory [%s]", dirpath); */

    fsdir = opendir(dirpath);
    if(fsdir == 0)
    {
        WARNING("%s: %s", dirpath, strerror(errno));
        return;
    }

    while((dp = readdir(fsdir)) != NULL)
    {
        const char *filename = dp->d_name;
        if(share_skip_file(filename))
        {
            /* INFO("- skipping file %s/%s", dirpath, filename); */
            continue;
        }

        char *filepath = share_scan_absolute_path(dirpath, filename);
        if(filepath == NULL)
            continue;

        struct stat stbuf;
        if(stat(filepath, &stbuf) == 0)
        {
            if(S_ISDIR(stbuf.st_mode))
            {
                share_scan_push_directory(ctx, filepath);
            }
            else if(S_ISREG(stbuf.st_mode))
            {
                if(stbuf.st_size == 0)
                    INFO("- skipping zero-sized file '%s'", filepath);
                else
                    share_scan_add_file(ctx, filepath, &stbuf);
            }
            else /* neither directory nor regular file */
            {
                INFO("- skipping file %s (not a regular file)", filename);
            }
        }
        else
        {
            /* stat failed */
            WARNING("%s: %s", filepath, strerror(errno));
        }

        free(filepath);
    }
    closedir(fsdir);
}

static void share_scan_event(int fd, short why, void *user_data)
{
    share_scan_state_t *ctx = user_data;
    int i;

    /* check if the share currently being scanned has been removed */
    if(ctx->mp->removed)
    {
	WARNING("aborting scanning of removed share [%s]", ctx->mp->local_root);
	share_remove_mountpoint(ctx->share, ctx->mp);
	ctx->share->scanning--;
	free(ctx);
	return;
    }

    /* Scan 5 directories in each event. */
    for(i = 0; i < 5; i++)
    {
        share_scan_directory_t *d = LIST_FIRST(&ctx->directories);
        if(d == NULL)
        {
            INFO("Done scanning directory [%s]", ctx->mp->local_root);
	    INFO("bloom filter is %.1f%% filled",
		bloom_filled_percent(ctx->share->bloom));
            nc_send_share_scan_finished_notification(nc_default(),
                    ctx->mp->local_root);
            ctx->share->uptodate = false;
            ctx->mp->scan_in_progress = false;
            free(ctx);

	    ctx->share->scanning--;
	    return_if_fail(ctx->share->scanning >= 0);

            return;
        }

        char *dirpath = d->dirpath;
        LIST_REMOVE(d, link);
        free(d);

        share_scan_context(ctx, dirpath);
        free(dirpath);
    }

    share_scan_schedule_event(ctx);
}

static void share_scan_schedule_event(share_scan_state_t *ctx)
{
    if(event_initialized(&ctx->ev))
    {
        event_del(&ctx->ev);
    }
    else
    {
        evtimer_set(&ctx->ev, share_scan_event, ctx);
    }

    struct timeval tv = {.tv_sec = 0, .tv_usec = 0};
    event_add(&ctx->ev, &tv);
}

int share_scan(share_t *share, share_mountpoint_t *mp)
{
    return_val_if_fail(share, -1);
    return_val_if_fail(mp, -1);
    return_val_if_fail(!mp->scan_in_progress, -1);

    /* Keep a counter to indicate for the myinfo update event that
     * we should wait until rescanning is done. Otherwise we risk
     * sending out a too low share size that gets us kicked.
     */
    share->scanning++;

    share_scan_state_t *ctx = calloc(1, sizeof(share_scan_state_t));

    LIST_INIT(&ctx->directories);
    ctx->share = share;
    ctx->mp = mp;

    /* reset mountpoint statistics */
    memset(&mp->stats, 0, sizeof(share_stats_t));
    mp->scan_in_progress = true;

    share_scan_push_directory(ctx, mp->local_root);
    share_scan_schedule_event(ctx);

    return 0;
}

