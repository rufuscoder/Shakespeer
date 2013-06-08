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

#ifndef _spclient_h_
#define _spclient_h_

#include "filelist.h"
#include "spclient_cmd.h"
#include "spclient_send.h"

sp_t *sp_create(void *user_data);
void sp_free(sp_t *sp);
int sp_connect_remote(sp_t *sp, const char *remote_address);
int sp_connect(sp_t *sp, const char *working_directory, const char *executable_path);
int sp_disconnect(sp_t *sp);

int sp_send_string(sp_t *sp, const char *string);
int sp_send_command(sp_t *sp, const char *fmt, ...);

int sp_in_event(int fd, short why, void *data);

#endif

