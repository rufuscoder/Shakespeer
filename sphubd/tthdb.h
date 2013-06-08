/*
 * Copyright (c) 2007 Martin Hedenfalk <martin@bzero.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _tthdb_h_
#define _tthdb_h_

#include "sys_tree.h"

#include <stdio.h>
#include <stdbool.h>

typedef struct tth_entry tth_entry_t;
struct tth_entry
{
	RB_ENTRY(tth_entry) link;

	uint64_t active_inode;
	char tth[40];

	off_t leafdata_offset;
	unsigned leafdata_len;
	char *leafdata;
};

struct tth_inode
{
	RB_ENTRY(tth_inode) link;

	uint64_t inode;
	time_t mtime;
	char tth[40];
};

struct tth_store
{
	char *filename;
	FILE *fp;
	bool loading;
	bool need_normalize;
	unsigned line_number;
	RB_HEAD(tth_entries_head, tth_entry) entries;
	RB_HEAD(tth_inodes_head, tth_inode) inodes;
};

RB_PROTOTYPE(tth_entries_head, tth_entry, link, tth_entry_cmp);
RB_PROTOTYPE(tth_inodes_head, tth_inode, link, tth_inode_cmp);

void tth_store_init(void);
void tth_store_close(void);
int tth_store_load_leafdata(struct tth_store *store, struct tth_entry *entry);

void tth_store_add_entry(struct tth_store *store,
	const char *tth, const char *leafdata_base64,
	off_t leafdata_offset);
void tth_store_add_inode(struct tth_store *store,
	uint64_t inode, time_t mtime, const char *tth);

struct tth_entry *tth_store_lookup(struct tth_store *store, const char *tth);
void tth_store_remove(struct tth_store *store, const char *tth);

struct tth_entry *tth_store_lookup_by_inode(struct tth_store *store, uint64_t inode);
void tth_store_remove_inode(struct tth_store *store, uint64_t inode);
struct tth_inode *tth_store_lookup_inode(struct tth_store *store, uint64_t inode);

void tth_store_set_active_inode(struct tth_store *store, const char *tth, uint64_t inode);

#endif

