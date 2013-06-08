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
#include <sys/stat.h>
#include <sys/time.h>
#include <event.h>
#include <string.h>

#include "share.h"
#include "bloom.h"
#include "log.h"
#include "notifications.h"

static void share_create_bloom(share_t *share, unsigned length)
{
    DEBUG("(re)creating bloom filter");

    return_if_fail(share);

    bloom_free(share->bloom);
    share->bloom = bloom_create(length);

    share_file_t *f;
    RB_FOREACH(f, file_tree, &share->files)
    {
	char *filename = strrchr(f->partial_path, '/');
	if(filename++ == NULL)
	    filename = f->partial_path;
        bloom_add_filename(share->bloom, filename);
    }

    INFO("bloom filter is %.1f%% filled", bloom_filled_percent(share->bloom));
}

static void share_bloom_handle_did_remove_share_notification(
        nc_t *nc,
        const char *channel,
        nc_did_remove_share_t *notification,
        void *user_data)
{
	return_if_fail(user_data);

	/* Need to re-create the bloom filter after a share has been removed */
	DEBUG("re-creating bloom filter after share removal");
	share_t *share = user_data;
	return_if_fail(share->bloom);
	share_create_bloom(share, share->bloom->length);
}

static void share_bloom_handle_scan_finished(
        nc_t *nc,
        const char *channel,
        nc_share_scan_finished_t *notification,
        void *user_data)
{
	return_if_fail(user_data);

	/* Check that the created bloom filter is not over-filled. */
	share_t *share = user_data;

	return_if_fail(share->bloom);
	float fill = bloom_filled_percent(share->bloom);

	if(fill > 70.0)
	{
		INFO("bloom filter is %.1f%% filled, increasing filter length", fill);
		share_create_bloom(share, share->bloom->length * 2);
	}
}

void share_bloom_init(share_t *share)
{
	nc_add_did_remove_share_observer(nc_default(),
		share_bloom_handle_did_remove_share_notification, share);
	nc_add_share_scan_finished_observer(nc_default(),
		share_bloom_handle_scan_finished, share);
}

