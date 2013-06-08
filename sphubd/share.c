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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "xstr.h"
#include "log.h"
#include "base32.h"
#include "nfkc.h"
#include "notifications.h"
#include "share.h"
#include "ui.h"
#include "globals.h"

RB_GENERATE(file_tree, share_file, entry, share_file_cmp);

share_t *share_new(void)
{
    DEBUG("initializing share");

    share_t *share = calloc(1, sizeof(share_t));

    RB_INIT(&share->files);
    RB_INIT(&share->unhashed_files);
    LIST_INIT(&share->mountpoints);

    int i;
    for(i = 0; i < SHARE_INODE_BUCKETS; i++)
    {
        LIST_INIT(&share->inodes[i]);
    }

    share->cid = share_get_cid(share);
    share_bloom_init(share);

    return share;
}

int share_add(share_t *share, const char *path)
{
    return_val_if_fail(share, -1);
    return_val_if_fail(path, -1);
    return_val_if_fail(path[0] == '/', -1);

    share_mountpoint_t *mp = share_lookup_local_root(share, path);

    if(mp)
    {
        if(mp->scan_in_progress)
        {
            INFO("Already scanning [%s], won't re-scan", path);
            return 0;
        }

        INFO("path already shared, will re-scan: %s", path);
        share_remove(share, path, true);
    }

    struct stat stbuf;
    if(stat(path, &stbuf) != 0 || !S_ISDIR(stbuf.st_mode))
    {
        WARNING("unavailable share: [%s]", path);
        return -1;
    }

    mp = share_add_mountpoint(share, path);
    return_val_if_fail(mp, -1);

    if(share->bloom == NULL)
    {
        share->bloom = bloom_create(32768);
    }

    ui_send_status_message(NULL, NULL, "Scanning %s...", path);

    return share_scan(share, mp);
}

void share_rescan(share_t *share)
{
    return_if_fail(share);

    /* share_add() below will find that the mountpoint is already shared,
     * remove the mountpoint (freeing it) and then re-adding it. That's the
     * reason for not using LIST_FOREACH and the strdup of local_root. */

    share_mountpoint_t *mp, *next;
    for(mp = LIST_FIRST(&share->mountpoints); mp; mp = next)
    {
        next = LIST_NEXT(mp, link);
        char *local_root = strdup(mp->local_root);
        share_add(share, local_root);
        free(local_root);
    }
}

/* is_rescan should be true if the share is being removed due to a rescan.
 * Otherwise the notification handler will send an updated MyINFO command to
 * the hub, possibly causing a low share kick.
 */
int share_remove(share_t *share, const char *local_root, bool is_rescan)
{
    share_mountpoint_t *mp;

    mp = share_lookup_local_root(share, local_root);
    if(mp == NULL)
    {
        WARNING("no such share: %s", local_root);
        return -1;
    }

    DEBUG("removing share [%s]", local_root);

    nc_send_will_remove_share_notification(nc_default(), local_root);

    share_file_t *f;
    share_file_t *next;
    for(f = RB_MIN(file_tree, &share->files); f; f = next)
    {
        next = RB_NEXT(file_tree, &share->files, f);

	if(f->mp == mp)
        {
            RB_REMOVE(file_tree, &share->files, f);
            share_remove_from_inode_table(share, f);
            share_file_free(f);
        }
    }

    for(f = RB_MIN(file_tree, &share->unhashed_files); f; f = next)
    {
        next = RB_NEXT(file_tree, &share->unhashed_files, f);

	if(f->mp == mp)
        {
            RB_REMOVE(file_tree, &share->unhashed_files, f);
            share_remove_from_inode_table(share, f);
            share_file_free(f);
        }
    }

    share->uptodate = false;

    if(mp->scan_in_progress)
    {
	WARNING("removing mountpoint currently scanning, delaying removal");
	mp->removed = true;
    }
    else
    {
	share_remove_mountpoint(share, mp);
    }

    nc_send_did_remove_share_notification(nc_default(), local_root, is_rescan);

    return 0;
}

/* A virtual root is the root directory as named in the filelist. It is
 * mapped to a mount point, which is an absolute path in the local filesystem.
 *
 * Example: The mount point "/mnt/media/music" is mapped to the virtual root
 * "music".
 */
share_mountpoint_t *share_add_mountpoint(share_t *share, const char *local_root)
{
    return_val_if_fail(share, NULL);
    return_val_if_fail(local_root, NULL);

    share_mountpoint_t *mp = share_lookup_local_root(share, local_root);
    if (mp && strcmp(mp->local_root, local_root) == 0)
    {
        /* Mountpoint already exists, return it */
        return mp;
    }

    /* Check for path containing already shared path.
     * eg: /tmp/foo is already shared and trying to add /tmp
     *   or
     *     /tmp is already shared and trying to add /tmp/foo
     */
    size_t len = strlen(local_root);
    LIST_FOREACH(mp, &share->mountpoints, link) {
        if (str_has_prefix(mp->local_root, local_root) && mp->local_root[len] == '/') {
            WARNING("Subdirectory [%s] already shared (trying to add [%s])", mp->local_root, local_root);
            return NULL;
        }

        if (str_has_prefix(local_root, mp->local_root) && local_root[strlen(mp->local_root)] == '/') {
            WARNING("Parent directory [%s] already shared (trying to add [%s])", mp->local_root, local_root);
            return NULL;
        }
    }

    DEBUG("Adding mountpoint [%s]", local_root);

    mp = calloc(1, sizeof(share_mountpoint_t));
    mp->local_root = strdup(local_root);

    /* Add a new mountpoint */

    const char *basename = strrchr(local_root, '/');
    if (basename++ == 0)
        basename = local_root;

    mp->virtual_root = strdup(basename);

    /* the virtual root can not contain $ or | characters, as they have
     * special meanings in the DC protocol */
    str_replace_set(mp->virtual_root, "$|", '_');

    int n;
    for (n = 2; n < 100; n++) {
        /* check if this virtual root is already used */
        share_mountpoint_t *old_mp = share_lookup_mountpoint(share, mp->virtual_root);

        if (old_mp == NULL)
            /* virtual root is unused */
            break;

        free(mp->virtual_root);
        int num_returned_bytes = asprintf(&mp->virtual_root, "%s-%i", basename, n);
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
    }


    /* insert it into the list */
    LIST_INSERT_HEAD(&share->mountpoints, mp, link);

    return mp;
}

void share_free_mountpoint(share_mountpoint_t *mp)
{
    if(mp)
    {
        free(mp->local_root);
        free(mp->virtual_root);
        free(mp);
    }
}

void share_remove_mountpoint(share_t *share, share_mountpoint_t *mp)
{
    return_if_fail(share);
    return_if_fail(mp);

    LIST_REMOVE(mp, link);
    share_free_mountpoint(mp);
}

share_mountpoint_t *share_lookup_local_root(share_t *share,
        const char *local_root)
{
    share_mountpoint_t *mp;
    LIST_FOREACH(mp, &share->mountpoints, link)
    {
        /* if(strcmp(local_root, mp->local_root) == 0) */
        size_t len = strlen(mp->local_root);
        if(strncmp(local_root, mp->local_root, len) == 0 &&
                (local_root[len] == 0 || local_root[len] == '/'))
            return mp;
    }

    return NULL;
}

share_mountpoint_t *share_lookup_mountpoint(share_t *share,
        const char *virtual_root)
{
    share_mountpoint_t *mp;
    LIST_FOREACH(mp, &share->mountpoints, link)
    {
        if(strcmp(virtual_root, mp->virtual_root) == 0)
            return mp;
    }

    return NULL;
}

/*
 * /folder
 * /folder/prout
 * /folder 1
 *
 * /folder < /folder/prout
 *
 * /folder/prout < /folder 1
 * 
 */

static int share_path_cmp(const char *a, const char *b)
{
    const char *slash_a = a, *slash_b = b;
    int rc = 0;

    int a_total_len = strlen(a);
    int b_total_len = strlen(b);

    if(a_total_len == b_total_len &&
            strncmp(a, b, a_total_len) == 0)
    {
        return 0;
    }

    int alen = 0;
    int blen = 0;
    int minlen;

    do
    {
        const char *pa = slash_a + 1;
        const char *pb = slash_b + 1;

        slash_a = strchr(pa, '/');
        slash_b = strchr(pb, '/');

        if(slash_a == NULL)
        {
            if(slash_b)
            {
                return -1;
            }
            /* slash_a = a + a_total_len; */
        }

        if(slash_b == NULL)
        {
            if(slash_a)
            {
                return 1;
            }
            /* slash_b = b + b_total_len; */
        }

        if(slash_a == NULL)
        {
            rc = strcmp(a, b);
            break;
        }

        alen = slash_a - a;
        blen = slash_b - b;

        minlen = alen;
        if(blen < minlen)
            minlen = blen;

        rc = strncmp(a, b, minlen);

        if(rc == 0 && alen != blen)
        {
            rc = alen - blen;
        }
    } while(rc == 0);

    return rc < 0 ? -1 : 1;
}

/* sort function used by the red-black tree */
int share_file_cmp(share_file_t *a, share_file_t *b)
{
    if(a->mp < b->mp)
	return -1;
    if(a->mp > b->mp)
	return 1;

    return share_path_cmp(a->partial_path, b->partial_path);
}

unsigned share_inode_hash(uint64_t inode)
{
    return (unsigned)inode % SHARE_INODE_BUCKETS;
}

void share_add_to_inode_table(share_t *share, share_file_t *file)
{
    LIST_INSERT_HEAD(&share->inodes[share_inode_hash(file->inode)],
            file, inode_link);
}

void share_remove_from_inode_table(share_t *share, share_file_t *file)
{
    LIST_REMOVE(file, inode_link);
}

share_file_t *share_lookup_file(share_t *share, const char *local_path)
{
    share_file_t find;
    find.mp = share_lookup_local_root(share, local_path);
    if(find.mp == NULL)
	return NULL;
    find.partial_path = (char *)local_path + strlen(find.mp->local_root);
    return RB_FIND(file_tree, &share->files, &find);
}

share_file_t *share_lookup_unhashed_file(share_t *share, const char *local_path)
{
    share_file_t find;
    find.mp = share_lookup_local_root(share, local_path);
    if(find.mp == NULL)
	return NULL;
    find.partial_path = (char *)local_path + strlen(find.mp->local_root);
    return RB_FIND(file_tree, &share->unhashed_files, &find);
}

share_file_t *share_lookup_file_by_inode(share_t *share, uint64_t inode)
{
    share_file_t *f;
    LIST_FOREACH(f, &share->inodes[share_inode_hash(inode)], inode_link)
    {
        if(f->inode == inode)
            return f;
    }

    return NULL;
}

char *share_get_cid(share_t *share)
{
    uint64_t u1 = random();
    uint64_t u2 = random();
    uint64_t cid = (u1 << 32) | u2;
    char *cid_string = base32_encode((char *)&cid, sizeof(uint64_t));

    return cid_string;
}

/***** utility functions *****/

/* Converts virtual_root\directory\file.ext to /local/root/directory/file.ext
 * Path must already be in UTF-8 encoding. Returned path is in decomposed
 * format.
 */
char *share_translate_path(share_t *share, const char *virtual_path)
{
    /* First look up the mountpoint from the virtual root */
    const char *slash = strchr(virtual_path, '\\');
    if (slash == NULL)
        slash = virtual_path + strlen(virtual_path);
    char *virtual_root = xstrndup(virtual_path, slash - virtual_path);

    share_mountpoint_t *mp = share_lookup_mountpoint(share, virtual_root);
    if (mp == 0) {
        DEBUG("Warning, can't find mount point for virtual root '%s'", virtual_root);
        free(virtual_root);
        return NULL;
    }
    free(virtual_root);

    /* replace virtual_root with local_root and change "\" to "/" */

    char *npath;
    if (*slash && *(slash + 1)) {
        int num_returned_bytes = asprintf(&npath, "%s/%s", mp->local_root, slash + 1);
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
    }
    else
        npath = xstrdup(mp->local_root);

    str_replace_set(npath, "\\", '/');

    return npath;
}

char *share_local_to_virtual_path(share_t *share, share_file_t *file)
{
    return_val_if_fail(share, NULL);
    return_val_if_fail(file, NULL);
    return_val_if_fail(file->mp, NULL);

    char *virtual_path;
    int num_returned_bytes = asprintf(&virtual_path, "%s%s", file->mp->virtual_root, file->partial_path);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
    str_replace_set(virtual_path, "/", '\\');
    
    return virtual_path;
}

share_file_t *share_file_dup(share_file_t *file)
{
    return_val_if_fail(file, NULL);
    return_val_if_fail(file->partial_path, NULL);

    share_file_t *dup = calloc(1, sizeof(share_file_t));
    dup->partial_path = strdup(file->partial_path);
    dup->mp = file->mp;
    dup->type = file->type;
    dup->size = file->size;
    dup->inode = file->inode;
    return dup;
}

share_file_list_t *share_next_unhashed(share_t *share, unsigned limit)
{
    return_val_if_fail(share, NULL);
    return_val_if_fail(limit > 0, NULL);

    DEBUG("getting batch of unhashed files...");

    int olimit = limit;
    share_file_list_t *unfinished = NULL;

    share_file_t *f;
    RB_FOREACH(f, file_tree, &share->unhashed_files)
    {
	if(unfinished == NULL)
	{
	    unfinished = malloc(sizeof(share_file_list_t));
	    SLIST_INIT(unfinished);
	}

	SLIST_INSERT_HEAD(unfinished, f, link);
	if(--limit == 0)
	    break;
    }

    DEBUG("Returning %i files", olimit - limit);
    return unfinished;
}

/* returns the complete local path, should be free'd by caller */
char *share_complete_path(share_file_t *file)
{
    return_val_if_fail(file, NULL);
    return_val_if_fail(file->mp, NULL);

    /* construct the complete local path */
    char *local_path;
    int num_returned_bytes = asprintf(&local_path, "%s%s", file->mp->local_root, file->partial_path);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");

    return local_path;
}

/* returned string should be freed by caller */
char *share_translate_tth(share_t *share, const char *tth)
{
    struct tth_entry *te = tth_store_lookup(global_tth_store, tth);
    
    if(te == NULL)
        return NULL;

    share_file_t *f = share_lookup_file_by_inode(share, te->active_inode);
    if(f == NULL)
        return NULL;

    return share_complete_path(f);
}

void share_file_free(share_file_t *file)
{
    if(file)
    {
        free(file->partial_path);
        free(file);
    }
}

/* Get total statistics from all mounts. */
void share_get_stats(share_t *share, share_stats_t *stats)
{
    return_if_fail(share);
    return_if_fail(stats);

    memset(stats, 0, sizeof(share_stats_t));

    share_mountpoint_t *mp;
    LIST_FOREACH(mp, &share->mountpoints, link)
    {
        stats->size += mp->stats.size;
        stats->totsize += mp->stats.totsize;
        stats->dupsize += mp->stats.dupsize;
        stats->nfiles += mp->stats.nfiles;
        stats->ntotfiles += mp->stats.ntotfiles;
        stats->nduplicates += mp->stats.nduplicates;
    }
}

#ifdef TEST
#include "unit_test.h"

int ui_send_status_message(ui_t *ui, const char *hub_address, const char *message, ...)
{
    return 0;
}

int main(void)
{
    share_t *share = share_new();
    sp_log_set_level("debug");
    fail_unless(share);

    share_mountpoint_t *mp = share_add_mountpoint(share, "/local/root");
    fail_unless(mp);
    fail_unless(mp->local_root && mp->virtual_root);
    fail_unless(strcmp(mp->local_root, "/local/root") == 0);
    fail_unless(strcmp(mp->virtual_root, "root") == 0);

    /* Can't add the parent directory to an already shared subdirectory */
    fail_unless(share_add_mountpoint(share, "/local") == NULL);

    /* Can't add a subdirectory to an already shared parent directory */
    fail_unless(share_add_mountpoint(share, "/local/root/subdir") == NULL);

    /* But this should work: */
    mp = share_add_mountpoint(share, "/loca");
    fail_unless(mp);
    mp = share_add_mountpoint(share, "/local/root2");
    fail_unless(mp);

    /* /a
     * /a2
     * /a/b
     * /a/b2
     * /a/b/c
     * /a/c
     * /a/c2
     *
     */

    fail_unless(share_path_cmp("/a", "/a2") == -1);
    fail_unless(share_path_cmp("/a2", "/a/b") == -1);
    fail_unless(share_path_cmp("/a/b", "/a/b2") == -1);
    fail_unless(share_path_cmp("/a/b2", "/a/b/c") == -1);
    fail_unless(share_path_cmp("/a/b2", "/a/c") == -1);
    fail_unless(share_path_cmp("/a/c", "/a/c2") == -1);

    fail_unless(share_path_cmp("/a", "/a/b/c") == -1);
    fail_unless(share_path_cmp("/a", "/a2/b/c") == -1);

    fail_unless(share_path_cmp("/a", "/a") == 0);
    fail_unless(share_path_cmp("/a/b", "/a/b") == 0);
    fail_unless(share_path_cmp("/a/b/c", "/a/b/c") == 0);

    fail_unless(share_path_cmp("/a2", "/a") == 1);
    fail_unless(share_path_cmp("/a/b", "/a2") == 1);
    fail_unless(share_path_cmp("/a/b/c", "/a/b") == 1);
    fail_unless(share_path_cmp("/a/b/c", "/a/b2") == 1);
    fail_unless(share_path_cmp("/a/c", "/a/b2") == 1);
    fail_unless(share_path_cmp("/a/c2", "/a/c") == 1);

    fail_unless(share_path_cmp("/a", "/a/filen") == -1);
    fail_unless(share_path_cmp("/a/filen", "/a") == 1);

    fail_unless(share_path_cmp("/folder", "/folder/prout") == -1);
    fail_unless(share_path_cmp("/folder/prout", "/folder") == 1);

    fail_unless(share_path_cmp("/folder/prout", "/folder 1") == 1);
    fail_unless(share_path_cmp("/folder 1", "/folder/prout") == -1);

    return 0;
}

#endif

