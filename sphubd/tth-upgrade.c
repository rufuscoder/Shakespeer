/*
 * Copyright (c) 2006 Martin Hedenfalk <martin@bzero.se>
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

#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <db.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "globals.h"
#include "tthdb.h"
#include "log.h"
#include "xstr.h"
#include "dbenv.h"
#include "util.h"

extern DB *tth_db;
extern DB *tth_inode_db;

int main(int argc, char **argv)
{
    if(argc > 1)
        global_working_directory = argv[1];
    else
        global_working_directory = get_working_directory();

    printf("\nThis may take a while. Please be patient.\n\n");
    sleep(3);

    sp_log_set_level("debug");

    tthdb_open();
    if(tth_db == NULL)
        return 2;

    db_checkpoint();

    g_message("Upgrading TTH database...");
    bool modified;

    do
    {
	modified = false;

	/* create a cursor */
	DBC *cursor;
	tth_inode_db->cursor(tth_inode_db, NULL, &cursor, 0);

	int rc = -1;
	while(cursor)
	{
	    DBT key;
	    DBT val;

	    memset(&key, 0, sizeof(DBT));
	    memset(&val, 0, sizeof(DBT));

	    rc = cursor->c_get(cursor, &key, &val, DB_NEXT);
	    if(rc != 0)
		break;

	    uint64_t inode = *(uint64_t *)key.data;
	    tth_inode_t *ti = val.data;

	    struct tthdb_data *d = tthdb_lookup(ti->tth);
	    if(d == NULL || d->inode != inode)
	    {
		printf("No TTH found for inode %llu -- removing inode\n", inode);
		tthdb_remove_inode(inode);
		modified = true;
		continue;
	    }

	    uint64_t new_inode = (uint64_t)(((uint64_t)d->size << 32) | (inode & 0xFFFFFFFF));
	    if(d->inode != new_inode)
	    {
		printf("%s: inode %llu -> %llu\n", ti->tth, d->inode, new_inode);
		d->inode = new_inode;
		tthdb_update(ti->tth, d);
		modified = true;
	    }
	}

	if(rc == -1)
	{
	    printf("Failed: %s\n", strerror(errno));
	}

	if(cursor)
	{
	    cursor->c_close(cursor);
	}
    } while(modified);

    db_checkpoint();

    tthdb_close();
    close_default_db_environment();

    return 0;
}

