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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "log.h"
#include "share.h"
#include "globals.h"
#include "notifications.h"
#include "tthdb.h"

/***** handling TTHs and leaf data *****/

static void handle_tth_available_notification(nc_t *nc,
        const char *channel,
        nc_tth_available_t *notification,
        void *user_data)
{
    share_t *share = user_data;
    share_file_t *file = notification->file;

    char *local_path = share_complete_path(file);

    /* We're about to move the file from the unhashed to the hashed tree. */
    /* Start by removing it from the unhashed tree. */
    if(share_lookup_unhashed_file(share, local_path) != NULL)
	RB_REMOVE(file_tree, &share->unhashed_files, file);
    else
	WARNING("File [%s] not in unhashed tree!?", local_path);

    if(notification->tth == NULL)
    {
	/* hashing failed for some reason (eg, permission denied) */
	goto fail;
    }

    /* Find the modification time of the file, so we can detect changes when we
     * re-scan this file. FIXME: should probably store the mtime as it is when
     * _opening_ the file, not afterwards as it is now. If the file is being
     * modified (without affecting the filesize) while we're hashing it, we
     * won't detect this modification.
     */
    struct stat stbuf;
    if(stat(local_path, &stbuf) != 0)
    {
	WARNING("%s: failed to lookup mtime: %s", local_path, strerror(errno));
	goto fail;
    }

    struct tth_entry *te = tth_store_lookup(global_tth_store,
	notification->tth);

    if(te == NULL)
    {
	tth_store_add_entry(global_tth_store,
	    notification->tth,
	    notification->leafdata_base64,
	    0);
    }

    tth_store_add_inode(global_tth_store,
	file->inode, stbuf.st_mtime, notification->tth);

    share->uptodate = false;

    if(te == NULL)
    {
	/* there was no previous conflicting TTH, claim this TTH */
	tth_store_set_active_inode(global_tth_store,
		notification->tth, file->inode);
    }
    else
    {
        /* constraint violation; TTH not unique */
	/* duplicate, check if original is shared */
	share_file_t *original_file =
	    share_lookup_file_by_inode(share, te->active_inode);
	if(original_file)
	{
	    /* original shared, keep as duplicate */
	    file->mp->stats.nduplicates++;
	    file->mp->stats.dupsize += file->size;

	    /* there is no need to keep a duplicate around */
	    goto fail;
	}
	else
	{
	    /* original not shared, claim this TTH */
	    tth_store_set_active_inode(global_tth_store,
		notification->tth, file->inode);
	}
    }

    /* Now insert the file in the hashed tree. Do a safety check first. */
    if(share_lookup_file(share, local_path))
        WARNING("File [%s] already in hashed tree!?", local_path);
    else
    {
        RB_INSERT(file_tree, &share->files, file);
    }

    free(local_path);

    /* update mountpoint statistics */
    file->mp->stats.size += file->size;
    file->mp->stats.nfiles++;

    /* add the file to the bloom filter */
    char *filename = strrchr(file->partial_path, '/');
    if(filename++ == NULL)
	filename = file->partial_path;
    bloom_add_filename(share->bloom, filename);

    return;

fail:
    free(local_path);
    share_remove_from_inode_table(share, file);
    share_file_free(file);
}

void share_tth_init_notifications(share_t *share)
{
    nc_add_tth_available_observer(nc_default(),
            handle_tth_available_notification,
            share);
}

