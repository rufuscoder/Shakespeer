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

#ifndef _dbenv_h_
#define _dbenv_h_

#include <db.h>

void close_default_db_environment(void);
int verify_db(const char *dbfile, const char *db_list[]);
void backup_db(const char *dbfile);
int open_database(DB **db, const char *dbfile, const char *dbname,
        int type, int flags);
int close_db(DB **db, const char *dbname);
int db_transaction(DB_TXN **txn);

void db_checkpoint(void);
void db_prune_logfiles(void);
void db_schedule_maintenance(void);

#endif

