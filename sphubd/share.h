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

#ifndef _share_h_
#define _share_h_

#include <stdbool.h>

#include "sys_queue.h"
#include "sys_tree.h"

#include "bloom.h"
#include "args.h"
#include "tiger.h"
#include "util.h"
#include "tthdb.h"

#define SHARE_INODE_BUCKETS 509

typedef struct share_mountpoint share_mountpoint_t;

typedef struct share_search share_search_t;
struct share_search
{
    share_size_restriction_t size_restriction;
    uint64_t size;
    share_type_t type;
    arg_t *words;
    char *tth;
    unsigned matches;

    bool passive;
    int port;
    char *host;
    char *nick;
};

typedef struct share_file share_file_t;
struct share_file
{
    RB_ENTRY(share_file) entry;
    LIST_ENTRY(share_file) inode_link;
    SLIST_ENTRY(share_file) link; /* used by sphashd_client.c */

    share_mountpoint_t *mp;
    char *partial_path; /* sub-path within the mountpoint */
    share_type_t type;
    uint64_t size;
    uint64_t inode;
};

typedef struct file_tree file_tree_t;
RB_HEAD(file_tree, share_file);

typedef struct share share_t;
struct share
{
    LIST_HEAD(, share_mountpoint) mountpoints;
    bool uptodate;     /* if false, filelist must be re-saved */
    int scanning;      /* increased for each each share currently scanning */
    bloom_t *bloom;
    char *cid;
    unsigned listlen; /* length of the uncompressed MyList file */

    file_tree_t files;
    file_tree_t unhashed_files;
    LIST_HEAD(, share_file) inodes[SHARE_INODE_BUCKETS];
};

RB_PROTOTYPE(file_tree, share_file, entry, share_file_cmp);

typedef struct share_stats share_stats_t;
struct share_stats
{
    uint64_t size; /* hashed size */
    uint64_t totsize;
    uint64_t dupsize;
    unsigned nfiles; /* number of hashed, unique files */
    unsigned ntotfiles;
    unsigned nduplicates;
};

struct share_mountpoint
{
    LIST_ENTRY(share_mountpoint) link;

    char *local_root;    /* /mnt/media/music */
    char *virtual_root;  /* music */

    share_stats_t stats;
    bool scan_in_progress;
    bool removed; /* set to true if removed, so a scanner can abort */
};

typedef SLIST_HEAD(share_file_list, share_file) share_file_list_t;

share_t *share_new(void);

int share_file_cmp(share_file_t *a, share_file_t *b);
share_file_t *share_lookup_file(share_t *share, const char *local_path);
share_file_t *share_lookup_unhashed_file(share_t *share, const char *local_path);
share_mountpoint_t *share_lookup_mountpoint(share_t *share,
        const char *virtual_root);
share_mountpoint_t *share_lookup_local_root(share_t *share,
        const char *local_root);
char *share_get_cid(share_t *share);
char *share_translate_path(share_t *share, const char *virtual_path);
char *share_translate_tth(share_t *share, const char *tth);

void share_add_to_inode_table(share_t *share, share_file_t *file);
void share_remove_from_inode_table(share_t *share, share_file_t *file);
share_file_t *share_lookup_file_by_inode(share_t *share, uint64_t inode);
share_file_list_t *share_next_unhashed(share_t *share, unsigned int limit);
void share_file_free(share_file_t *file);
int share_remove(share_t *share, const char *local_root, bool is_rescan);
share_mountpoint_t *share_add_mountpoint(share_t *share,
        const char *local_root);
void share_remove_mountpoint(share_t *share, share_mountpoint_t *mp);
char *share_local_to_virtual_path(share_t *share, share_file_t *file);
char *share_complete_path(share_file_t *file);

int share_scan(share_t *share, share_mountpoint_t *mp);

typedef int (*search_match_func_t)(const share_search_t *search,
        share_file_t *file, const char *tth, void *data);



void share_get_stats(share_t *share, share_stats_t *stats);

int share_add(share_t *share, const char *path);
share_file_t *share_file_dup(share_file_t *file);
void share_rescan(share_t *share);

/* in share_bloom.c */
void share_bloom_init(share_t *share);

/* in share_search.c */
int share_search(share_t *share, const share_search_t *search,
        search_match_func_t func, void *user_data);
share_search_t *share_search_parse_nmdc(const char *search_string,
        const char *encoding);
void share_search_free(share_search_t *s);

/* in share_save.c */
int share_save(share_t *share, unsigned int type);

/* in share_tth.c */
void share_tth_init_notifications(share_t *share);


struct tthdb_data *share_get_leafdata(struct share *share, const char *virtual_path);

#endif

